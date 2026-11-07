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
#include "IconsFontAwesome7.h"
#include "Components/Render/AnimatorComponent.h"

ModelEditor::ModelEditor()
{
	m_Name = L"Model Editor";
	m_DisplayNameKey = "ResourceEditor.ModelTitle";
	m_PreviewWidget.Initialize(L"ModelEditorPreview", 512, 512, true);
}

ModelEditor::~ModelEditor()
{
	m_PreviewWidget.Free();
}

namespace
{
	bool IsImportedModelFile(const std::filesystem::path& path)
	{
		const auto name = ToLowerStr(Engine::WStrToStr(path.filename().wstring()));
		return name.ends_with(".bammodel") || name.ends_with(".bammodel.json");
	}
}

bool ModelEditor::IsSupported(const std::filesystem::path& assetPath) const
{
	const auto ext = assetPath.extension().wstring();
	return ext == L".fbx" || ext == L".obj" || ext == L".gltf" || ext == L".glb" || IsImportedModelFile(assetPath);
}

void ModelEditor::Update(f32 dt)
{
	if (!m_Open || !m_ModelHandle.IsValid())
		return;

	m_PreviewWidget.BeginFrame(dt);
	m_PreviewWidget.Submit(dt);
}

void ModelEditor::SetupPreviewModel()
{
	if (!m_PreviewWidget.GetScene())
		return;

	// 기존 프리뷰 엔티티 및 하위 메쉬들 제거
	if (m_PreviewEntity)
	{
		(void)m_PreviewWidget.GetScene()->DestroyEntity(*m_PreviewEntity);
		m_PreviewEntity = nullptr;
	}

	auto* model = m_ModelHandle.Get();
	if (!model) return;

	// 기본 머티리얼 선택 (모델 자체 슬롯의 첫 번째 머티리얼 또는 기본 셰이딩 머티리얼)
	auto& rm = Engine::ResourceManager::Get();
	auto materials = model->GetMaterials();
	Engine::ResourceHandle<Engine::MaterialInterface> matHandle;
	if (!materials.empty() && materials[0].IsValid())
	{
		matHandle = materials[0];
	}
	else
	{
		const wchar_t* defaultMatPath = model->GetSkeleton().IsValid()
			? L"Resources/Material/SkinningMaterial.bammat.json"
			: L"Resources/Material/GBufferMaterial.bammat.json";
		matHandle = Engine::ResourceHandle<Engine::MaterialInterface>(rm.LoadFile(defaultMatPath));
	}

	// 프리뷰 씬에 모델 인스턴스화
	const Engine::EResult result = m_PreviewWidget.GetScene()->InstantiateModel(*model, matHandle, m_PreviewEntity);
	if (result != Engine::EResult::Success)
	{
		EDITOR_LOG_ERROR("Failed to instantiate preview model: {}", ResultToString(result));
		return;
	}
	// 모델 에디터에서는 기본적으로 정적인 T-포즈(바인드 포즈)로 표시
	if (m_PreviewEntity)
	{
		auto& reg = m_PreviewWidget.GetRegistry();
		if (auto* animator = reg.try_get<Engine::AnimatorComponent>(m_PreviewEntity->GetEntityHandle()))
		{
			animator->isPlaying = false;
			animator->currentTime = 0.0f;
		}
		if (!m_PreviewWidget.SetModelFocusTarget(*m_PreviewEntity))
			EDITOR_LOG_ERROR("Failed to frame the preview model.");
	}
}

