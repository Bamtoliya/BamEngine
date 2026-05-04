#pragma once
#include "SpriteEditor.h"

bool SpriteEditor::IsSupported(const std::filesystem::path& assetPath) const
{
    const auto ext = assetPath.extension().wstring();
    return ext == L".bamsprite";
}

void SpriteEditor::Draw()
{
    if (!m_Open) return;
    std::string title = "Sprite Editor - " + m_TargetResourcePath.filename().string();
    if (ImGui::Begin(title.c_str(), &m_Open))
    {
        ImGui::Text("Target: %s", m_TargetResourcePath.string().c_str());
    }
    ImGui::End();
}