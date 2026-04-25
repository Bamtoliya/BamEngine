#pragma	once

#include "ModelInspector.h"
#include "PropertyDrawer.h"
#include "AssetManager.h"
#include "Mesh.h"

namespace
{
	template<typename TEnum>
	void DrawFlagCheckbox(const char* label, TEnum bit, TEnum& flags)
	{
		bool enabled = HasFlag(flags, bit);
		if (ImGui::Checkbox(label, &enabled))
		{
			if (enabled) flags |= bit;
			else flags &= ~bit;
		}
	}
}

bool ModelInspector::IsSupported(const std::filesystem::path& assetPath)
{
    return assetPath.extension() == L".fbx" || assetPath.extension() == L".obj" || assetPath.extension() == L".gltf";
}

bool ModelInspector::OnInspectorGUI(const std::filesystem::path& assetPath)
{
	ImGui::TextColored(ImVec4(0.2f, 0.7f, 1.0f, 1.0f), "Model Inspector");

	if (ImGui::CollapsingHeader("Import Targets", ImGuiTreeNodeFlags_DefaultOpen))
	{
		DrawFlagCheckbox("Mesh", EModelImportOption::Meshes, m_ImportDesc.ImportOptions);
		DrawFlagCheckbox("Material", EModelImportOption::Materials, m_ImportDesc.ImportOptions);
		DrawFlagCheckbox("Skeleton", EModelImportOption::Skeleton, m_ImportDesc.ImportOptions);
		DrawFlagCheckbox("Animation", EModelImportOption::Animations, m_ImportDesc.ImportOptions);
	}

	if (ImGui::CollapsingHeader("Assimp PostProcess", ImGuiTreeNodeFlags_DefaultOpen))
	{
		DrawFlagCheckbox("Triangulate", EModelPostProcessFlag::Triangulate, m_ImportDesc.PostProcessFlags);
		DrawFlagCheckbox("FlipUVs", EModelPostProcessFlag::FlipUVs, m_ImportDesc.PostProcessFlags);
		DrawFlagCheckbox("JoinIdenticalVertices", EModelPostProcessFlag::JoinIdenticalVertices, m_ImportDesc.PostProcessFlags);
		DrawFlagCheckbox("GenSmoothNormals", EModelPostProcessFlag::GenSmoothNormals, m_ImportDesc.PostProcessFlags);
		DrawFlagCheckbox("CalcTangentSpace", EModelPostProcessFlag::CalcTangentSpace, m_ImportDesc.PostProcessFlags);
		DrawFlagCheckbox("ImproveCacheLocality", EModelPostProcessFlag::ImproveCacheLocality, m_ImportDesc.PostProcessFlags);
		DrawFlagCheckbox("OptimizeMeshes", EModelPostProcessFlag::OptimizeMeshes, m_ImportDesc.PostProcessFlags);
		DrawFlagCheckbox("OptimizeGraph", EModelPostProcessFlag::OptimizeGraph, m_ImportDesc.PostProcessFlags);
	}

	if (ImGui::Button("Import Model", ImVec2(-1, 30)))
	{
		filesystem::path relSourcePath = std::filesystem::relative(assetPath, std::filesystem::current_path());
		filesystem::path relDestDir = relSourcePath.parent_path();
		AssetManager::Get().Import(relSourcePath, relDestDir, &m_ImportDesc);
	}

	return true;
}