#pragma once
#include "EditorUI.h"
#include "ModelEditor.h"
#include "ShortcutManager.h"
#include "Application.h"
#include "Scene.h"
#include "SceneManager.h"
#include "SceneEditHistory.h"
#include "SelectionManager.h"
#include "ResourceManager.h"
#include "Model.h"
#include "Material.h"
#include "Skeleton.h"
#include "Animation.h"
#include "Logger.h"
#include "Functions.h"
#include "ResourcePicker.h"
#include "PIEManager.h"
#include "IconsFontAwesome7.h"
#include "AnimatorComponent.h"
#include "StaticMeshRendererComponent.h"
#include "SkinnedMeshRendererComponent.h"
#include "LODGroupComponent.h" 

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
		const auto name = Engine::ToLowerStr(Engine::WStrToStr(path.filename().wstring()));
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
	m_SelectedAnimIndex = 0;
	m_ForcedLOD = -1;
	if (m_PreviewEntity)
	{
		auto& reg = m_PreviewWidget.GetRegistry();
		if (auto* animator = reg.try_get<Engine::AnimatorComponent>(m_PreviewEntity->GetEntityHandle()))
		{
			animator->isPlaying = false;
			animator->currentTime = 0.0f;
			const auto anims = model->GetAnimations();
			if (!anims.empty())
				animator->currentClip = anims.front();
		}
		if (!m_PreviewWidget.SetModelFocusTarget(*m_PreviewEntity))
			EDITOR_LOG_ERROR("Failed to frame the preview model.");
	}
}

void ModelEditor::DrawCustomToolbar()
{
	if (ImGui::BeginMenu(LOCAL_LABEL("EditorMenu.File").c_str()))
	{
		const bool canEdit = PIEManager::Get().IsEditing() && m_ModelHandle.IsValid();
		ImGui::BeginDisabled(!canEdit);
		auto& sm = ShortcutManager::Get();
		if (ImGui::MenuItem((ICON_FA_FLOPPY_DISK " " + LOCAL_LABEL("EditorCommon.Save")).c_str(), sm.GetShortcutHint("Animator.Save")))
			SaveModel();
		ImGui::EndDisabled();

		if (ImGui::MenuItem((ICON_FA_ROTATE_RIGHT " " + LOCAL_LABEL("ModelEditor.RetryLoad")).c_str()))
			SetTargetResource(m_TargetResourcePath);

		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu(LOCAL_LABEL("EditorMenu.Edit").c_str()))
	{
		auto& history = Application::Get().GetEditHistory();
		const bool canPlace = history.Enabled() && SceneManager::Get().GetCurrentScene() && m_ModelHandle.IsValid();
		ImGui::BeginDisabled(!canPlace);
		if (ImGui::MenuItem((ICON_FA_PLUS " " + LOCAL_LABEL("ResourceEditor.InstantiateModel")).c_str()))
			InstantiateModel();
		ImGui::EndDisabled();

		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu(LOCAL_LABEL("EditorMenu.View").c_str()))
	{
		auto& sm = ShortcutManager::Get();
		if (ImGui::MenuItem((ICON_FA_CROSSHAIRS " " + LOCAL_LABEL("ModelEditor.Focus")).c_str(), sm.GetShortcutHint("Viewport.Focus")))
		{
			if (m_PreviewEntity) m_PreviewWidget.SetModelFocusTarget(*m_PreviewEntity);
		}

		if (ImGui::MenuItem((ICON_FA_BORDER_ALL " " + LOCAL_LABEL("ModelEditor.ToggleGrid")).c_str(), sm.GetShortcutHint("Viewport.ToggleGrid"), m_PreviewWidget.IsGridVisible()))
		{
			m_PreviewWidget.ToggleGrid();
		}

		ImGui::EndMenu();
	}
}

