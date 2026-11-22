#include "UIRenderSystem.h"
#include "UILayout.h"
#include "LocalizationManager.h"
#include "RenderPass.h"
#include "Renderer.h"
#include "ResourceManager.h"
#include "Texture.h"
#include "UIEventSystem.h"
#include "UITextInputComponent.h"
#include "UIDropdownComponent.h"
#include "UIMultilineTextLayout.h"
#include "ComponentEnable.h"

#include <glm/gtc/matrix_transform.hpp>
#include <atomic>
#include <cstring>
#include <memory>

#if defined(_WIN32)
namespace
{
    Engine::ResourceHandle<Engine::Texture> CreateTextTexture(Engine::RHI& rhi, const std::string& text,
        const std::string& fontFamily, int fontPixels)
    {
        const auto byteCount = static_cast<int>(text.size());
        const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), byteCount, nullptr, 0);
        if (length <= 0) return {};
        std::wstring line(length, L'\0');
        if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), byteCount, line.data(), length) != length)
            return {};

        wchar_t family[LF_FACESIZE]{};
        const bool validFamily = !fontFamily.empty() &&
            MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, fontFamily.c_str(), -1, family, LF_FACESIZE) > 0;
        const auto* face = validFamily ? family : L"Malgun Gothic";

        // Destroy the DC before the selected font and bitmap.
        const auto deleteObject = [](auto* object) { DeleteObject(object); };
        std::unique_ptr<std::remove_pointer_t<HFONT>, decltype(deleteObject)> font(
            CreateFontW(-fontPixels, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, face), deleteObject);
        std::unique_ptr<std::remove_pointer_t<HBITMAP>, decltype(deleteObject)> bitmap(nullptr, deleteObject);
        std::unique_ptr<std::remove_pointer_t<HDC>, decltype(&DeleteDC)> dc(CreateCompatibleDC(nullptr), DeleteDC);
        if (!font || !dc || !SelectObject(dc.get(), font.get())) return {};

        SIZE extent{};
        if (!GetTextExtentPoint32W(dc.get(), line.data(), length, &extent) || extent.cx < 1 || extent.cy < 1 ||
            extent.cx > 4092 || extent.cy > 508) return {};

        constexpr int padding = 2;
        const int width = extent.cx + 2 * padding;
        const int height = extent.cy + 2 * padding;
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(info.bmiHeader);
        info.bmiHeader.biWidth = width;
        info.bmiHeader.biHeight = -height;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        void* pixels = nullptr;
        bitmap.reset(CreateDIBSection(dc.get(), &info, DIB_RGB_COLORS, &pixels, nullptr, 0));
        if (!bitmap || !pixels || !SelectObject(dc.get(), bitmap.get())) return {};

        constexpr std::size_t channels = 4;
        const auto size = static_cast<std::size_t>(width) * height * channels;
        std::memset(pixels, 0, size);
        SetBkMode(dc.get(), TRANSPARENT);
        SetTextColor(dc.get(), RGB(255, 255, 255));
        if (!TextOutW(dc.get(), padding, padding, line.data(), length) || !GdiFlush()) return {};

        std::vector<Engine::uint8> rgba(size, 255);
        const auto* source = static_cast<const Engine::uint8*>(pixels);
        for (std::size_t offset = 0; offset < size; offset += channels)
            rgba[offset + 3] = std::max({ source[offset], source[offset + 1], source[offset + 2] });

        RHITextureDesc desc{};
        desc.width = width;
        desc.height = height;
        desc.depth = 1;
        desc.mipLevels = 1;
        desc.arraySize = 1;
        desc.format = Engine::ETextureFormat::R8G8B8A8_UNORM;
        desc.dimension = Engine::ETextureDimension::Texture2D;
        desc.usage = Engine::ETextureUsage::Sampler;
        desc.data = rgba.data();
        desc.dataSize = static_cast<Engine::uint32>(rgba.size());
        auto* uploaded = rhi.CreateTexture(desc);
        if (!uploaded) return {};

        static std::atomic<Engine::uint64> nextId = 0;
        const auto key = L"UIRuntimeText/" + std::to_wstring(nextId.fetch_add(1, std::memory_order_relaxed));
        auto& resources = Engine::ResourceManager::Get();
        auto texture = resources.AddResource(key, Engine::Texture::CreateFromRHITexture(uploaded));
        // Ownership is limited to the cache and in-flight draw commands.
        if (texture.IsValid()) resources.ReleaseResource(texture.GetRawHandle());
        return texture;
    }

    struct FontContext
    {
        std::unique_ptr<std::remove_pointer_t<HFONT>, void(*)(HGDIOBJ)> font{ nullptr, [](HGDIOBJ obj) { if (obj) DeleteObject(obj); } };
        std::unique_ptr<std::remove_pointer_t<HDC>, decltype(&DeleteDC)> dc{ nullptr, DeleteDC };

        static FontContext Create(const std::string& fontFamily, int fontPixels)
        {
            FontContext ctx;
            wchar_t family[LF_FACESIZE]{};
            const bool validFamily = !fontFamily.empty() &&
                MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, fontFamily.c_str(), -1, family, LF_FACESIZE) > 0;
            const auto* face = validFamily ? family : L"Malgun Gothic";

            ctx.font.reset(CreateFontW(-fontPixels, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, face));
            ctx.dc.reset(CreateCompatibleDC(nullptr));
            if (ctx.font && ctx.dc)
            {
                SelectObject(ctx.dc.get(), ctx.font.get());
            }
            return ctx;
        }

        float Measure(std::string_view str, int fontPixels) const
        {
            if (str.empty()) return 0.0f;
            const auto byteCount = static_cast<int>(str.size());
            const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str.data(), byteCount, nullptr, 0);
            if (length <= 0) return static_cast<float>(str.size()) * (static_cast<float>(fontPixels) * 0.5f);
            std::wstring line(length, L'\0');
            if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str.data(), byteCount, line.data(), length) != length)
                return static_cast<float>(str.size()) * (static_cast<float>(fontPixels) * 0.5f);
            SIZE extent{};
            if (dc && GetTextExtentPoint32W(dc.get(), line.data(), length, &extent))
                return static_cast<float>(extent.cx);
            return static_cast<float>(str.size()) * (static_cast<float>(fontPixels) * 0.5f);
        }
    };
}
#endif

