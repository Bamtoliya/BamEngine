#include "ModelImportModal.h"
#include "ModelConverter.h"
#include "ResourceManager.h"
#include "AssetManager.h"
#include "Scene.h"
#include "Components/Core/TransformComponent.h"
#include "Components/Render/StaticMeshRendererComponent.h"
#include "Components/Render/AnimatorComponent.h"
#include "IconsFontAwesome7.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <algorithm>

namespace Editor
{
	ModelImportModal::ModelImportModal()
	{
		s_Instance = this;
		m_Name = L"Model Import Options";
		m_DisplayNameKey = "ModelImport.ModalTitle";
		m_Open = false;
	}

	EResult ModelImportModal::Initialize(void* arg)
	{
		(void)arg;
		m_PreviewWidget.Initialize(L"ModelImportModalPreview", 380, 380, true);
		return EResult::Success;
	}

	void ModelImportModal::Free()
	{
		ClearPreview();
		m_PreviewWidget.Free();
		if (s_Instance == this) s_Instance = nullptr;
	}

	void ModelImportModal::Open(const std::filesystem::path& sourcePath, const std::filesystem::path& destDir)
	{
		m_SourcePath = sourcePath;
		m_DestDir = destDir;
		m_Desc = ModelImporter::GetDefaultDesc(sourcePath);
		m_Desc.Overwrite = true; // 재임포트 시 덮어쓰기 보장

		m_Open = true;
		m_OpenRequested = true;
		LoadPreviewModel();
	}

	void ModelImportModal::ClearPreview()
	{
		if (m_PreviewEntity && m_PreviewWidget.GetScene())
		{
			(void)m_PreviewWidget.GetScene()->DestroyEntity(*m_PreviewEntity);
			m_PreviewEntity = nullptr;
		}

		auto& rm = Engine::ResourceManager::Get();
		for (auto& handle : m_TempMeshes)
		{
			if (handle.IsValid()) rm.DestroyResource(handle.GetRawHandle());
		}
		m_TempMeshes.clear();
	}