void ModelEditor::DrawCustomQuickAccess()
{
	EditorUI::ToolbarLayout toolbar;
	const bool canEdit = PIEManager::Get().IsEditing() && m_ModelHandle.IsValid();

	ImGui::BeginDisabled(!canEdit);
	if (toolbar.Icon("SaveModel", ICON_FA_FLOPPY_DISK, "EditorCommon.Save"))
		SaveModel();
	ImGui::EndDisabled();

	EditorUI::IconButtonOptions focusOptions;
	focusOptions.ShortcutHint = ShortcutManager::Get().GetShortcutHint("Viewport.Focus");
	if (toolbar.Icon("FocusModel", ICON_FA_CROSSHAIRS, "PreviewViewport.Focus", focusOptions))
	{
		if (m_PreviewEntity) m_PreviewWidget.SetModelFocusTarget(*m_PreviewEntity);
	}

	EditorUI::IconButtonOptions gridOptions;
	gridOptions.Active = m_PreviewWidget.IsGridVisible();
	gridOptions.ShortcutHint = ShortcutManager::Get().GetShortcutHint("Viewport.ToggleGrid");
	if (toolbar.Icon("ToggleGrid", ICON_FA_BORDER_ALL, "Action.Viewport.ToggleGrid", gridOptions))
		m_PreviewWidget.ToggleGrid();

	auto& history = Application::Get().GetEditHistory();
	const bool canPlace = history.Enabled() && SceneManager::Get().GetCurrentScene() && m_ModelHandle.IsValid();
	ImGui::BeginDisabled(!canPlace);
	if (toolbar.Button("InstantiateModel", ICON_FA_PLUS, "ResourceEditor.InstantiateModel"))
		InstantiateModel();
	ImGui::EndDisabled();

	if (auto* model = m_ModelHandle.Get())
	{
		const auto& animations = model->GetAnimations();
		if (!animations.empty())
		{
			auto* animator = GetPreviewAnimator();
			if (animator)
			{
				const char* playIcon = animator->isPlaying ? ICON_FA_PAUSE : ICON_FA_PLAY;
				if (toolbar.Icon("ModelPlayback", playIcon, animator->isPlaying ? "ModelEditor.PauseClip" : "ModelEditor.PlayClip"))
				{
					animator->isPlaying = !animator->isPlaying;
				}

				toolbar.SetNextItemWidth(ImGui::GetFontSize() * 10.0f);

				const int validIdx = (m_SelectedAnimIndex >= 0 && m_SelectedAnimIndex < static_cast<int>(animations.size()))
					? m_SelectedAnimIndex : 0;
				auto* curClip = animations[validIdx].Get();
				const std::string curName = curClip
					? (!curClip->GetName().empty() ? WStrToStr(curClip->GetName()) : std::filesystem::path(curClip->GetKey()).stem().string())
					: ("Clip #" + std::to_string(validIdx));

				if (ImGui::BeginCombo("##ToolbarAnimCombo", curName.c_str(), ImGuiComboFlags_HeightRegular))
				{
					for (size_t i = 0; i < animations.size(); ++i)
					{
						auto* clip = animations[i].Get();
						if (!clip) continue;
						const std::string name = !clip->GetName().empty()
							? WStrToStr(clip->GetName()) : std::filesystem::path(clip->GetKey()).stem().string();
						const bool isSelected = (static_cast<int>(i) == m_SelectedAnimIndex);
						if (ImGui::Selectable(name.c_str(), isSelected))
						{
							SelectAnimation(static_cast<int>(i), true);
						}
						if (isSelected) ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", LOCAL_CSTR("ModelEditor.SelectClip"));
			}
		}
	}

	if (auto* model = m_ModelHandle.Get())
	{
		const bool hasLODs = model->HasLODs();
		std::string lodLabel;
		if (m_ForcedLOD < 0) lodLabel = LOCAL("ModelEditor.LODAuto");
		else if (m_ForcedLOD == 0) lodLabel = LOCAL("ModelEditor.LOD0");
		else if (m_ForcedLOD == 1) lodLabel = LOCAL("ModelEditor.LOD1");
		else lodLabel = LOCAL("ModelEditor.LOD2");

		ImGui::BeginDisabled(!hasLODs);
		toolbar.SetNextItemWidth(ImGui::GetFontSize() * 9.0f);
		if (ImGui::BeginCombo("##ToolbarLODCombo", (ICON_FA_LAYER_GROUP " " + lodLabel).c_str(), ImGuiComboFlags_HeightRegular))
		{
			if (ImGui::Selectable(LOCAL_CSTR("ModelEditor.LODAuto"), m_ForcedLOD == -1))
				SetForcedLOD(-1);
			if (ImGui::Selectable(LOCAL_CSTR("ModelEditor.LOD0"), m_ForcedLOD == 0))
				SetForcedLOD(0);
			if (ImGui::Selectable(LOCAL_CSTR("ModelEditor.LOD1"), m_ForcedLOD == 1))
				SetForcedLOD(1);
			if (ImGui::Selectable(LOCAL_CSTR("ModelEditor.LOD2"), m_ForcedLOD == 2))
				SetForcedLOD(2);
			ImGui::EndCombo();
		}
		ImGui::EndDisabled();
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		{
			ImGui::SetTooltip("%s", hasLODs ? LOCAL_CSTR("ModelEditor.SelectLOD") : LOCAL_CSTR("ModelEditor.NoLODsAvailable"));
		}
	}

	// 파일 상태 배지
	const std::string fileName = m_TargetResourcePath.empty()
		? LOCAL("AnimatorGraph.Unsaved") : m_TargetResourcePath.filename().string();
	toolbar.Text(("[" + fileName + (m_IsDirty ? " *]" : "]")).c_str());
}

void ModelEditor::SetForcedLOD(int lod)
{
	m_ForcedLOD = lod;

	auto* model = m_ModelHandle.Get();
	if (!model || !m_PreviewEntity)
		return;

	auto* scene = m_PreviewWidget.GetScene();
	if (!scene)
		return;

	auto& reg = scene->GetRegistry();
	const auto& baseMeshes = model->GetMeshes();

	auto updateEntityMesh = [&](entt::entity e)
	{
		auto* skinned = reg.try_get<Engine::SkinnedMeshRendererComponent>(e);
		auto* rigid = reg.try_get<Engine::StaticMeshRendererComponent>(e);

		int foundMeshIndex = -1;
		for (size_t i = 0; i < baseMeshes.size(); ++i)
		{
			const uint32 idx = static_cast<uint32>(i);
			const auto baseHandle = baseMeshes[i];
			const auto lod1Handle = model->GetMeshLOD(idx, 1);
			const auto lod2Handle = model->GetMeshLOD(idx, 2);

			if ((skinned && (skinned->meshHandle == baseHandle || skinned->meshHandle == lod1Handle || skinned->meshHandle == lod2Handle)) ||
				(rigid && (rigid->meshHandle == baseHandle || rigid->meshHandle == lod1Handle || rigid->meshHandle == lod2Handle)))
			{
				foundMeshIndex = static_cast<int>(i);
				break;
			}
		}

		if (foundMeshIndex < 0) return;

		const uint32 meshIdx = static_cast<uint32>(foundMeshIndex);
		if (m_ForcedLOD < 0)
		{
			if (skinned) skinned->meshHandle = baseMeshes[meshIdx];
			if (rigid) rigid->meshHandle = baseMeshes[meshIdx];
		}
		else
		{
			auto targetMesh = model->GetMeshLOD(meshIdx, static_cast<uint32>(m_ForcedLOD));
			if (!targetMesh.IsValid())
				targetMesh = baseMeshes[meshIdx];

			if (skinned) skinned->meshHandle = targetMesh;
			if (rigid) rigid->meshHandle = targetMesh;
		}
	};

	auto skinnedView = reg.view<Engine::SkinnedMeshRendererComponent>();
	for (auto entity : skinnedView)
		updateEntityMesh(entity);

	auto rigidView = reg.view<Engine::StaticMeshRendererComponent>();
	for (auto entity : rigidView)
		updateEntityMesh(entity);
}

void ModelEditor::DrawLODInfo(Engine::Model& model)
{
	if (!ImGui::CollapsingHeader((ICON_FA_LAYER_GROUP " " + LOCAL("ModelEditor.LODHeader")).c_str(), ImGuiTreeNodeFlags_DefaultOpen))
		return;

	const auto& baseMeshes = model.GetMeshes();

	auto getMeshStats = [](const std::vector<Engine::ResourceHandle<Engine::Mesh>>& meshes, uint32& outTris, uint32& outVerts)
	{
		outTris = 0;
		outVerts = 0;
		for (const auto& h : meshes)
		{
			if (auto* m = h.Get())
			{
				outTris += m->GetIndexCount() / 3;
				outVerts += m->GetVertexCount();
			}
		}
	};

	uint32 lod0Tris = 0, lod0Verts = 0;
	getMeshStats(baseMeshes, lod0Tris, lod0Verts);

	std::vector<Engine::ResourceHandle<Engine::Mesh>> lod1Meshes;
	std::vector<Engine::ResourceHandle<Engine::Mesh>> lod2Meshes;
	for (size_t i = 0; i < baseMeshes.size(); ++i)
	{
		auto lod1 = model.GetMeshLOD(static_cast<uint32>(i), 1);
		if (lod1.IsValid()) lod1Meshes.push_back(lod1);
		auto lod2 = model.GetMeshLOD(static_cast<uint32>(i), 2);
		if (lod2.IsValid()) lod2Meshes.push_back(lod2);
	}

	uint32 lod1Tris = 0, lod1Verts = 0;
	getMeshStats(lod1Meshes, lod1Tris, lod1Verts);
	uint32 lod2Tris = 0, lod2Verts = 0;
	getMeshStats(lod2Meshes, lod2Tris, lod2Verts);

	// 전체 합산 헤더
	ImGui::TextDisabled("%s (%s %zu)", LOCAL_CSTR("ModelEditor.TotalModel"), LOCAL_CSTR("ModelEditor.MeshCount"), baseMeshes.size());

	if (ImGui::BeginTable("##ModelLODTable", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV))
	{
		ImGui::TableSetupColumn(LOCAL_CSTR("ModelEditor.LODLevel"), ImGuiTableColumnFlags_WidthFixed, 60.0f);
		ImGui::TableSetupColumn(LOCAL_CSTR("ModelEditor.Triangles"), ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn(LOCAL_CSTR("ModelEditor.Vertices"), ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn(LOCAL_CSTR("ModelEditor.Reduction"), ImGuiTableColumnFlags_WidthFixed, 75.0f);
		ImGui::TableSetupColumn(LOCAL_CSTR("ModelEditor.Action"), ImGuiTableColumnFlags_WidthFixed, 60.0f);
		ImGui::TableHeadersRow();

		// Row 0: LOD 0
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::Text("%s%s", LOCAL_CSTR("ModelEditor.LOD0Label"), m_ForcedLOD == 0 ? " *" : "");
		ImGui::TableSetColumnIndex(1);
		ImGui::Text("%u", lod0Tris);
		ImGui::TableSetColumnIndex(2);
		ImGui::Text("%u", lod0Verts);
		ImGui::TableSetColumnIndex(3);
		ImGui::TextDisabled("100%%");
		ImGui::TableSetColumnIndex(4);
		if (ImGui::SmallButton((LOCAL("ModelEditor.ViewLOD") + "##0").c_str()))
			SetForcedLOD(0);

		// Row 1: LOD 1
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::Text("%s%s", LOCAL_CSTR("ModelEditor.LOD1Label"), m_ForcedLOD == 1 ? " *" : "");
		ImGui::TableSetColumnIndex(1);
		if (!lod1Meshes.empty()) ImGui::Text("%u", lod1Tris);
		else ImGui::TextDisabled("-");
		ImGui::TableSetColumnIndex(2);
		if (!lod1Meshes.empty()) ImGui::Text("%u", lod1Verts);
		else ImGui::TextDisabled("-");
		ImGui::TableSetColumnIndex(3);
		if (!lod1Meshes.empty() && lod0Tris > 0)
			ImGui::Text("%.1f%%", (static_cast<float>(lod1Tris) / static_cast<float>(lod0Tris)) * 100.0f);
		else ImGui::TextDisabled("-");
		ImGui::TableSetColumnIndex(4);
		if (!lod1Meshes.empty())
		{
			if (ImGui::SmallButton((LOCAL("ModelEditor.ViewLOD") + "##1").c_str()))
				SetForcedLOD(1);
		}
		else ImGui::TextDisabled("-");

		// Row 2: LOD 2
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::Text("%s%s", LOCAL_CSTR("ModelEditor.LOD2Label"), m_ForcedLOD == 2 ? " *" : "");
		ImGui::TableSetColumnIndex(1);
		if (!lod2Meshes.empty()) ImGui::Text("%u", lod2Tris);
		else ImGui::TextDisabled("-");
		ImGui::TableSetColumnIndex(2);
		if (!lod2Meshes.empty()) ImGui::Text("%u", lod2Verts);
		else ImGui::TextDisabled("-");
		ImGui::TableSetColumnIndex(3);
		if (!lod2Meshes.empty() && lod0Tris > 0)
			ImGui::Text("%.1f%%", (static_cast<float>(lod2Tris) / static_cast<float>(lod0Tris)) * 100.0f);
		else ImGui::TextDisabled("-");
		ImGui::TableSetColumnIndex(4);
		if (!lod2Meshes.empty())
		{
			if (ImGui::SmallButton((LOCAL("ModelEditor.ViewLOD") + "##2").c_str()))
				SetForcedLOD(2);
		}
		else ImGui::TextDisabled("-");

		ImGui::EndTable();
	}

	if (m_ForcedLOD >= 0)
	{
		ImGui::Spacing();
		if (ImGui::Button((ICON_FA_ROTATE_LEFT " " + LOCAL("ModelEditor.ResetAutoLOD")).c_str()))
			SetForcedLOD(-1);
	}

	// 파츠별 세부 브레이크다운 (파츠가 2개 이상인 경우)
	if (baseMeshes.size() > 1)
	{
		ImGui::Spacing();
		if (ImGui::TreeNode(LOCAL_CSTR("ModelEditor.PartBreakdown")))
		{
			if (ImGui::BeginTable("##PartLODTable", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV))
			{
				ImGui::TableSetupColumn(LOCAL_CSTR("ModelEditor.PartName"), ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn(LOCAL_CSTR("ModelEditor.LOD0Label"), ImGuiTableColumnFlags_WidthFixed, 60.0f);
				ImGui::TableSetupColumn(LOCAL_CSTR("ModelEditor.LOD1Label"), ImGuiTableColumnFlags_WidthFixed, 75.0f);
				ImGui::TableSetupColumn(LOCAL_CSTR("ModelEditor.LOD2Label"), ImGuiTableColumnFlags_WidthFixed, 75.0f);
				ImGui::TableHeadersRow();

				for (size_t i = 0; i < baseMeshes.size(); ++i)
				{
					const uint32 idx = static_cast<uint32>(i);
					auto baseMesh = baseMeshes[i].Get();
					auto lod1Mesh = model.GetMeshLOD(idx, 1).Get();
					auto lod2Mesh = model.GetMeshLOD(idx, 2).Get();

					const uint32 p0Tris = baseMesh ? baseMesh->GetIndexCount() / 3 : 0;
					const uint32 p1Tris = lod1Mesh ? lod1Mesh->GetIndexCount() / 3 : 0;
					const uint32 p2Tris = lod2Mesh ? lod2Mesh->GetIndexCount() / 3 : 0;

					std::string partName = baseMesh ? std::filesystem::path(baseMesh->GetKey()).stem().string() : ("Part #" + std::to_string(i));
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("%s", partName.c_str());

					ImGui::TableSetColumnIndex(1);
					ImGui::Text("%u", p0Tris);

					ImGui::TableSetColumnIndex(2);
					if (lod1Mesh)
					{
						const float r1 = p0Tris > 0 ? (static_cast<float>(p1Tris) / static_cast<float>(p0Tris)) * 100.0f : 0.0f;
						ImGui::Text("%u (%.0f%%)", p1Tris, r1);
					}
					else
					{
						ImGui::TextDisabled("-");
					}

					ImGui::TableSetColumnIndex(3);
					if (lod2Mesh)
					{
						const float r2 = p0Tris > 0 ? (static_cast<float>(p2Tris) / static_cast<float>(p0Tris)) * 100.0f : 0.0f;
						ImGui::Text("%u (%.0f%%)", p2Tris, r2);
					}
					else
					{
						ImGui::TextDisabled("-");
					}
				}

				ImGui::EndTable();
			}
			ImGui::TreePop();
		}
	}
}

void ModelEditor::DrawModelInfo(Engine::Model& model)
{
	if (!ImGui::CollapsingHeader((ICON_FA_CIRCLE_INFO " " + LOCAL("ModelEditor.ModelInfo")).c_str(), ImGuiTreeNodeFlags_DefaultOpen))
		return;

	if (ImGui::BeginTable("##ModelInfoTable", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
	{
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::TextDisabled("%s", LOCAL_CSTR("ModelEditor.MeshCount"));
		ImGui::TableSetColumnIndex(1);
		ImGui::Text("%zu", model.GetMeshes().size());

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::TextDisabled("%s", LOCAL_CSTR("ModelEditor.NodeCount"));
		ImGui::TableSetColumnIndex(1);
		ImGui::Text("%zu", model.GetNodes().size());

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::TextDisabled("%s", LOCAL_CSTR("ModelEditor.SkeletonInfo"));
		ImGui::TableSetColumnIndex(1);
		const auto skeleton = model.GetSkeleton();
		if (skeleton.IsValid() && skeleton.Get())
		{
			const auto boneCount = skeleton.Get()->GetBones().size();
			ImGui::Text("%s (%s %zu)", LOCAL_CSTR("ModelEditor.HasSkeleton"), LOCAL_CSTR("ModelEditor.BoneCount"), boneCount);
		}
		else
		{
			ImGui::TextDisabled("%s", LOCAL_CSTR("ModelEditor.NoSkeleton"));
		}

		ImGui::EndTable();
	}
}

void ModelEditor::DrawAnimations(Engine::Model& model)
{
	const auto& animations = model.GetAnimations();
	if (animations.empty()) return;

	if (!ImGui::CollapsingHeader((ICON_FA_PERSON_RUNNING " " + LOCAL("ModelEditor.Animations")).c_str(), ImGuiTreeNodeFlags_DefaultOpen))
		return;

	auto* animator = GetPreviewAnimator();

	// 1. 애니메이션 클립 목록 테이블
	ImGui::TextDisabled("%s (%zu)", LOCAL_CSTR("ModelEditor.EmbeddedClips"), animations.size());

	if (ImGui::BeginTable("##ModelAnimTable", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV))
	{
		ImGui::TableSetupColumn("##Status", ImGuiTableColumnFlags_WidthFixed, 26.0f);
		ImGui::TableSetupColumn(LOCAL_CSTR("ModelEditor.ClipName"), ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn(LOCAL_CSTR("ModelEditor.Duration"), ImGuiTableColumnFlags_WidthFixed, 65.0f);
		ImGui::TableHeadersRow();

		for (size_t i = 0; i < animations.size(); ++i)
		{
			const auto animHandle = animations[i];
			auto* clip = animHandle.Get();
			if (!clip) continue;

			ImGui::TableNextRow();
			const bool isSelected = (static_cast<int>(i) == m_SelectedAnimIndex);
			const bool isCurrentlyPlaying = (animator && animator->isPlaying && animator->currentClip.Get() == clip);

			// 열 0: 재생/일시정지 작은 버튼
			ImGui::TableSetColumnIndex(0);
			const char* playIcon = isCurrentlyPlaying ? ICON_FA_PAUSE : ICON_FA_PLAY;
			ImGui::PushID(static_cast<int>(i));
			if (ImGui::SmallButton(playIcon))
			{
				if (isCurrentlyPlaying)
				{
					animator->isPlaying = false;
				}
				else
				{
					SelectAnimation(static_cast<int>(i), true);
				}
			}
			ImGui::PopID();

			// 열 1: 클립 이름 (선택 시 해당 클립으로 변경 및 재생)
			ImGui::TableSetColumnIndex(1);
			const std::string name = !clip->GetName().empty()
				? WStrToStr(clip->GetName())
				: (!clip->GetKey().empty() ? std::filesystem::path(clip->GetKey()).stem().string() : ("Clip #" + std::to_string(i)));

			if (ImGui::Selectable(name.c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns))
			{
				SelectAnimation(static_cast<int>(i), true);
			}

			// 열 2: 클립 길이 (초)
			ImGui::TableSetColumnIndex(2);
			const double tps = clip->GetTicksPerSecond() > 0.0 ? clip->GetTicksPerSecond() : 25.0;
			const float durationSec = clip->GetDuration() > 0.0 ? static_cast<float>(clip->GetDuration() / tps) : 0.0f;
			ImGui::Text("%s", LOCAL_FMT("ModelEditor.SecondsFormat", durationSec).c_str());
		}
		ImGui::EndTable();
	}

	ImGui::Spacing();

	// 2. 선택된 클립의 정밀 제어 (타임라인 슬라이더, 루프, 배속 조절)
	if (animator && m_SelectedAnimIndex >= 0 && m_SelectedAnimIndex < static_cast<int>(animations.size()))
	{
		auto* activeClip = animations[m_SelectedAnimIndex].Get();
		if (activeClip)
		{
			const double tps = activeClip->GetTicksPerSecond() > 0.0 ? activeClip->GetTicksPerSecond() : 25.0;
			const float durationSec = activeClip->GetDuration() > 0.0 ? static_cast<float>(activeClip->GetDuration() / tps) : 0.0f;

			// 재생 / 일시정지 / 바인드 포즈 리셋 버튼
			const std::string playIconText = animator->isPlaying
				? (ICON_FA_PAUSE " " + LOCAL("ModelEditor.PauseClip"))
				: (ICON_FA_PLAY " " + LOCAL("ModelEditor.PlayClip"));

			if (ImGui::Button(playIconText.c_str()))
			{
				animator->isPlaying = !animator->isPlaying;
			}
			ImGui::SameLine();
			if (ImGui::Button((ICON_FA_STOP " " + LOCAL("ModelEditor.StopAndReset")).c_str()))
			{
				animator->isPlaying = false;
				animator->currentTime = 0.0f;
			}

			// 타임라인 슬라이더
			float time = animator->currentTime;
			if (durationSec > 0.0f)
			{
				if (ImGui::SliderFloat("##AnimTimeSlider", &time, 0.0f, durationSec, "%.2f / %.2fs"))
				{
					animator->currentTime = time;
				}
			}

			// 루프 토글 및 배속 슬라이더
			ImGui::Checkbox(LOCAL_CSTR("ModelEditor.LoopAnimation"), &animator->isLooping);
			ImGui::SameLine(0.0f, 20.0f);
			ImGui::SetNextItemWidth(120.0f);
			ImGui::SliderFloat(LOCAL_CSTR("ModelEditor.PlayRate"), &animator->playRate, 0.1f, 3.0f, "%.2fx");
		}
	}
}

void ModelEditor::DrawCustomContent()
{
	auto& history = Application::Get().GetEditHistory();
	history.AllowShortcuts(m_Focused);


	const float height = ImGui::GetContentRegionAvail().y;
	const auto previewFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
	Editor::DrawSidebarLayout("##ModelViewportPane", "##ModelSplitter", "##ModelDetailsPane",
		m_LeftPaneWidth, height, 250.0f, 280.0f,
		[this] { m_PreviewWidget.Draw(); },
		[this, &history]
		{
			if (auto* model = m_ModelHandle.Get())
			{
				const bool canEdit = PIEManager::Get().IsEditing();

				DrawModelInfo(*model);
				ImGui::Spacing();

				DrawLODInfo(*model);
				ImGui::Spacing();

				const auto modelKey = WStrToStr(model->GetKey());
				ImGui::PushID(modelKey.c_str());
				DrawMaterialSlots(*model, canEdit);
				ImGui::PopID();
				ImGui::Spacing();

				DrawAnimations(*model);
				ImGui::Spacing();

				if (ImGui::CollapsingHeader((ICON_FA_LAYER_GROUP " " + LOCAL("ModelEditor.Placement")).c_str(), ImGuiTreeNodeFlags_DefaultOpen))
				{
					const bool canPlace = history.Enabled() && SceneManager::Get().GetCurrentScene();
					ImGui::BeginDisabled(!canPlace);
					if (ImGui::Button((ICON_FA_PLUS " " + LOCAL_LABEL("ResourceEditor.InstantiateModel")).c_str()))
						InstantiateModel();
					ImGui::EndDisabled();

					if (!m_StatusKey.empty())
						ImGui::TextWrapped("%s", LOCAL(m_StatusKey).c_str());
				}
			}
			else
			{
				ImGui::TextWrapped("%s", LOCAL("ModelEditor.NoModel").c_str());
				if (ImGui::Button(LOCAL_LABEL("ModelEditor.RetryLoad").c_str()))
					SetTargetResource(m_TargetResourcePath);
			}
		}, previewFlags, /*reserveHalfForContent=*/false);
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
	if (!ImGui::CollapsingHeader((ICON_FA_FILL_DRIP " " + LOCAL("ModelEditor.MaterialSlots")).c_str(), ImGuiTreeNodeFlags_DefaultOpen))
		return;

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
	const bool isJson = Engine::ToLowerStr(Engine::WStrToStr(m_TargetResourcePath.extension().wstring())) == ".json";
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

Engine::AnimatorComponent* ModelEditor::GetPreviewAnimator() const
{
	if (!m_PreviewEntity) return nullptr;
	auto& reg = const_cast<PreviewViewportWidget&>(m_PreviewWidget).GetRegistry();
	return reg.try_get<Engine::AnimatorComponent>(m_PreviewEntity->GetEntityHandle());
}

void ModelEditor::SelectAnimation(int index, bool autoPlay)
{
	auto* model = m_ModelHandle.Get();
	if (!model) return;
	const auto& animations = model->GetAnimations();
	if (index < 0 || index >= static_cast<int>(animations.size())) return;

	m_SelectedAnimIndex = index;
	auto* animator = GetPreviewAnimator();
	if (animator)
	{
		animator->currentClip = animations[index];
		animator->currentTime = 0.0f;
		if (autoPlay)
		{
			animator->isPlaying = true;
		}
	}
}