void Engine::UIRenderSystem::SubmitText(Renderer& renderer, RenderPass& pass, const entt::registry& world,
    const UIElementLayout& layout, const UITextComponent& text, const glm::vec2& canvasSize,
    const ResourceHandle<MaterialInterface>& material)
{
#if defined(_WIN32)
    const bool useTranslation = text.useLocalization && !text.localizationKey.empty() && LocalizationManager::IsCreated();
    const auto resolved = useTranslation ? LocalizationManager::Get().GetText(text.localizationKey, text.text) : text.text;
    if (layout.size.x <= 0.0f || layout.size.y <= 0.0f || resolved.empty() || resolved.size() > UITextComponent::MaxTextBytes) return;

    const bool isMultiline = text.multiline || text.wordWrap;
    if (!isMultiline)
    {
        if (resolved.find_first_of("\r\n") != std::string::npos) return;

        auto* rhi = renderer.GetRHI();
        const auto* mat = material.Get();
        if (!rhi || !mat || !pass.IsAcceptsBlendMode(mat->GetBlendMode())) return;

        auto& cached = m_TextCache[{ text.fontPixels, text.fontFamily, resolved }];
        if (!cached.submitted && !cached.texture.IsValid())
            cached.texture = CreateTextTexture(*rhi, resolved, text.fontFamily, text.fontPixels);
        cached.submitted = true;
        const auto* texture = cached.texture.Get();
        if (!texture) return;

        const auto& header = texture->GetCachedHeader();
        const glm::vec2 bitmapSize(header.width, header.height);
        const float fit = std::min({ 1.0f, layout.size.x / bitmapSize.x, layout.size.y / bitmapSize.y });
        const glm::vec2 size = bitmapSize * fit;
        const auto& rect = world.get<RectTransformComponent>(layout.entity);
        glm::vec2 offset = -layout.size * rect.pivot;
        const glm::vec2 available = layout.size - size;
        if (text.alignment == EUITextAlignment::Center) offset.x += available.x * 0.5f;
        if (text.alignment == EUITextAlignment::Right) offset.x += available.x;
        if (text.verticalAlignment == EUITextVerticalAlignment::Middle) offset.y += available.y * 0.5f;
        if (text.verticalAlignment == EUITextVerticalAlignment::Bottom) offset.y += available.y;

        UIImageDrawCommand command;
        command.textureHandle = cached.texture;
        command.materialHandle = material;
        command.worldMatrix = glm::scale(glm::translate(layout.worldTransform, glm::vec3(offset, 0.0f)), glm::vec3(size, 1.0f));
        command.color = text.color;
        command.canvasSize = canvasSize;
        command.clipRect = layout.clipRect;
        command.ellipseMaskPlanes = layout.ellipseMaskPlanes;
        command.ellipseMaskCount = layout.ellipseMaskCount;
        command.alphaMasks = layout.alphaMasks;
        command.alphaMaskCount = layout.alphaMaskCount;
        renderer.SubmitUIImage(command, pass.GetID());
    }
    else
    {
        auto* rhi = renderer.GetRHI();
        const auto* mat = material.Get();
        if (!rhi || !mat || !pass.IsAcceptsBlendMode(mat->GetBlendMode())) return;

        FontContext fontCtx = FontContext::Create(text.fontFamily, text.fontPixels);
        const float maxWidth = text.wordWrap ? layout.size.x : 0.0f;
        const float lineHeight = static_cast<float>(text.fontPixels) * 1.25f;

        const auto layoutResult = WrapMultilineText(resolved, maxWidth,
            [&](std::string_view s) { return fontCtx.Measure(s, text.fontPixels); },
            lineHeight, layout.size.y, text.alignment,
            static_cast<EUIVerticalAlignment>(text.verticalAlignment));

        const auto& rect = world.get<RectTransformComponent>(layout.entity);
        const glm::vec2 baseOffset = -layout.size * rect.pivot;

        for (const auto& line : layoutResult.lines)
        {
            if (line.text.empty()) continue;
            const std::string lineStr(line.text);
            auto& cached = m_TextCache[{ text.fontPixels, text.fontFamily, lineStr }];
            if (!cached.submitted && !cached.texture.IsValid())
                cached.texture = CreateTextTexture(*rhi, lineStr, text.fontFamily, text.fontPixels);
            cached.submitted = true;
            const auto* texture = cached.texture.Get();
            if (!texture) continue;

            const auto& header = texture->GetCachedHeader();
            const glm::vec2 bitmapSize(header.width, header.height);
            const glm::vec2 linePos = baseOffset + line.offset - glm::vec2(2.0f, 2.0f);

            UIImageDrawCommand command;
            command.textureHandle = cached.texture;
            command.materialHandle = material;
            command.worldMatrix = glm::scale(glm::translate(layout.worldTransform, glm::vec3(linePos, 0.0f)), glm::vec3(bitmapSize, 1.0f));
            command.color = text.color;
            command.canvasSize = canvasSize;
            command.clipRect = layout.clipRect;
            command.ellipseMaskPlanes = layout.ellipseMaskPlanes;
            command.ellipseMaskCount = layout.ellipseMaskCount;
            command.alphaMasks = layout.alphaMasks;
            command.alphaMaskCount = layout.alphaMaskCount;
            renderer.SubmitUIImage(command, pass.GetID());
        }
    }
#endif
}

