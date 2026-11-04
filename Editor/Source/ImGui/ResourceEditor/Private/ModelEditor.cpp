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

bool ModelEditor::IsSupported(const std::filesystem::path& assetPath) const
{
	const auto ext = assetPath.extension().wstring();
	return ext == L".fbx" || ext == L".obj" || ext == L".gltf" || ext == L".glb" || ext == L".bammodel";
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
	}
	// 모델 중심 쿼터뷰 포커싱
	m_PreviewWidget.FocusTarget(Engine::vec3(0.0f, 0.5f, 0.0f), 3.0f);
}

void ModelEditor::Draw()
{
	if (!m_Open)
		return;

	const string title = GetDisplayName() + " - " + m_TargetResourcePath.filename().string() +
		"###" + WStrToStr(m_Name);

	if (ImGui::Begin(title.c_str(), &m_Open))
	{
		auto& history = Application::Get().GetEditHistory();
		history.AllowShortcuts(ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows));

		const float availWidth = ImGui::GetContentRegionAvail().x;
		const float availHeight = ImGui::GetContentRegionAvail().y;

		// 가이드라인 17절: 최소/최대 분할 너비 클램핑
		const float minLeft = 250.0f;
		const float minRight = 280.0f;
		const float maxLeft = std::max(minLeft, availWidth - minRight - 12.0f);
		m_LeftPaneWidth = std::clamp(m_LeftPaneWidth, minLeft, maxLeft);

		// ── [좌측 영역] 3D 프리뷰 뷰포트 ──
		const ImGuiWindowFlags vpFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
		if (ImGui::BeginChild("##ModelViewportPane", ImVec2(m_LeftPaneWidth, availHeight), true, vpFlags))
		{
			m_PreviewWidget.Draw(ImVec2(m_LeftPaneWidth - 16.0f, availHeight - 16.0f));
		}
		ImGui::EndChild();

		ImGui::SameLine();

		// ── [중앙 영역] 수평 스플리터 (마우스 드래그 리사이징) ──
		ImGui::Button("##ModelSplitter", ImVec2(4.0f, availHeight));
		if (ImGui::IsItemActive())
		{
			m_LeftPaneWidth += ImGui::GetIO().MouseDelta.x;
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
		}

		ImGui::SameLine();

		// ── [우측 영역] 모델 상세 정보 & 머티리얼 슬롯 인스펙터 ──
		if (ImGui::BeginChild("##ModelDetailsPane", ImVec2(0.0f, availHeight), true))
		{
			ImGui::Text("%s", LOCAL_FMT("ResourceEditor.Target", m_TargetResourcePath.string()).c_str());
			ImGui::Separator();

			if (auto* model = m_ModelHandle.Get())
			{
				const bool canEdit = PIEManager::Get().IsEditing();

				ImGui::BeginDisabled(!canEdit);
				if (ImGui::Button((ICON_FA_FLOPPY_DISK " " + LOCAL_LABEL("MaterialEditor.Save")).c_str()))
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
				if (ImGui::Button(LOCAL_LABEL("MaterialEditor.Retry").c_str()))
					SetTargetResource(m_TargetResourcePath);
			}
		}
		ImGui::EndChild();
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
				"ModelEditor.PendingSave" : "UI_RESOURCE_REJECTED";

			// 슬롯 머티리얼 변경 시 3D 프리뷰 모델 실시간 재인스턴스화
			if (result == EResult::Success)
			{
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