	void ModelImportModal::LoadPreviewModel()
	{
		ClearPreview();
		if (!m_PreviewWidget.GetScene()) return;

		auto& rm = Engine::ResourceManager::Get();
		const std::string stem = m_SourcePath.stem().string();

		// 1) 기존 임포트된 .bammodel 에셋이 이미 존재하는 경우 (재임포트 경로)
		std::filesystem::path bammodelCandidate = m_DestDir / (stem + ".bammodel");
		if (!std::filesystem::exists(bammodelCandidate))
			bammodelCandidate = m_SourcePath.parent_path() / (stem + ".bammodel");

		if (std::filesystem::exists(bammodelCandidate))
		{
			const auto handle = rm.LoadFile(bammodelCandidate.wstring());
			if (auto* model = dynamic_cast<Engine::Model*>(rm.GetResource(handle)))
			{
				const wchar_t* defaultMatPath = model->GetSkeleton().IsValid()
					? L"Resources/Material/SkinningMaterial.bammat.json"
					: L"Resources/Material/GBufferMaterial.bammat.json";
				auto matHandle = Engine::ResourceHandle<Engine::MaterialInterface>(rm.LoadFile(defaultMatPath));

				if (m_PreviewWidget.GetScene()->InstantiateModel(*model, matHandle, m_PreviewEntity) == Engine::EResult::Success)
				{
					if (m_PreviewEntity)
					{
						auto& reg = m_PreviewWidget.GetRegistry();
						if (auto* anim = reg.try_get<Engine::AnimatorComponent>(m_PreviewEntity->GetEntityHandle()))
						{
							anim->isPlaying = false;
							anim->currentTime = 0.0f;
						}
					}
					m_PreviewWidget.FocusTarget(Engine::vec3(0.0f, 0.5f, 0.0f), 3.0f);
					UpdatePreviewTransform();
					return;
				}
			}
		}

		// 2) 신규 외부 모델(.glb, .fbx 등): ConvertStaticModel을 통한 메모리 임시 메쉬 생성
		Tools::ModelConversion::Options convOptions;
		convOptions.FlipUVs = HasFlag(m_Desc.PostProcessFlags, EModelPostProcessFlag::FlipUVs);
		convOptions.ConvertToLeftHanded = HasFlag(m_Desc.PostProcessFlags, EModelPostProcessFlag::MakeLeftHanded);
		convOptions.UniformScale = 1.0f;

		auto converted = Tools::ModelConversion::ConvertStaticModel(m_SourcePath, convOptions);
		if (!converted || !converted.Model || converted.Model->Meshes.empty()) return;

		const auto& modelData = *converted.Model;
		auto defaultMatHandle = Engine::ResourceHandle<Engine::MaterialInterface>(
			rm.LoadFile(L"Resources/Material/GBufferMaterial.bammat.json"));

		m_PreviewEntity = &m_PreviewWidget.CreateEntity(L"ImportPreviewEntity");
		Engine::vec3 aabbMin(1e9f), aabbMax(-1e9f);

		for (size_t i = 0; i < modelData.Meshes.size(); ++i)
		{
			const auto& srcMesh = modelData.Meshes[i];
			if (srcMesh.Vertices.empty() || srcMesh.Indices.empty()) continue;

			Engine::MeshCreateDesc meshDesc{};
			std::vector<Engine::VertexPosition> positions(srcMesh.Vertices.size());
			std::vector<Engine::VertexMaterial> materials(srcMesh.Vertices.size());

			for (size_t v = 0; v < srcMesh.Vertices.size(); ++v)
			{
				const auto& vert = srcMesh.Vertices[v];
				positions[v].position = { vert.Position[0], vert.Position[1], vert.Position[2] };
				materials[v].normal = { vert.Normal[0], vert.Normal[1], vert.Normal[2] };
				materials[v].texCoord = { vert.TexCoord[0], vert.TexCoord[1] };
				materials[v].tangent = { vert.Tangent[0], vert.Tangent[1], vert.Tangent[2] };
				materials[v].binormal = { vert.Bitangent[0], vert.Bitangent[1], vert.Bitangent[2] };
				materials[v].color = { vert.Color[0], vert.Color[1], vert.Color[2], vert.Color[3] };
			}

			meshDesc.Streams[(uint32)Engine::EMeshStream::Position] = { positions.data(), (uint32)positions.size(), (uint32)sizeof(Engine::VertexPosition) };
			meshDesc.Streams[(uint32)Engine::EMeshStream::Material] = { materials.data(), (uint32)materials.size(), (uint32)sizeof(Engine::VertexMaterial) };
			meshDesc.IndexData = (void*)srcMesh.Indices.data();
			meshDesc.IndexCount = (uint32)srcMesh.Indices.size();
			meshDesc.IndexStride = sizeof(uint32);
			meshDesc.BoundingBoxMin = { srcMesh.BoundsMin[0], srcMesh.BoundsMin[1], srcMesh.BoundsMin[2] };
			meshDesc.BoundingBoxMax = { srcMesh.BoundsMax[0], srcMesh.BoundsMax[1], srcMesh.BoundsMax[2] };

			aabbMin = glm::min(aabbMin, meshDesc.BoundingBoxMin);
			aabbMax = glm::max(aabbMax, meshDesc.BoundingBoxMax);

			auto* newMesh = Engine::Mesh::Create(&meshDesc);
			if (!newMesh) continue;

			const std::wstring tempKey = L"__temp_preview_mesh_" + std::to_wstring(i);
			auto meshHandle = rm.AddResource<Engine::Mesh>(tempKey, newMesh);
			m_TempMeshes.push_back(meshHandle);

			auto& reg = m_PreviewWidget.GetRegistry();
			if (i == 0)
			{
				auto& renderer = reg.emplace<Engine::StaticMeshRendererComponent>(m_PreviewEntity->GetEntityHandle());
				renderer.meshHandle = meshHandle;
				renderer.materialHandle = defaultMatHandle;
			}
			else
			{
				auto& child = m_PreviewWidget.CreateEntity(L"ImportPreviewChild");
				std::ignore = m_PreviewWidget.GetScene()->ReparentEntity(child, m_PreviewEntity);
				auto& renderer = reg.emplace<Engine::StaticMeshRendererComponent>(child.GetEntityHandle());
				renderer.meshHandle = meshHandle;
				renderer.materialHandle = defaultMatHandle;
			}
		}

		const Engine::vec3 center = (aabbMin + aabbMax) * 0.5f;
		const float radius = glm::length(aabbMax - aabbMin) * 0.5f;
		m_PreviewWidget.FocusTarget(center, std::max(radius * 2.2f, 2.0f));
		UpdatePreviewTransform();
	}

	void ModelImportModal::UpdatePreviewTransform()
	{
		if (!m_PreviewEntity) return;

		auto& reg = m_PreviewWidget.GetRegistry();
		auto* transform = reg.try_get<Engine::TransformComponent>(m_PreviewEntity->GetEntityHandle());
		if (!transform) return;

		transform->scale = Engine::vec3(m_Desc.UniformScale);

		const Engine::quat rotX = glm::angleAxis(glm::radians(m_Desc.RotationOffset[0]), Engine::vec3(1.0f, 0.0f, 0.0f));
		const Engine::quat rotY = glm::angleAxis(glm::radians(m_Desc.RotationOffset[1]), Engine::vec3(0.0f, 1.0f, 0.0f));
		const Engine::quat rotZ = glm::angleAxis(glm::radians(m_Desc.RotationOffset[2]), Engine::vec3(0.0f, 0.0f, 1.0f));
		transform->rotation = rotZ * rotY * rotX;
	}

