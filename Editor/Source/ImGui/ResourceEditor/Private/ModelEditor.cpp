#pragma once
#include "ModelEditor.h"
#include "Application.h"
#include "Scene.h"
#include "SceneManager.h"
#include "SceneEditHistory.h"
#include "SelectionManager.h"
#include "ResourceManager.h"
#include "Model.h"
#include "Material.h"
#include "Logger.h"
#include "ResourcePicker.h"
#include "PIEManager.h"

bool ModelEditor::IsSupported(const std::filesystem::path& assetPath) const
{
    const auto ext = assetPath.extension().wstring();
    return ext == L".fbx" || ext == L".obj" || ext == L".gltf" || ext == L".glb" || ext == L".bammodel";
}

void ModelEditor::Draw()
{
	if (!m_Open)
		return;

	const string title = GetDisplayName() + " - " + m_TargetResourcePath.filename().string() +
		"###" + WStrToStr(m_Name);

	if (ImGui::Begin(title.c_str(), &m_Open))
	{
		ImGui::Text("%s", LOCAL_FMT("ResourceEditor.Target", m_TargetResourcePath.string()).c_str());

		auto& history = Application::Get().GetEditHistory();
		history.AllowShortcuts(ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows));

		if (auto* model = m_ModelHandle.Get())
		{
			const bool canEdit = PIEManager::Get().IsEditing();

			ImGui::BeginDisabled(!canEdit);
			if (ImGui::Button(LOCAL_LABEL("MaterialEditor.Save").c_str()))
				SaveModel();
			ImGui::EndDisabled();

			if (!canEdit)
				ImGui::TextDisabled("%s", LOCAL("ModelEditor.EditModeOnly").c_str());

			ImGui::Separator();

			const auto modelKey = WStrToStr(model->GetKey());
			ImGui::PushID(modelKey.c_str());
			DrawMaterialSlots(*model, canEdit);
			ImGui::PopID();

			ImGui::Separator();

			const bool canPlace = history.Enabled() && SceneManager::Get().GetCurrentScene();
			ImGui::BeginDisabled(!canPlace);
			if (ImGui::Button(LOCAL_LABEL("ResourceEditor.InstantiateModel").c_str()))
				InstantiateModel();
			ImGui::EndDisabled();

			if (!m_StatusKey.empty())
				ImGui::TextWrapped("%s", LOCAL(m_StatusKey).c_str());
		}
		else
		{
			ImGui::TextWrapped("%s", LOCAL("ModelEditor.NoModel").c_str());
			if (ImGui::Button(LOCAL_LABEL("MaterialEditor.Retry").c_str()))
				SetTargetResource(m_TargetResourcePath);
		}
	}
	ImGui::End();
}

void ModelEditor::InstantiateModel()
{
	Scene* scene = SceneManager::Get().GetCurrentScene();
	auto& history = Application::Get().GetEditHistory();
	if (!scene || !history.Enabled())
		return;
	auto& manager = ResourceManager::Get();
	auto model = m_ModelHandle;
	const wchar_t* defaultMatPath = (model.IsValid() && model->GetSkeleton().IsValid())
		? L"Resources/Material/SkinningMaterial.bammat.json"
		: L"Resources/Material/GBufferMaterial.bammat.json";
	ResourceHandle<Material> material(manager.LoadFile(defaultMatPath));
	if (!model.IsValid() || !material.IsValid())
	{
		EDITOR_LOG_ERROR("Model placement requires a valid model and material.");
		return;
	}

	auto before = SceneEditSnapshot::Capture(*scene);
	if (!before)
	{
		EDITOR_LOG_ERROR("Failed to capture scene before model placement.");
		return;
	}

	Entity* root = nullptr;
	const EResult result = scene->InstantiateModel(*model, material, root);

	if (result != EResult::Success)
	{
		EDITOR_LOG_ERROR("Model placement failed: {}", ResultToString(result));
		return;
	}

	history.Record(std::move(before), SceneEditSnapshot::Capture(*scene), "UI_HISTORY_CREATE");
	SelectionManager::Get().SetSelectedEntity(root);
}

void ModelEditor::SetTargetResource(const std::filesystem::path& assetPath)
{
	ResourceEditorInterface::SetTargetResource(assetPath);
	m_ModelHandle.Reset();
	m_StatusKey.clear();

	if (assetPath.extension() != L".bammodel")
		return;

	auto& manager = ResourceManager::Get();
	const auto handle = manager.LoadFile(assetPath.wstring());
	if (dynamic_cast<Model*>(manager.GetResource(handle)) != nullptr)
		m_ModelHandle = ResourceHandle<Model>(handle);
}

void ModelEditor::DrawMaterialSlots(Model& model, bool canEdit)
{
	const auto materials = model.GetMaterials();

	if (materials.empty())
	{
		ImGui::TextWrapped("%s", LOCAL("ModelEditor.NoSlots").c_str());
		return;
	}

	for (size_t i = 0; i < materials.size(); ++i)
	{
		const auto slotId = std::to_string(i);
		ImGui::PushID(slotId.c_str());
		ImGui::Text("%s", LOCAL_FMT("ModelEditor.MaterialSlot", i).c_str());

		auto candidate = materials[i];
		if (DrawInspectorResourcePicker(candidate, canEdit) && candidate != materials[i])
		{
			const EResult result = model.SetMaterial(static_cast<uint32>(i), candidate);
			m_StatusKey = result == EResult::Success ?
				"ModelEditor.PendingSave" : "UI_RESOURCE_REJECTED";
		}

		ImGui::PopID();
	}
}

void ModelEditor::SaveModel()
{
	auto* model = m_ModelHandle.Get();
	if (!model || !PIEManager::Get().IsEditing())
		return;

	const EResult result = ResourceManager::Get().SaveToBinaryFile(model, m_TargetResourcePath.wstring());
	if (result != EResult::Success)
	{
		m_StatusKey = "ModelEditor.SaveFailed";
		EDITOR_LOG_ERROR("Model save failed: path={}, result={}",
			m_TargetResourcePath.string(), ResultToString(result));
		return;
	}

	m_StatusKey = "ModelEditor.Saved";
}