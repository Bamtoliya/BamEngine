#include "UIRenderSystem.h"
#include "UILayout.h"
#include "LocalizationManager.h"
#include "RenderPass.h"
#include "Renderer.h"
#include "ResourceManager.h"
#include "Texture.h"

#include <glm/gtc/matrix_transform.hpp>
#include <atomic>
#include <cstring>
#include <memory>

#if defined(_WIN32)
namespace
{
    Engine::ResourceHandle<Engine::Texture> CreateTextTexture(Engine::RHI& rhi, const std::string& text, int fontPixels)
    {
        const auto byteCount = static_cast<int>(text.size());
        const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), byteCount, nullptr, 0);
        if (length <= 0) return {};
        std::wstring line(length, L'\0');
        if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), byteCount, line.data(), length) != length)
            return {};

        // Destroy the DC before the selected font and bitmap.
        const auto deleteObject = [](auto* object) { DeleteObject(object); };
        std::unique_ptr<std::remove_pointer_t<HFONT>, decltype(deleteObject)> font(
            CreateFontW(-fontPixels, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Malgun Gothic"), deleteObject);
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
}
#endif

void Engine::UIRenderSystem::SubmitText(Renderer& renderer, RenderPass& pass, const entt::registry& world,
    const UIElementLayout& layout, const UITextComponent& text, const glm::vec2& canvasSize,
    const ResourceHandle<MaterialInterface>& material)
{
#if defined(_WIN32)
    const auto* resolved = &text.text;
    if (!text.localizationKey.empty() && LocalizationManager::IsCreated())
        if (const auto* localized = LocalizationManager::Get().FindText(text.localizationKey)) resolved = localized;
    if (layout.size.x <= 0.0f || layout.size.y <= 0.0f || resolved->empty() || resolved->size() > UITextComponent::MaxTextBytes ||
        resolved->find_first_of("\r\n") != std::string::npos) return;

    auto* rhi = renderer.GetRHI();
    const auto* mat = material.Get();
    if (!rhi || !mat || !pass.IsAcceptsBlendMode(mat->GetBlendMode())) return;

    auto& cached = m_TextCache[{ text.fontPixels, *resolved }];
    if (!cached.submitted && !cached.texture.IsValid()) cached.texture = CreateTextTexture(*rhi, *resolved, text.fontPixels);
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
    offset.y += available.y * 0.5f;

    UIImageDrawCommand command;
    command.textureHandle = cached.texture;
    command.materialHandle = material;
    command.worldMatrix = glm::scale(glm::translate(layout.worldTransform, glm::vec3(offset, 0.0f)), glm::vec3(size, 1.0f));
    command.color = text.color;
    command.canvasSize = canvasSize;
    command.clipRect = layout.clipRect;
    renderer.SubmitUIImage(command, pass.GetID());
#endif
}
