#pragma once
#include "TextureEditor.h"
#include "AssetCache.h"
#include "ImGuiManager.h"
#include "RHITexture.h"

bool TextureEditor::IsSupported(const std::filesystem::path& assetPath) const
{
    const auto ext = ToLowerStr(Engine::WStrToStr(assetPath.extension().wstring()));
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga" ||
        ext == ".dds" || ext == ".hdr" ||
        ext == ".bamtex" || ToLowerStr(Engine::WStrToStr(assetPath.filename().wstring())).ends_with(".bamtex.json");
}

void TextureEditor::Draw()
{
    if (!m_Open) return;
    const auto title = BuildResourceWindowTitle();

    if (BeginPanelWindow(title.c_str(), m_Open) && !m_TargetResourcePath.empty())
    {
        ImGui::TextWrapped("%s", LOCAL_FMT("ResourceEditor.Target", Engine::WStrToStr(m_TargetResourcePath.wstring())).c_str());

        auto* texture = AssetCache::Get().GetThumbnail(m_TargetResourcePath);
        if (texture && texture->GetDimension() == Engine::ETextureDimension::Texture2D &&
            texture->GetArraySize() == 1 && texture->GetWidth() > 0 && texture->GetHeight() > 0)
        {
            const auto id = ImGuiManager::Get().GetImGuiTextureID(texture);
            const auto width = static_cast<float>(texture->GetWidth());
            const auto height = static_cast<float>(texture->GetHeight());
            const float scale = std::min(1.0f, 256.0f / std::max(width, height));
            if (id != ImTextureID{})
                ImGui::Image(id, ImVec2(width * scale, height * scale));
        }
    }
    ImGui::End();
}
