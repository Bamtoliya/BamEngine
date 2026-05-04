#pragma once
#include "TextureEditor.h"

bool TextureEditor::IsSupported(const std::filesystem::path& assetPath) const
{
    const auto ext = assetPath.extension().wstring();
    return ext == L".png" || ext == L".jpg" || ext == L".jpeg" || ext == L".bamtex";
}

void TextureEditor::Draw()
{
    if (!m_Open) return;
    std::string title = "Texture Editor - " + m_TargetResourcePath.filename().string();
    if (ImGui::Begin(title.c_str(), &m_Open))
    {
        // 기존 TextureInspector::OnInspectorGUI 내용 이식
    }
    ImGui::End();
}
