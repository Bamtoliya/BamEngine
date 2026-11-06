#pragma once
#include "ShaderEditor.h"

bool ShaderEditor::IsSupported(const std::filesystem::path& assetPath) const
{
    const auto ext = assetPath.extension().wstring();
    return ext == L".bamshader" || ext == L".hlsl" || ext == L".glsl";
}

void ShaderEditor::Draw()
{
    if (!m_Open) return;
    const auto title = BuildResourceWindowTitle();

    if (BeginPanelWindow(title.c_str(), m_Open))
    {
        ImGui::Text("%s", LOCAL_FMT("ResourceEditor.Target", m_TargetResourcePath.string().c_str()).c_str());
    }
    ImGui::End();
}