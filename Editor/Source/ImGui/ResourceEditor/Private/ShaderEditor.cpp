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
    std::string title = "Shader Editor - " + m_TargetResourcePath.filename().string();
    if (ImGui::Begin(title.c_str(), &m_Open))
    {
        ImGui::Text("Target: %s", m_TargetResourcePath.string().c_str());
    }
    ImGui::End();
}