void ModelEditor::Draw()
{
	if (!m_Open)
		return;

	const auto title = BuildResourceWindowTitle();

	if (BeginPanelWindow(title.c_str(), m_Open))
	{
		auto& history = Application::Get().GetEditHistory();
		history.AllowShortcuts(ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows));

		const float height = ImGui::GetContentRegionAvail().y;
		const auto previewFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
		Editor::DrawSidebarLayout("##ModelViewportPane", "##ModelSplitter", "##ModelDetailsPane",
			m_LeftPaneWidth, height, 250.0f, 280.0f,
			[this] { m_PreviewWidget.Draw(); },
			[this, &history]
			{
				ImGui::Text("%s", LOCAL_FMT("ResourceEditor.Target", m_TargetResourcePath.string()).c_str());
				ImGui::Separator();

				if (auto* model = m_ModelHandle.Get())
				{
					const bool canEdit = PIEManager::Get().IsEditing();

					ImGui::BeginDisabled(!canEdit);
					if (ImGui::Button((ICON_FA_FLOPPY_DISK " " + LOCAL_LABEL("EditorCommon.Save")).c_str()))
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
					if (ImGui::Button((ICON_FA_PLUS " " + LOCAL_LABEL("ResourceEditor.InstantiateModel")).c_str()))
						InstantiateModel();
					ImGui::EndDisabled();

					if (!m_StatusKey.empty())
						ImGui::TextWrapped("%s", LOCAL(m_StatusKey).c_str());
				}
				else
				{
					ImGui::TextWrapped("%s", LOCAL("ModelEditor.NoModel").c_str());
					if (ImGui::Button(LOCAL_LABEL("ModelEditor.RetryLoad").c_str()))
						SetTargetResource(m_TargetResourcePath);
				}
			}, previewFlags, /*reserveHalfForContent=*/false);
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
	const EResult result = scene->InstantiateLinkedModel(model, material, root);

	if (result != EResult::Success)
	{
		EDITOR_LOG_ERROR("Model placement failed: {}", ResultToString(result));
		return;
	}

	history.Record(std::move(before), SceneEditSnapshot::Capture(*scene), "History.Command.CreateEntity");
	SelectionManager::Get().SetSelectedEntity(root);
}

void ModelEditor::SetTargetResource(const std::filesystem::path& assetPath)
{
	ResourceEditorInterface::SetTargetResource(assetPath);
	m_ModelHandle.Reset();
	m_StatusKey.clear();
	m_IsDirty = false;

	if (!IsImportedModelFile(assetPath))
		return;

	auto& manager = ResourceManager::Get();
	const auto handle = manager.LoadFile(assetPath.wstring());
	if (dynamic_cast<Model*>(manager.GetResource(handle)) != nullptr)
	{
		m_ModelHandle = ResourceHandle<Model>(handle);
		SetupPreviewModel();
	}
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
				"ModelEditor.PendingSave" : "ResourcePicker.Rejected";

			// 슬롯 머티리얼 변경 시 3D 프리뷰 모델 실시간 재인스턴스화
			if (result == EResult::Success)
			{
				m_IsDirty = true;
				SetupPreviewModel();
			}
		}

		ImGui::PopID();
	}
}

void ModelEditor::SaveModel()
{
	auto* model = m_ModelHandle.Get();
	if (!model || !PIEManager::Get().IsEditing())
		return;

	auto& manager = ResourceManager::Get();
	const auto path = m_TargetResourcePath.wstring();
	const bool isJson = ToLowerStr(Engine::WStrToStr(m_TargetResourcePath.extension().wstring())) == ".json";
	const EResult result = isJson ? manager.SaveToJsonFile(model, path) : manager.SaveToBinaryFile(model, path);
	if (result != EResult::Success)
	{
		m_StatusKey = "ModelEditor.SaveFailed";
		EDITOR_LOG_ERROR("Model save failed: path={}, result={}",
			m_TargetResourcePath.string(), ResultToString(result));
		return;
	}

	m_StatusKey = "ModelEditor.Saved";
	m_IsDirty = false;
}
void ModelEditor::CaptureState(EditorWorkspace& workspace) const
{
	IImGui::CaptureState(workspace);
	workspace.SetSplitter("ModelEditor.LeftPaneWidth", m_LeftPaneWidth);
}

void ModelEditor::RestoreState(const EditorWorkspace& workspace)
{
	IImGui::RestoreState(workspace);
	m_LeftPaneWidth = workspace.GetSplitter("ModelEditor.LeftPaneWidth", 420.0f);
}