void Engine::UIRenderSystem::SubmitTextInput(Renderer& renderer, RenderPass& pass, const entt::registry& world,
    const UIElementLayout& layout, const UITextInputComponent& input, const glm::vec2& canvasSize,
    const ResourceHandle<MaterialInterface>& material, const ResourceHandle<Texture>& white)
{
#if defined(_WIN32)
    if (layout.size.x <= 0.0f || layout.size.y <= 0.0f) return;
    const auto* mat = material.Get();
    const auto* whiteTex = white.Get();
    if (!mat || !whiteTex || !pass.IsAcceptsBlendMode(mat->GetBlendMode())) return;

    const auto tint = UIEventSystem::GetButtonTint(world, layout.entity);
    const auto drawRect = [&](glm::vec2 offset, glm::vec2 size, glm::vec4 color)
    {
        if (size.x <= 0.0f || size.y <= 0.0f || color.a <= 0.0f ||
            !glm::all(glm::greaterThanEqual(color, glm::vec4(0.0f))) ||
            !glm::all(glm::lessThanEqual(color, glm::vec4(1.0f)))) return;
        UIImageDrawCommand command;
        command.textureHandle = white;
        command.materialHandle = material;
        command.worldMatrix = glm::scale(glm::translate(layout.renderMatrix, glm::vec3(offset, 0.0f)), glm::vec3(size, 1.0f));
        command.color = color * tint;
        command.canvasSize = canvasSize;
        command.clipRect = layout.clipRect;
        command.ellipseMaskPlanes = layout.ellipseMaskPlanes;
        command.ellipseMaskCount = layout.ellipseMaskCount;
        command.alphaMasks = layout.alphaMasks;
        command.alphaMaskCount = layout.alphaMaskCount;
        renderer.SubmitUIImage(command, pass.GetID());
    };

    int fontPixels = 18;
    std::string fontFamily;
    if (const auto* textComp = TryGetEnabledComponent<UITextComponent>(world, layout.entity))
    {
        fontPixels = textComp->fontPixels;
        fontFamily = textComp->fontFamily;
    }

    FontContext fontCtx = FontContext::Create(fontFamily, fontPixels);
    const float padX = 6.0f;

    std::string displayText;
    glm::vec4 textColor = input.textColor;
    bool isPlaceholder = false;
    if (input.text.empty() && input.imeComposition.empty())
    {
        if (!input.placeholder.empty())
        {
            displayText = input.placeholder;
            textColor = input.placeholderColor;
            isPlaceholder = true;
        }
    }
    else
    {
        displayText = input.GetDisplayText();
    }

    // 1. Selection highlight quad
    if (input.isFocused && input.HasSelection())
    {
        const std::size_t selStart = input.SelectionStart();
        const std::size_t selEnd = input.SelectionEnd();
        const std::string_view preSel = std::string_view(input.text).substr(0, std::min(selStart, input.text.size()));
        const std::string_view selText = std::string_view(input.text).substr(selStart, std::min(selEnd, input.text.size()) - selStart);

        float selStartX = padX;
        float selW = 0.0f;
        if (input.isPassword)
        {
            selStartX += fontCtx.Measure(std::string(UIUtf8::CountCodepoints(preSel), '*'), fontPixels);
            selW = fontCtx.Measure(std::string(UIUtf8::CountCodepoints(selText), '*'), fontPixels);
        }
        else
        {
            selStartX += fontCtx.Measure(preSel, fontPixels);
            selW = fontCtx.Measure(selText, fontPixels);
        }

        const float selY = (layout.size.y - static_cast<float>(fontPixels)) * 0.5f;
        const float selH = static_cast<float>(fontPixels);
        drawRect({ selStartX / layout.size.x, selY / layout.size.y },
                 { selW / layout.size.x, selH / layout.size.y }, input.selectionColor);
    }

    // 2. Caret indicator and IME composition preview
    if (input.isFocused && !input.readOnly)
    {
        const std::size_t clampedCursor = std::min(input.cursorByteIndex, input.text.size());
        const std::string_view preCursor = std::string_view(input.text).substr(0, clampedCursor);
        float cursorX = padX;
        if (input.isPassword)
        {
            cursorX += fontCtx.Measure(std::string(UIUtf8::CountCodepoints(preCursor), '*'), fontPixels);
        }
        else
        {
            cursorX += fontCtx.Measure(preCursor, fontPixels);
        }

        if (!input.imeComposition.empty())
        {
            const float imeW = input.isPassword
                ? fontCtx.Measure(std::string(UIUtf8::CountCodepoints(input.imeComposition), '*'), fontPixels)
                : fontCtx.Measure(input.imeComposition, fontPixels);

            // IME underline
            const float underlineY = (layout.size.y + static_cast<float>(fontPixels)) * 0.5f;
            drawRect({ cursorX / layout.size.x, underlineY / layout.size.y },
                     { imeW / layout.size.x, 2.0f / layout.size.y }, input.caretColor);
            cursorX += imeW;
        }

        if (!input.HasSelection())
        {
            const float caretH = static_cast<float>(fontPixels) * 1.1f;
            const float caretY = (layout.size.y - caretH) * 0.5f;
            drawRect({ cursorX / layout.size.x, caretY / layout.size.y },
                     { 2.0f / layout.size.x, caretH / layout.size.y }, input.caretColor);
        }
    }

    // 3. Render display text
    if (!displayText.empty())
    {
        auto* rhi = renderer.GetRHI();
        if (rhi)
        {
            auto& cached = m_TextCache[{ fontPixels, fontFamily, displayText }];
            if (!cached.submitted && !cached.texture.IsValid())
                cached.texture = CreateTextTexture(*rhi, displayText, fontFamily, fontPixels);
            cached.submitted = true;
            if (const auto* texture = cached.texture.Get())
            {
                const auto& header = texture->GetCachedHeader();
                const glm::vec2 bitmapSize(header.width, header.height);
                const auto& rect = world.get<RectTransformComponent>(layout.entity);
                const glm::vec2 offset = -layout.size * rect.pivot + glm::vec2(padX - 2.0f, (layout.size.y - bitmapSize.y) * 0.5f);

                UIImageDrawCommand command;
                command.textureHandle = cached.texture;
                command.materialHandle = material;
                command.worldMatrix = glm::scale(glm::translate(layout.worldTransform, glm::vec3(offset, 0.0f)), glm::vec3(bitmapSize, 1.0f));
                command.color = textColor;
                command.canvasSize = canvasSize;
                command.clipRect = layout.clipRect;
                command.ellipseMaskPlanes = layout.ellipseMaskPlanes;
                command.ellipseMaskCount = layout.ellipseMaskCount;
                command.alphaMasks = layout.alphaMasks;
                command.alphaMaskCount = layout.alphaMaskCount;
                renderer.SubmitUIImage(command, pass.GetID());
            }
        }
    }
#endif
}

