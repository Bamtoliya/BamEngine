#pragma once
#include "TextureEditor.h"
#include "Functions.h"
#include "AssetCache.h"
#include "ImGuiManager.h"
#include "RHITexture.h"
#include "EditorUI.h"
#include "IconsFontAwesome7.h"

bool TextureEditor::IsSupported(const std::filesystem::path& assetPath) const
{
    const auto ext = Engine::ToLowerStr(Engine::WStrToStr(assetPath.extension().wstring()));
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga" ||
        ext == ".dds" || ext == ".hdr" ||
        ext == ".bamtex" || Engine::ToLowerStr(Engine::WStrToStr(assetPath.filename().wstring())).ends_with(".bamtex.json");
}

void TextureEditor::DrawCustomQuickAccess()
{
    EditorUI::ToolbarLayout toolbar;
    DrawCopyResourcePath();
    if (!m_TargetResourcePath.empty())
    {
        toolbar.Text(m_TargetResourcePath.filename().string().c_str());
        if (auto* texture = AssetCache::Get().GetThumbnail(m_TargetResourcePath))
        {
            const std::string dim = std::to_string(texture->GetWidth()) + "x" + std::to_string(texture->GetHeight());
            toolbar.Text(dim.c_str(), false);
        }
    }
}
void TextureEditor::DrawCustomContent()
{
    if (m_TargetResourcePath.empty()) return;
    auto* texture = AssetCache::Get().GetThumbnail(m_TargetResourcePath);
    if (texture && texture->GetDimension() == Engine::ETextureDimension::Texture2D &&
        texture->GetArraySize() == 1 && texture->GetWidth() > 0 && texture->GetHeight() > 0)
    {
        // 텍스처 썸네일 중앙 미리보기
        const auto id = ImGuiManager::Get().GetImGuiTextureID(texture);
        const auto width = static_cast<float>(texture->GetWidth());
        const auto height = static_cast<float>(texture->GetHeight());
        const float maxPreviewSize = 384.0f;
        const float scale = std::min(1.0f, maxPreviewSize / std::max(width, height));
        if (id != ImTextureID{})
        {
            ImGui::Spacing();
            ImGui::Image(id, ImVec2(width * scale, height * scale));
            ImGui::Spacing();
            ImGui::Separator();
        }
        // 텍스처 메타데이터 정보 테이블
        if (ImGui::BeginTable("TextureMetaTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 100.0f);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(LOCAL_CSTR("TextureEditor.Dimensions"));
            ImGui::TableSetColumnIndex(1); ImGui::Text("%u x %u", texture->GetWidth(), texture->GetHeight());
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(LOCAL_CSTR("EditorCommon.Path"));
            ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(m_TargetResourcePath.string().c_str());
            ImGui::EndTable();
        }
    }
    else
    {
        EditorUI::EmptyState("ContentBrowser.NoPreviewAvailable");
    }
}
