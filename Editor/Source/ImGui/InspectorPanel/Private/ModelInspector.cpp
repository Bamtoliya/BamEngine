#pragma once
#include "ModelInspector.h"
#include "PropertyDrawer.h"
#include "AssetManager.h"
#include "Mesh.h"
#include "reflection/runtime/Registry.h"
#include "reflection/core/EnumInfo.h"

namespace
{
	// 리플렉션을 통해 모든 종류의 비트마스크 ENUM을 그려주는 만능 템플릿 함수
	template<typename TEnum>
	void DrawReflectionEnumFlags(const char* headerLabel, TEnum& flagsValue, const char* reflectionEnumName)
	{
		if (ImGui::CollapsingHeader(headerLabel, ImGuiTreeNodeFlags_DefaultOpen))
		{
			// ImGui 위젯 ID 충돌 방지를 위해 PushID 사용
			ImGui::PushID(headerLabel);

			uint32* flags = reinterpret_cast<uint32*>(&flagsValue);

			if (ImGui::Button("Select All")) { *flags = 0xFFFFFFFF; }
			ImGui::SameLine();
			if (ImGui::Button("Clear All")) { *flags = 0; }

			ImGui::Spacing();

			// 리플렉션 레지스트리에서 ENUM 메타데이터 조회
			const reflection::EnumInfo* enumInfo = reflection::Registry::Get().GetEnumByQualifiedName(reflectionEnumName);

			if (enumInfo)
			{
				// 2열 테이블로 깔끔하게 배치
				if (ImGui::BeginTable("FlagsTable", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
				{
					for (const reflection::EnumEntry& entry : enumInfo->Entries)
					{
						// "None"(0) 값은 체크박스로 그릴 의미가 없으므로 패스
						if (entry.Value == 0) continue;

						ImGui::TableNextColumn();
						std::string label(entry.Name);
						ImGui::CheckboxFlags(label.c_str(), flags, static_cast<uint32>(entry.Value));
					}
					ImGui::EndTable();
				}
			}
			else
			{
				ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "Error: %s not found!", reflectionEnumName);
			}

			ImGui::PopID();
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

	DrawReflectionEnumFlags("Import Targets", m_ImportDesc.ImportOptions, "EModelImportOption");
	DrawReflectionEnumFlags("Assimp PostProcess", m_ImportDesc.PostProcessFlags, "EModelPostProcessFlag");

	ImGui::Spacing();
	if (ImGui::Button("Import Model", ImVec2(-1, 30)))
	{
		filesystem::path relSourcePath = std::filesystem::relative(assetPath, std::filesystem::current_path());
		filesystem::path relDestDir = relSourcePath.parent_path();
		AssetManager::Get().Import(relSourcePath, relDestDir, &m_ImportDesc);
	}

	return true;
}