void Engine::UIRenderSystem::SubmitDropdown(Renderer& renderer, RenderPass& pass, const entt::registry& world,
    const UIElementLayout& layout, const UIDropdownComponent& dropdown, const glm::vec2& canvasSize,
    const ResourceHandle<MaterialInterface>& material, const ResourceHandle<Texture>& white)
{
#if defined(_WIN32)
    if (layout.size.x <= 0.0f || layout.size.y <= 0.0f) return;
    auto* rhi = renderer.GetRHI();
    const auto* mat = material.Get();
    if (!rhi || !mat || !pass.IsAcceptsBlendMode(mat->GetBlendMode())) return;

    int fontPixels = 18;
    std::string fontFamily;
    if (const auto* textComp = TryGetEnabledComponent<UITextComponent>(world, layout.entity))
    {
        fontPixels = textComp->fontPixels;
        fontFamily = textComp->fontFamily;
    }

    const std::string label = std::string(dropdown.GetSelectedLabel());
    const float padX = 8.0f;
    const auto& rect = world.get<RectTransformComponent>(layout.entity);

    // 1. Header label text
    if (!label.empty())
    {
        auto& cached = m_TextCache[{ fontPixels, fontFamily, label }];
        if (!cached.submitted && !cached.texture.IsValid())
            cached.texture = CreateTextTexture(*rhi, label, fontFamily, fontPixels);
        cached.submitted = true;
        if (const auto* texture = cached.texture.Get())
        {
            const auto& header = texture->GetCachedHeader();
            const glm::vec2 bitmapSize(header.width, header.height);
            const glm::vec2 offset = -layout.size * rect.pivot + glm::vec2(padX - 2.0f, (layout.size.y - bitmapSize.y) * 0.5f);

            UIImageDrawCommand command;
            command.textureHandle = cached.texture;
            command.materialHandle = material;
            command.worldMatrix = glm::scale(glm::translate(layout.worldTransform, glm::vec3(offset, 0.0f)), glm::vec3(bitmapSize, 1.0f));
            command.color = dropdown.textColor;
            command.canvasSize = canvasSize;
            command.clipRect = layout.clipRect;
            command.ellipseMaskPlanes = layout.ellipseMaskPlanes;
            command.ellipseMaskCount = layout.ellipseMaskCount;
            command.alphaMasks = layout.alphaMasks;
            command.alphaMaskCount = layout.alphaMaskCount;
            renderer.SubmitUIImage(command, pass.GetID());
        }
    }

    // 2. Dropdown arrow indicator (e.g. "v")
    {
        const std::string arrow = "v";
        auto& cachedArrow = m_TextCache[{ fontPixels, "", arrow }];
        if (!cachedArrow.submitted && !cachedArrow.texture.IsValid())
            cachedArrow.texture = CreateTextTexture(*rhi, arrow, "", fontPixels);
        cachedArrow.submitted = true;
        if (const auto* arrowTex = cachedArrow.texture.Get())
        {
            const auto& header = arrowTex->GetCachedHeader();
            const glm::vec2 bitmapSize(header.width, header.height);
            const glm::vec2 offset = -layout.size * rect.pivot + glm::vec2(layout.size.x - padX - bitmapSize.x + 2.0f, (layout.size.y - bitmapSize.y) * 0.5f);

            UIImageDrawCommand command;
            command.textureHandle = cachedArrow.texture;
            command.materialHandle = material;
            command.worldMatrix = glm::scale(glm::translate(layout.worldTransform, glm::vec3(offset, 0.0f)), glm::vec3(bitmapSize, 1.0f));
            command.color = dropdown.textColor;
            command.canvasSize = canvasSize;
            command.clipRect = layout.clipRect;
            command.ellipseMaskPlanes = layout.ellipseMaskPlanes;
            command.ellipseMaskCount = layout.ellipseMaskCount;
            command.alphaMasks = layout.alphaMasks;
            command.alphaMaskCount = layout.alphaMaskCount;
            renderer.SubmitUIImage(command, pass.GetID());
        }
    }
#endif
}

