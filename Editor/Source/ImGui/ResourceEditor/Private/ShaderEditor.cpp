#pragma once
#include "ShaderEditor.h"
#include "Functions.h"
#include "EditorUI.h"
#include "IconsFontAwesome7.h"
#include "HotReloadManager.h"
#include "ShaderHotReloadManager.h"
#include "LocalizationManager.h"

bool ShaderEditor::IsSupported(const std::filesystem::path& assetPath) const
{
    const auto ext = assetPath.extension().wstring();
    return ext == L".bamshader" || ext == L".hlsl" || ext == L".glsl" || ext == L".cso" ||
        Engine::ToLowerStr(Engine::Unicode::WideToUtf8(assetPath.filename().wstring())).ends_with(".bamshader.json");
}

void ShaderEditor::DrawCustomQuickAccess()
{
    EditorUI::ToolbarLayout toolbar;
    DrawCopyResourcePath();

    if (toolbar.Icon("RecompileShader", ICON_FA_ROTATE, "ShaderEditor.RecompileShader"))
    {
        if (HotReloadManager::IsCreated() && !m_TargetResourcePath.empty())
        {
            HotReloadManager::Get().GetShaderReloader().RecompileFile(m_TargetResourcePath);
        }
    }

    if (!m_TargetResourcePath.empty())
    {
        toolbar.Text(m_TargetResourcePath.filename().string().c_str());
    }
}

void ShaderEditor::DrawCustomContent()
{
    if (m_TargetResourcePath.empty())
    {
        EditorUI::EmptyState("ResourceEditor.NoResource");
        return;
    }

    ImGui::Spacing();
    if (ImGui::BeginTable("ShaderMetaTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn(LOCAL_LABEL("EditorCommon.Property").c_str(), ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableSetupColumn(LOCAL_LABEL("EditorCommon.Value").c_str(), ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(LOCAL_CSTR("EditorCommon.File"));
        ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(m_TargetResourcePath.filename().string().c_str());

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(LOCAL_CSTR("EditorCommon.Path"));
        ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(m_TargetResourcePath.string().c_str());

        ImGui::EndTable();
    }
}