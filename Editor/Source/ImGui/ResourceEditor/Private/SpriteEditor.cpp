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
    const string title = GetDisplayName() + " - " + m_TargetResourcePath.filename().string() +
        "###" + WStrToStr(m_Name);
    if (ImGui::Begin(title.c_str(), &m_Open))
    {
        ImGui::Text("%s", LOCAL_FMT("ResourceEditor.Target", m_TargetResourcePath.string().c_str()).c_str());
    }
    ImGui::End();
}