	void ModelImportModal::Update(f32 dt)
	{
		if (m_Open)
		{
			m_PreviewWidget.BeginFrame(dt);
			m_PreviewWidget.Submit(dt);
		}
	}

	void ModelImportModal::Draw()
	{
		const std::string modalTitle = LOCAL("ModelImport.ModalTitle") + "###ModelImportOptionsModal";

		if (m_OpenRequested)
		{
			ImGui::OpenPopup(modalTitle.c_str());
			m_OpenRequested = false;
		}

		const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(880.0f, 560.0f), ImGuiCond_Appearing);

		if (ImGui::BeginPopupModal(modalTitle.c_str(), &m_Open, ImGuiWindowFlags_NoScrollbar))
		{
			const float availWidth = ImGui::GetContentRegionAvail().x;
			const float availHeight = ImGui::GetContentRegionAvail().y - 45.0f;

			const float minLeft = 280.0f;
			const float minRight = 360.0f;
			const float maxLeft = std::max(minLeft, availWidth - minRight - 12.0f);
			m_SplitterWidth = std::clamp(m_SplitterWidth, minLeft, maxLeft);

			// ── [좌측] 3D 프리뷰 뷰포트 ──
			const ImGuiWindowFlags vpFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
			if (ImGui::BeginChild("##ImportPreviewPane", ImVec2(m_SplitterWidth, availHeight), true, vpFlags))
			{
				m_PreviewWidget.Draw(ImVec2(m_SplitterWidth - 16.0f, availHeight - 16.0f));
			}
			ImGui::EndChild();

			ImGui::SameLine();

			// ── [중앙] 가변 스플리터 ──
			ImGui::Button("##ImportModalSplitter", ImVec2(4.0f, availHeight));
			if (ImGui::IsItemActive()) m_SplitterWidth += ImGui::GetIO().MouseDelta.x;
			if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

			ImGui::SameLine();

			// ── [우측] 옵션 인스펙터 ──
			if (ImGui::BeginChild("##ImportOptionsPane", ImVec2(0.0f, availHeight), true))
			{
				ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), ICON_FA_CUBE " %s", m_SourcePath.filename().string().c_str());
				ImGui::TextDisabled("%s", m_SourcePath.string().c_str());
				ImGui::Spacing();
				ImGui::Separator();
				ImGui::Spacing();

				// Transform 옵션
				if (ImGui::CollapsingHeader(LOCAL_CSTR("ModelImport.Transform"), ImGuiTreeNodeFlags_DefaultOpen))
				{
					if (ImGui::DragFloat(LOCAL_LABEL("ModelImport.Scale").c_str(), &m_Desc.UniformScale, 0.01f, 0.0001f, 1000.0f, "%.4f"))
						UpdatePreviewTransform();

					ImGui::SameLine();
					if (ImGui::SmallButton(LOCAL_CSTR("ModelImport.ScalePoint01"))) { m_Desc.UniformScale = 0.01f; UpdatePreviewTransform(); }
					ImGui::SameLine();
					if (ImGui::SmallButton(LOCAL_CSTR("ModelImport.Scale1"))) { m_Desc.UniformScale = 1.0f; UpdatePreviewTransform(); }
					ImGui::SameLine();
					if (ImGui::SmallButton(LOCAL_CSTR("ModelImport.Scale100"))) { m_Desc.UniformScale = 100.0f; UpdatePreviewTransform(); }

					if (ImGui::DragFloat3(LOCAL_LABEL("ModelImport.Rotation").c_str(), m_Desc.RotationOffset.data(), 1.0f, -360.0f, 360.0f, "%.1f deg"))
						UpdatePreviewTransform();

					ImGui::SameLine();
					if (ImGui::SmallButton(LOCAL_CSTR("ModelImport.Pitch180"))) { m_Desc.RotationOffset[0] = 180.0f; UpdatePreviewTransform(); }
					ImGui::SameLine();
					if (ImGui::SmallButton(LOCAL_CSTR("ModelImport.Yaw180"))) { m_Desc.RotationOffset[1] = 180.0f; UpdatePreviewTransform(); }
					ImGui::SameLine();
					if (ImGui::SmallButton(LOCAL_CSTR("ModelImport.Reset"))) { m_Desc.RotationOffset = { 0.0f, 0.0f, 0.0f }; UpdatePreviewTransform(); }

					bool leftHanded = HasFlag(m_Desc.PostProcessFlags, EModelPostProcessFlag::MakeLeftHanded);
					if (ImGui::Checkbox(LOCAL_LABEL("ModelImport.LeftHanded").c_str(), &leftHanded))
					{
						if (leftHanded) m_Desc.PostProcessFlags |= EModelPostProcessFlag::MakeLeftHanded;
						else m_Desc.PostProcessFlags &= ~EModelPostProcessFlag::MakeLeftHanded;
					}

					bool flipUVs = HasFlag(m_Desc.PostProcessFlags, EModelPostProcessFlag::FlipUVs);
					if (ImGui::Checkbox(LOCAL_LABEL("ModelImport.FlipUVs").c_str(), &flipUVs))
					{
						if (flipUVs) m_Desc.PostProcessFlags |= EModelPostProcessFlag::FlipUVs;
						else m_Desc.PostProcessFlags &= ~EModelPostProcessFlag::FlipUVs;
					}

					bool flipWinding = HasFlag(m_Desc.PostProcessFlags, EModelPostProcessFlag::FlipWindingOrder);
					if (ImGui::Checkbox(LOCAL_LABEL("ModelImport.FlipWinding").c_str(), &flipWinding))
					{
						if (flipWinding) m_Desc.PostProcessFlags |= EModelPostProcessFlag::FlipWindingOrder;
						else m_Desc.PostProcessFlags &= ~EModelPostProcessFlag::FlipWindingOrder;
					}
				}

