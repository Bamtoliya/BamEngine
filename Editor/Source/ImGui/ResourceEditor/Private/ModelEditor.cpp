#pragma once
#include "ModelEditor.h"

bool ModelEditor::IsSupported(const std::filesystem::path& assetPath) const
{
    const auto ext = assetPath.extension().wstring();
    return ext == L".fbx" || ext == L".obj" || ext == L".gltf" || ext == L".bammodel";
}

void ModelEditor::Draw()
{
    if (!m_Open) return;
    std::string title = "Model Editor - " + m_TargetResourcePath.filename().string();
    if (ImGui::Begin(title.c_str(), &m_Open))
    {
        ImGui::Text("Target: %s", m_TargetResourcePath.string().c_str());
    }
    ImGui::End();
}