void Engine::UIRenderSystem::SubmitDropdownPopup(Renderer& renderer, RenderPass& pass, const entt::registry& world,
    const UIElementLayout& layout, const UIDropdownComponent& dropdown, const glm::vec2& canvasSize,
    const ResourceHandle<MaterialInterface>& material, const ResourceHandle<Texture>& white)
{
#if defined(_WIN32)
    if (!dropdown.isOpen || dropdown.options.empty() || layout.size.x <= 0.0f || layout.size.y <= 0.0f) return;
    auto* rhi = renderer.GetRHI();
    const auto* mat = material.Get();
    const auto* whiteTex = white.Get();
    if (!rhi || !mat || !whiteTex || !pass.IsAcceptsBlendMode(mat->GetBlendMode())) return;

    const float popupHeight = dropdown.GetPopupHeight();
    if (popupHeight <= 0.0f) return;

    const auto drawRect = [&](glm::vec2 offset, glm::vec2 size, glm::vec4 color)
    {
        if (size.x <= 0.0f || size.y <= 0.0f || color.a <= 0.0f ||
            !glm::all(glm::greaterThanEqual(color, glm::vec4(0.0f))) ||
            !glm::all(glm::lessThanEqual(color, glm::vec4(1.0f)))) return;
        UIImageDrawCommand command;
        command.textureHandle = white;
        command.materialHandle = material;
        command.worldMatrix = glm::scale(glm::translate(layout.renderMatrix, glm::vec3(offset, 0.0f)), glm::vec3(size, 1.0f));
        command.color = color;
        command.canvasSize = canvasSize;
        command.clipRect = layout.clipRect;
        command.ellipseMaskPlanes = layout.ellipseMaskPlanes;
        command.ellipseMaskCount = layout.ellipseMaskCount;
        command.alphaMasks = layout.alphaMasks;
        command.alphaMaskCount = layout.alphaMaskCount;
        renderer.SubmitUIImage(command, pass.GetID());
    };

    // 1. Popup background quad
    drawRect({ 0.0f, 1.0f }, { 1.0f, popupHeight / layout.size.y }, dropdown.popupBackgroundColor);

    const int visibleCount = std::min(static_cast<int>(dropdown.options.size()), std::max(1, dropdown.maxVisibleItems));
    int startIndex = static_cast<int>(dropdown.scrollOffset / dropdown.itemHeight);
    startIndex = std::clamp(startIndex, 0, std::max(0, static_cast<int>(dropdown.options.size()) - visibleCount));

    int fontPixels = std::clamp(static_cast<int>(dropdown.itemHeight * 0.65f), 10, 24);
    std::string fontFamily;
    if (const auto* textComp = TryGetEnabledComponent<UITextComponent>(world, layout.entity))
    {
        fontFamily = textComp->fontFamily;
    }

    const auto& rect = world.get<RectTransformComponent>(layout.entity);
    const float padX = 8.0f;

    for (int i = 0; i < visibleCount; ++i)
    {
        const int optIndex = startIndex + i;
        if (optIndex >= static_cast<int>(dropdown.options.size())) break;

        const float rowTop = layout.size.y + static_cast<float>(i) * dropdown.itemHeight;

        // Highlight hovered or selected row
        if (optIndex == dropdown.selectedIndex)
        {
            drawRect({ 0.0f, rowTop / layout.size.y }, { 1.0f, dropdown.itemHeight / layout.size.y }, dropdown.highlightColor);
        }

        // Render option text
        const auto& optText = dropdown.options[static_cast<std::size_t>(optIndex)];
        if (!optText.empty())
        {
            auto& cached = m_TextCache[{ fontPixels, fontFamily, optText }];
            if (!cached.submitted && !cached.texture.IsValid())
                cached.texture = CreateTextTexture(*rhi, optText, fontFamily, fontPixels);
            cached.submitted = true;
            if (const auto* texture = cached.texture.Get())
            {
                const auto& header = texture->GetCachedHeader();
                const glm::vec2 bitmapSize(header.width, header.height);
                const glm::vec2 offset = -layout.size * rect.pivot + glm::vec2(padX - 2.0f, rowTop + (dropdown.itemHeight - bitmapSize.y) * 0.5f);

                UIImageDrawCommand command;
                command.textureHandle = cached.texture;
                command.materialHandle = material;
                command.worldMatrix = glm::scale(glm::translate(layout.worldTransform, glm::vec3(offset, 0.0f)), glm::vec3(bitmapSize, 1.0f));
                command.color = dropdown.textColor;
                command.canvasSize = canvasSize;
                command.clipRect = layout.clipRect;
                command.ellipseMaskPlanes = layout.ellipseMaskPlanes;
                command.ellipseMaskCount = layout.ellipseMaskCount;
                command.alphaMasks = layout.alphaMasks;
                command.alphaMaskCount = layout.alphaMaskCount;
                renderer.SubmitUIImage(command, pass.GetID());
            }
        }
    }
#endif
}
