#pragma once
#include "ShaderEditor.h"
#include "Functions.h"

bool ShaderEditor::IsSupported(const std::filesystem::path& assetPath) const
{
    const auto ext = assetPath.extension().wstring();
    return ext == L".bamshader" || ext == L".hlsl" || ext == L".glsl" || ext == L".cso" ||
        Engine::ToLowerStr(Engine::WStrToStr(assetPath.filename().wstring())).ends_with(".bamshader.json");
}

void ShaderEditor::Draw()
{
    if (!m_Open) return;
    const auto title = BuildResourceWindowTitle();

    if (BeginPanelWindow(title.c_str(), m_Open))
    {
        ImGui::Text("%s", LOCAL_FMT("ResourceEditor.Target", Engine::WStrToStr(m_TargetResourcePath.wstring())).c_str());
    }
    ImGui::End();
}