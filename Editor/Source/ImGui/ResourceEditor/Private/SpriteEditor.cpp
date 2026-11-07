#pragma once
#include "SpriteEditor.h"

bool SpriteEditor::IsSupported(const std::filesystem::path& assetPath) const
{
    const auto ext = assetPath.extension().wstring();
    return ext == L".bamsprite" ||
        ToLowerStr(Engine::WStrToStr(assetPath.filename().wstring())).ends_with(".bamsprite.json");
}

void SpriteEditor::Draw()
{
    if (!m_Open) return;
    const auto title = BuildResourceWindowTitle();
    if (BeginPanelWindow(title.c_str(), m_Open))
    {
        ImGui::Text("%s", LOCAL_FMT("ResourceEditor.Target", Engine::WStrToStr(m_TargetResourcePath.wstring())).c_str());
    }
    ImGui::End();
}