				ImGui::Spacing();

				// Meshes & Materials 옵션
				if (ImGui::CollapsingHeader(LOCAL_CSTR("ModelImport.MeshesAndMaterials"), ImGuiTreeNodeFlags_DefaultOpen))
				{
					bool importMeshes = HasFlag(m_Desc.ImportOptions, EModelImportOption::Meshes);
					if (ImGui::Checkbox(LOCAL_LABEL("ModelImport.ImportMeshes").c_str(), &importMeshes))
					{
						if (importMeshes) m_Desc.ImportOptions |= EModelImportOption::Meshes;
						else m_Desc.ImportOptions &= ~EModelImportOption::Meshes;
					}

					bool importMats = HasFlag(m_Desc.ImportOptions, EModelImportOption::Materials);
					if (ImGui::Checkbox(LOCAL_LABEL("ModelImport.ImportMaterials").c_str(), &importMats))
					{
						if (importMats) m_Desc.ImportOptions |= EModelImportOption::Materials;
						else m_Desc.ImportOptions &= ~EModelImportOption::Materials;
					}
				}

				ImGui::Spacing();

				// Animation & Rig 옵션
				if (ImGui::CollapsingHeader(LOCAL_CSTR("ModelImport.AnimationAndRig"), ImGuiTreeNodeFlags_DefaultOpen))
				{
					bool importSkel = HasFlag(m_Desc.ImportOptions, EModelImportOption::Skeleton);
					if (ImGui::Checkbox(LOCAL_LABEL("ModelImport.ImportSkeleton").c_str(), &importSkel))
					{
						if (importSkel) m_Desc.ImportOptions |= EModelImportOption::Skeleton;
						else m_Desc.ImportOptions &= ~EModelImportOption::Skeleton;
					}

					bool importAnims = HasFlag(m_Desc.ImportOptions, EModelImportOption::Animations);
					if (ImGui::Checkbox(LOCAL_LABEL("ModelImport.ImportAnimations").c_str(), &importAnims))
					{
						if (importAnims) m_Desc.ImportOptions |= EModelImportOption::Animations;
						else m_Desc.ImportOptions &= ~EModelImportOption::Animations;
					}
				}
			}
			ImGui::EndChild();

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			// 하단 버튼
			if (ImGui::Button((ICON_FA_DOWNLOAD " " + LOCAL_LABEL("ModelImport.ImportBtn")).c_str(), ImVec2(120.0f, 0.0f)))
			{
				ClearPreview(); // 비동기 임포터가 새 파일을 쓰기 전에 프리뷰 점유 해제!
				AssetManager::Get().ImportAsync(m_SourcePath, m_DestDir, &m_Desc);
				m_Open = false;
				ImGui::CloseCurrentPopup();
			}

			ImGui::SameLine(0.0f, 10.0f);
			if (ImGui::Button(LOCAL_CSTR("EditorCommon.Cancel"), ImVec2(100.0f, 0.0f)))
			{
				ClearPreview();
				m_Open = false;
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
		else
		{
			if (m_Open) ClearPreview();
			m_Open = false;
		}
	}
}