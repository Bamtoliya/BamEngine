#include "ModelImportModal.h"
#include "EditorUI.h"
#include "ModelConverter.h"
#include "ResourceManager.h"
#include "AssetManager.h"
#include "Scene.h"
#include "TransformComponent.h"
#include "AnimatorComponent.h"
#include "IconsFontAwesome7.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <thread>
#include "ModelImportTask.h"
#include "Model.h"
#include "TransformSystem.h"
#include "MaterialInstance.h"
#include "Texture.h"
#include "ImGuiManager.h"
#include "ModelSubfolderHelper.h"

namespace Editor
{
	ModelImportModal::ModelImportModal()
	{
		s_Instance = this;
		m_Name = L"Model Import Options";
		m_DisplayNameKey = "ModelImport.ModalTitle";
		m_Open = false;
	}

	ModelImportModal::~ModelImportModal()
	{
		if (m_PreviewThread.joinable())
			m_PreviewThread.join();
	}

	EResult ModelImportModal::OnInitialize(void*)
	{
		m_PreviewWidget.Initialize(L"ModelImportModalPreview", 380, 380, true);
		return EResult::Success;
	}

	void ModelImportModal::Free()
	{
		CancelAsyncPreview();
		if (m_PreviewThread.joinable())
			m_PreviewThread.join();
		m_AsyncPreview.reset();
		ClearPreview();
		m_PreviewWidget.Free();
		if (s_Instance == this) s_Instance = nullptr;
	}

	void ModelImportModal::Open(const std::filesystem::path& sourcePath, const std::filesystem::path& destDir)
	{
		if (m_Open || m_CloseRequested || sourcePath.empty()) return;

		m_AlbedoPreviewInfo.clear();

		m_SourcePath = sourcePath;
		m_DestDir = ModelImporter::ResolveWritableDestinationDirectory(sourcePath, destDir, &m_RedirectedFromReadOnly);
		if (m_RedirectedFromReadOnly)
		{
			BAM_LOG(Info, "Editor", "Asset",
				"Redirected read-only model import destination from '{}' to '{}'",
				Engine::WStrToStr(destDir.wstring()), Engine::WStrToStr(m_DestDir.wstring()));
		}
		m_Desc = {};
		m_Desc.Overwrite = true;

		LoadSavedImportOptions();

		m_PreviewReady = false;
		m_PreviewBuilding = false;
		m_PreviewProgress = 0.0f;
		m_PreviewReloadRequested = true;
		m_CloseRequested = false;
		m_ImportRequested = false;
		m_OpenRequested = true;
		m_Open = true;
	}

	void ModelImportModal::LoadSavedImportOptions()
	{
		const auto sidecar = ModelImporter::ResolveSidecarPath(m_SourcePath, m_DestDir);
		if (!sidecar.empty())
		{
			ModelImportDesc loaded;
			if (ModelImporter::LoadImportOptions(sidecar, loaded))
			{
				m_Desc = loaded;
				m_Desc.Overwrite = true; // 재임포트 시 덮어쓰기 유지
			}
		}
	}

	void ModelImportModal::ApplyDccPreset(EModelImportDccPreset preset)
	{
		m_Desc.DccPreset = preset;
		switch (preset)
		{
		case EModelImportDccPreset::Blender:
			m_Desc.UniformScale = 1.0f;
			m_Desc.RotationOffset = { 0.0f, 0.0f, 0.0f };
			m_Desc.FlipPitch = false;
			m_Desc.FlipHeading = false;
			m_Desc.PostProcessFlags |= (EModelPostProcessFlag::MakeLeftHanded | EModelPostProcessFlag::FlipUVs | EModelPostProcessFlag::FlipWindingOrder);
			break;
		case EModelImportDccPreset::Unreal:
			m_Desc.UniformScale = 0.01f; // Unreal cm -> BamEngine m
			m_Desc.RotationOffset = { -90.0f, 0.0f, 0.0f }; // Unreal Z-up -> BamEngine Y-up
			m_Desc.FlipPitch = false;
			m_Desc.FlipHeading = false;
			m_Desc.PostProcessFlags |= (EModelPostProcessFlag::MakeLeftHanded | EModelPostProcessFlag::FlipUVs | EModelPostProcessFlag::FlipWindingOrder);
			break;
		case EModelImportDccPreset::Unity:
			m_Desc.UniformScale = 1.0f;
			m_Desc.RotationOffset = { 0.0f, 0.0f, 0.0f };
			m_Desc.FlipPitch = false;
			m_Desc.FlipHeading = false;
			m_Desc.PostProcessFlags |= (EModelPostProcessFlag::MakeLeftHanded | EModelPostProcessFlag::FlipUVs | EModelPostProcessFlag::FlipWindingOrder);
			break;
		case EModelImportDccPreset::Custom:
		default:
			break;
		}
	}

	void ModelImportModal::ClearPreview()
	{
		m_PreviewReady = false;
		m_PreviewBuilding = false;
		m_PreviewProgress = 0.0f;
		m_BuildingTask.reset();
		m_BuildingModelDesc = {};
		m_BuildingBaseMaterial = {};
		m_NextBuildMeshIndex = 0;
		m_NextBuildMaterialIndex = 0;

		if (m_PreviewEntity)
		{
			auto* scene = m_PreviewWidget.GetScene();
			if (!scene || !scene->DestroyEntity(*m_PreviewEntity))
			{
				BAM_LOG(Error, "Editor", "Asset", "Model import preview entity cleanup failed.");
			}

			m_PreviewEntity = nullptr;
		}

		auto& manager = Engine::ResourceManager::Get();
		for (auto& handle : m_TempMaterials)
		{
			if (handle.IsValid())
				(void)manager.DestroyResource(handle.GetRawHandle());
		}
		m_TempMaterials.clear();

		for (auto& handle : m_TempTextures)
		{
			if (handle.IsValid())
				(void)manager.DestroyResource(handle.GetRawHandle());
		}
		m_TempTextures.clear();

		for (auto& handle : m_TempMeshes)
		{
			if (handle.IsValid())
				(void)manager.DestroyResource(handle.GetRawHandle());
		}
		m_TempMeshes.clear();
	}

	void ModelImportModal::StartAsyncPreviewPrepare(bool focusCamera)
	{
		if (m_PreviewThread.joinable())
		{
			if (m_AsyncPreview)
				m_AsyncPreview->Cancelled.store(true, std::memory_order_release);
			m_PreviewThread.join();
		}

		m_AlbedoPreviewInfo.clear();
		ClearPreview();

		ModelImportRequest request;
		request.SourcePath = m_SourcePath;
		request.Settings = m_Desc;
		request.Settings.SaveToFile = false;
		request.Settings.GenerateLODs = false; // 프리뷰는 LOD0만 사용하여 파싱 속도 극대화
		request.Settings.ImportOptions = EModelImportOption::Meshes;
		if (HasFlag(m_Desc.ImportOptions, EModelImportOption::Materials))
			request.Settings.ImportOptions |= EModelImportOption::Materials;
		request.Settings.UniformScale = 1.0f;
		request.Settings.RotationOffset = { 0.0f, 0.0f, 0.0f };
		request.Settings.FlipPitch = false;
		request.Settings.FlipHeading = false;

		auto state = std::make_shared<AsyncPreviewState>();
		state->Task = std::make_unique<ModelImportTask>(std::move(request));
		state->FocusCamera = focusCamera;
		m_AsyncPreview = state;
		m_PreviewProgress = 0.05f;

		m_PreviewThread = std::thread([state]()
			{
				const EResult res = state->Task->Prepare();
				// 폐기된 요청의 파싱 결과는 메인 스레드가 수거할 때까지 붙잡지 않고 작업 스레드에서 바로 해제한다
				if (state->Cancelled.load(std::memory_order_acquire))
					state->Task.reset();
				state->Result.store(res, std::memory_order_relaxed);
				state->Completed.store(true, std::memory_order_release);
			});
	}

	void ModelImportModal::CancelAsyncPreview()
	{
		if (!m_AsyncPreview) return;

		// 완료된 결과는 즉시 해제하고, 진행 중이면 취소 표시만 남겨 완료 시점에 버린다
		m_AsyncPreview->Cancelled.store(true, std::memory_order_release);
		if (m_PreviewThread.joinable())
		{
			m_PreviewThread.join();
		}
		m_AsyncPreview.reset();
	}

	bool ModelImportModal::BeginPreviewBuild(std::unique_ptr<ModelImportTask> completedTask, bool focusCamera)
	{
		auto* scene = m_PreviewWidget.GetScene();
		if (!scene || !completedTask) return false;

		auto* prepared = completedTask->GetPreparedModel();
		if (!prepared || prepared->Nodes.empty() || prepared->Meshes.empty())
		{
			BAM_LOG(Error, "Editor", "Asset", "Model import preview failed: prepared model is empty");
			return false;
		}

		auto& manager = Engine::ResourceManager::Get();
		const auto materialRaw = manager.LoadFile(L"Resources/Material/DefaultMaterial.bammat.json");
		if (!dynamic_cast<Engine::Material*>(manager.GetResource(materialRaw)))
		{
			BAM_LOG(Error, "Editor", "Asset", "Model import preview failed: default material is unavailable");
			return false;
		}

		m_BuildingBaseMaterial = Engine::ResourceHandle<Engine::Material>(materialRaw);
		m_BuildingModelDesc = {};
		m_BuildingModelDesc.Key = L"__ModelImportPreview";
		m_BuildingModelDesc.Nodes.reserve(prepared->Nodes.size());
		m_BuildingModelDesc.Meshes.reserve(prepared->Meshes.size());
		m_TempMeshes.reserve(prepared->Meshes.size());

		for (const auto& source : prepared->Nodes)
		{
			Engine::ModelNode node;
			node.Name = Engine::StrToWStr(source.Name);
			node.ParentIndex = source.ParentIndex;
			node.MeshIndices = source.MeshIndices;

			for (int column = 0; column < 4; ++column)
			{
				for (int row = 0; row < 4; ++row)
					node.LocalTransform[column][row] = source.LocalTransform[column * 4 + row];
			}

			m_BuildingModelDesc.Nodes.push_back(std::move(node));
		}

		if (HasFlag(m_Desc.ImportOptions, EModelImportOption::Materials) && !prepared->Materials.empty())
		{
			m_AlbedoPreviewInfo.resize(prepared->Materials.size());
			for (size_t i = 0; i < prepared->Materials.size(); ++i)
			{
				const auto& source = prepared->Materials[i];
				auto& info = m_AlbedoPreviewInfo[i];
				info.MaterialName = source.Name;
				info.Reference = source.BaseColorTexture.Path;
				info.Embedded = source.BaseColorTexture.IsEmbedded;
			}

			m_BuildingModelDesc.MeshMaterialIndices.reserve(prepared->Meshes.size());
			for (const auto& mesh : prepared->Meshes)
			{
				if (mesh.MaterialIndex >= prepared->Materials.size())
				{
					BAM_LOG(Error, "Editor", "Asset", "Preview mesh references an invalid material index.");
					ClearPreview();
					return false;
				}
				m_BuildingModelDesc.MeshMaterialIndices.push_back(mesh.MaterialIndex);
			}

			m_BuildingModelDesc.Materials.reserve(prepared->Materials.size());
			m_TempMaterials.reserve(prepared->Materials.size());
			m_TempTextures.reserve(prepared->Materials.size());
		}

		m_BuildingTask = std::move(completedTask);
		m_PendingFocusCamera = focusCamera;
		m_NextBuildMeshIndex = 0;
		m_NextBuildMaterialIndex = 0;
		m_PreviewBuilding = true;
		m_PreviewProgress = 0.25f;
		return true;
	}

	bool ModelImportModal::BuildSinglePreviewMaterial(const PreparedModelData& data, size_t materialIndex)
	{
		const auto& source = data.Materials[materialIndex];
		for (const float channel : source.BaseColorFactor)
		{
			if (!std::isfinite(channel)) return false;
		}
		if (!std::isfinite(source.RoughnessFactor) || !std::isfinite(source.MetallicFactor))
			return false;

		auto* instance = Engine::MaterialInstance::Create(m_BuildingBaseMaterial);
		if (!instance) return false;

		const auto& color = source.BaseColorFactor;
		instance->SetFloat4("BaseColor", Engine::vec4(color[0], color[1], color[2], color[3]));
		instance->SetFloat("Roughness", source.RoughnessFactor);
		instance->SetFloat("Metallic", source.MetallicFactor);

		const std::wstring keyPrefix = L"__ModelImportPreview_" + std::to_wstring(reinterpret_cast<std::uintptr_t>(this));
		auto& manager = Engine::ResourceManager::Get();

		const std::wstring key = keyPrefix + L"_Material_" + std::to_wstring(materialIndex);
		auto handle = manager.AddResource<Engine::MaterialInstance>(key, instance);
		if (!handle.IsValid() || handle.Get() != instance)
			return false;

		m_TempMaterials.push_back(handle);
		m_BuildingModelDesc.Materials.emplace_back(handle);

		auto& textureInfo = m_AlbedoPreviewInfo[materialIndex];
		textureInfo.StatusKey = "ModelImport.TextureLoadFailed";

		const auto& albedo = source.BaseColorTexture;
		if (albedo.IsEmbedded)
		{
			if (albedo.EmbeddedData.empty()) return false;
			const std::wstring textureKey = keyPrefix + L"_Albedo_" + std::to_wstring(materialIndex);
			auto* texture = Engine::Texture::CreateFromEncodedMemory(albedo.EmbeddedData, textureKey);
			if (!texture) return false;

			auto textureHandle = manager.AddResource<Engine::Texture>(textureKey, texture);
			if (!textureHandle.IsValid() || textureHandle.Get() != texture) return false;

			m_TempTextures.push_back(textureHandle);
			instance->SetTextureBySlot(0, textureHandle);
		}
		else if (!albedo.Path.empty())
		{
			const auto texturePath = ModelImporter::ResolveExternalTexturePath(data.SourcePath, m_DestDir, albedo.Path);
			if (!texturePath.empty())
			{
				try
				{
					textureInfo.ResolvedPath = Engine::WStrToStr(texturePath.wstring());
					const auto rawHandle = manager.LoadFile(texturePath.wstring());
					auto* texture = dynamic_cast<Engine::Texture*>(manager.GetResource(rawHandle));
					if (texture && texture->GetRHITexture())
					{
						const Engine::ResourceHandle<Engine::Texture> textureHandle(rawHandle);
						instance->SetTextureBySlot(0, textureHandle);
					}
					else
					{
						textureInfo.ResolvedPath.clear();
					}
				}
				catch (const std::filesystem::filesystem_error&)
				{
					textureInfo.ResolvedPath.clear();
				}
			}
		}

		if (albedo.IsEmbedded || !textureInfo.ResolvedPath.empty())
			textureInfo.StatusKey = "ModelImport.TextureLoaded";
		else if (albedo.Path.empty())
			textureInfo.StatusKey = "ModelImport.TextureUnused";
		else
			textureInfo.StatusKey = "ModelImport.TextureMissing";

		return true;
	}

	void ModelImportModal::StepPreviewBuild()
	{
		if (!m_PreviewBuilding || !m_BuildingTask) return;

		auto* prepared = m_BuildingTask->GetPreparedModel();
		auto* scene = m_PreviewWidget.GetScene();
		if (!prepared || !scene)
		{
			ClearPreview();
			return;
		}

		const bool buildMaterials = HasFlag(m_Desc.ImportOptions, EModelImportOption::Materials) && !prepared->Materials.empty();
		const size_t totalMeshes = prepared->Meshes.size();
		const size_t totalMaterials = buildMaterials ? prepared->Materials.size() : 0;
		const size_t totalSteps = std::max<size_t>(1, totalMeshes + totalMaterials);

		const std::wstring keyPrefix = L"__ModelImportPreview_" + std::to_wstring(reinterpret_cast<std::uintptr_t>(this));
		auto& manager = Engine::ResourceManager::Get();

		// 프레임당 최대 4.0ms 타임 버짓 내에서만 GPU 버퍼 및 텍스처 생성 (메인 스레드 60FPS 유지)
		const auto startTime = std::chrono::steady_clock::now();
		constexpr double kFrameBudgetMs = 4.0;

		while (m_NextBuildMeshIndex < totalMeshes)
		{
			auto& source = prepared->Meshes[m_NextBuildMeshIndex];
			if (source.Positions.empty() || source.Indices.empty())
			{
				ClearPreview();
				return;
			}

			const auto vertexCount = static_cast<Engine::uint32>(source.Positions.size());
			Engine::MeshCreateDesc meshDesc{};
			meshDesc.Streams[static_cast<Engine::uint32>(Engine::EMeshStream::Position)] =
			{ source.Positions.data(), vertexCount, sizeof(Engine::VertexPosition) };
			meshDesc.Streams[static_cast<Engine::uint32>(Engine::EMeshStream::Material)] =
			{ source.Attributes.data(), vertexCount, sizeof(Engine::VertexMaterial) };
			meshDesc.IndexData = source.Indices.data();
			meshDesc.IndexCount = static_cast<Engine::uint32>(source.Indices.size());
			meshDesc.IndexStride = sizeof(Engine::uint32);
			meshDesc.BoundingBoxMin = source.BoundsMin;
			meshDesc.BoundingBoxMax = source.BoundsMax;

			auto* mesh = Engine::Mesh::Create(&meshDesc);
			if (!mesh)
			{
				ClearPreview();
				return;
			}

			const std::wstring key = keyPrefix + L"_Mesh_" + std::to_wstring(m_NextBuildMeshIndex);
			auto handle = manager.AddResource<Engine::Mesh>(key, mesh);
			if (!handle.IsValid() || handle.Get() != mesh)
			{
				ClearPreview();
				return;
			}

			m_TempMeshes.push_back(handle);
			m_BuildingModelDesc.Meshes.push_back(std::move(handle));
			++m_NextBuildMeshIndex;

			const double elapsedMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - startTime).count();
			if (elapsedMs >= kFrameBudgetMs)
				break;
		}

		while (m_NextBuildMeshIndex >= totalMeshes && m_NextBuildMaterialIndex < totalMaterials)
		{
			if (!BuildSinglePreviewMaterial(*prepared, m_NextBuildMaterialIndex))
			{
				ClearPreview();
				return;
			}
			++m_NextBuildMaterialIndex;

			const double elapsedMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - startTime).count();
			if (elapsedMs >= kFrameBudgetMs)
				break;
		}

		const size_t doneSteps = m_NextBuildMeshIndex + m_NextBuildMaterialIndex;
		m_PreviewProgress = 0.25f + 0.70f * (static_cast<float>(doneSteps) / static_cast<float>(totalSteps));

		if (m_NextBuildMeshIndex < totalMeshes || m_NextBuildMaterialIndex < totalMaterials)
			return;

		// 모든 메시와 머티리얼 준비 완료 -> 프리뷰 엔티티 인스턴스화
		const auto releaseResource = [](Engine::Resource* resource)
			{
				if (!resource) return;
				resource->Free();
				delete resource;
			};

		std::unique_ptr<Engine::Resource, decltype(releaseResource)> model(
			Engine::Model::Create(&m_BuildingModelDesc), releaseResource);
		const Engine::ResourceHandle<Engine::MaterialInterface> defaultMat(m_BuildingBaseMaterial);

		m_BuildingTask.reset();
		m_BuildingModelDesc = {};
		m_PreviewBuilding = false;

		if (!model)
		{
			ClearPreview();
			return;
		}

		const auto& previewModel = *static_cast<Engine::Model*>(model.get());
		if (scene->InstantiateModel(previewModel, defaultMat, m_PreviewEntity) != EResult::Success ||
			!UpdatePreviewTransform() ||
			!m_PreviewWidget.SetModelFocusTarget(*m_PreviewEntity, m_PendingFocusCamera))
		{
			ClearPreview();
			return;
		}

		m_PreviewProgress = 1.0f;
		m_PreviewReady = true;
	}

	bool ModelImportModal::UpdatePreviewTransform()
	{
		if (!m_PreviewEntity) return false;

		auto& registry = m_PreviewWidget.GetRegistry();
		const auto entity = m_PreviewEntity->GetEntityHandle();
		const auto* transform = registry.try_get<Engine::TransformComponent>(entity);
		if (!transform) return false;

		auto candidate = *transform;
		if (!candidate.SetLocalScale(Engine::vec3(m_Desc.UniformScale)))
			return false;

		const auto& rotation = m_Desc.RotationOffset;
		const auto rotX = glm::angleAxis(glm::radians(rotation[0]), Engine::vec3(1.0f, 0.0f, 0.0f));
		const auto rotY = glm::angleAxis(glm::radians(rotation[1]), Engine::vec3(0.0f, 1.0f, 0.0f));
		const auto rotZ = glm::angleAxis(glm::radians(rotation[2]), Engine::vec3(0.0f, 0.0f, 1.0f));
		const auto pitch = glm::angleAxis(glm::radians(m_Desc.FlipPitch ? 180.0f : 0.0f), Engine::vec3(1.0f, 0.0f, 0.0f));
		const auto heading = glm::angleAxis(glm::radians(m_Desc.FlipHeading ? 180.0f : 0.0f), Engine::vec3(0.0f, 1.0f, 0.0f));

		if (!candidate.SetLocalRotation(rotZ * rotY * rotX * heading * pitch))
			return false;

		return Engine::TransformSystem::TryCommitTransform(registry, entity, std::move(candidate));
	}

	void ModelImportModal::Update(f32 dt)
	{
		if (!m_Open)
		{
			// 닫힌 뒤 끝난 프리뷰 파싱은 결과를 버리고 상태를 해제한다
			if (m_AsyncPreview && m_AsyncPreview->Completed.load(std::memory_order_acquire))
			{
				if (m_PreviewThread.joinable())
					m_PreviewThread.join();
				m_AsyncPreview.reset();
			}
			return;
		}

		if (m_CloseRequested)
		{
			CancelAsyncPreview();
			ClearPreview();
			if (m_PreviewEntity) return;
			m_AlbedoPreviewInfo.clear();

			const bool shouldImport = m_ImportRequested;
			m_ImportRequested = false;
			m_CloseRequested = false;
			m_PreviewReloadRequested = false;
			m_OpenRequested = false;
			m_Open = false;

			if (shouldImport)
				AssetManager::Get().ImportAsync(m_SourcePath, m_DestDir, &m_Desc);

			return;
		}

		// 첫 프레임에 모달 팝업이 먼저 열리도록 m_OpenRequested가 소비된 뒤 백그라운드 로딩 시작
		// 원본 파싱은 한 번에 하나만 실행한다. 진행 중에 설정이 바뀌면 완료를 기다렸다가 새 설정으로 다시 시작한다.
		if (m_PreviewReloadRequested && !m_OpenRequested && !m_AsyncPreview)
		{
			m_PreviewReloadRequested = false;
			const bool focusCamera = m_PreviewEntity == nullptr;
			StartAsyncPreviewPrepare(focusCamera);
		}

		if (m_AsyncPreview && m_AsyncPreview->Completed.load(std::memory_order_acquire))
		{
			if (m_PreviewThread.joinable())
				m_PreviewThread.join();
			auto completed = std::move(m_AsyncPreview);
			if (m_PreviewReloadRequested || completed->Cancelled.load(std::memory_order_acquire))
			{
				// 파싱 도중 설정이 바뀌었거나 닫힌 요청의 결과다. 버리고 다음 프레임에 새 설정으로 시작한다.
			}
			else if (completed->Result.load(std::memory_order_relaxed) == EResult::Success)
			{
				BeginPreviewBuild(std::move(completed->Task), completed->FocusCamera);
			}
			else
			{
				BAM_LOG(Error, "Editor", "Asset", "Model import preview failed: {}",
					completed->Task ? completed->Task->GetErrorMessage() : "unknown error");
				ClearPreview();
			}
		}

		if (m_PreviewBuilding)
		{
			StepPreviewBuild();
		}

		if (!m_PreviewReady) return;

		m_PreviewWidget.BeginFrame(dt);
		m_PreviewWidget.Submit(dt);
	}

	void ModelImportModal::DrawPreviewPane()
	{
		if (m_PreviewReady)
		{
			m_PreviewWidget.Draw();
			return;
		}

		const ImVec2 avail = ImGui::GetContentRegionAvail();
		const ImVec2 origin = ImGui::GetCursorScreenPos();
		ImGui::GetWindowDrawList()->AddRectFilled(
			origin, ImVec2(origin.x + avail.x, origin.y + avail.y), IM_COL32(22, 24, 28, 255), 4.0f);

		const bool isLoading = m_PreviewReloadRequested || (m_AsyncPreview != nullptr) || m_PreviewBuilding;
		if (!isLoading)
		{
			EditorUI::EmptyState("ModelImport.PreviewFailed");
			return;
		}

		const float barWidth = std::clamp(avail.x * 0.65f, 140.0f, 280.0f);
		const char* pendingText = LOCAL_CSTR("ModelImport.PreviewPending");
		const ImVec2 textSize = ImGui::CalcTextSize(pendingText);
		const float blockHeight = textSize.y + ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.y * 2.0f;

		ImGui::SetCursorPosY(std::max(12.0f, (avail.y - blockHeight) * 0.5f));
		ImGui::SetCursorPosX(std::max(12.0f, (avail.x - textSize.x) * 0.5f));
		ImGui::TextDisabled("%s", pendingText);

		ImGui::Spacing();
		ImGui::SetCursorPosX(std::max(12.0f, (avail.x - barWidth) * 0.5f));
		const float displayProgress = m_PreviewBuilding
			? m_PreviewProgress
			: 0.08f + 0.15f * (0.5f + 0.5f * std::sin(static_cast<float>(ImGui::GetTime()) * 4.0f));
		ImGui::ProgressBar(displayProgress, ImVec2(barWidth, 0.0f));
	}

	void ModelImportModal::Draw()
	{
		if (!m_Open || m_CloseRequested) return;

		const std::string modalTitle = LOCAL("ModelImport.ModalTitle") + "###ModelImportOptionsModal";

		if (m_OpenRequested)
		{
			ImGui::OpenPopup(modalTitle.c_str());
			m_OpenRequested = false;
		}

		const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(880.0f, 560.0f), ImGuiCond_Appearing);

		bool popupOpen = true;
		if (ImGui::BeginPopupModal(modalTitle.c_str(), &popupOpen, ImGuiWindowFlags_NoScrollbar))
		{
			const bool canImport = HasFlag(m_Desc.ImportOptions, EModelImportOption::Meshes);
			EditorUI::DialogAction actions[] = {
				{ "EditorCommon.Cancel" }, { "ModelImport.ImportBtn", canImport }
			};
			const float footerHeight = EditorUI::GetDialogButtonBarHeight(actions, IM_ARRAYSIZE(actions)) +
				ImGui::GetStyle().ItemSpacing.y * 5.0f + 1.0f;
			const float height = std::max(1.0f, ImGui::GetContentRegionAvail().y - footerHeight);
			const auto previewFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
			Editor::DrawSidebarLayout("##ImportPreviewPane", "##ImportModalSplitter", "##ImportOptionsPane",
				m_SplitterWidth, height, 280.0f, 360.0f,
				[this] { DrawPreviewPane(); },
				[this]
				{
					const std::string sourceName = Engine::WStrToStr(m_SourcePath.filename().wstring());
					const std::string sourcePath = Engine::WStrToStr(m_SourcePath.wstring());

					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 1.0f, 1.0f));
					ImGui::TextWrapped(ICON_FA_CUBE " %s", sourceName.c_str());
					ImGui::PopStyleColor();
					ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
					ImGui::TextWrapped("%s", sourcePath.c_str());
					ImGui::PopStyleColor();

					ImGui::Spacing();
					ImGui::Separator();
					ImGui::Spacing();

					if (ImGui::CollapsingHeader(LOCAL_LABEL("ModelImport.Transform").c_str(), ImGuiTreeNodeFlags_DefaultOpen))
						DrawTransformOptions();

					ImGui::Spacing();

					// Meshes & Materials 옵션
					if (ImGui::CollapsingHeader(LOCAL_LABEL("ModelImport.MeshesAndMaterials").c_str(), ImGuiTreeNodeFlags_DefaultOpen))
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

							m_PreviewReloadRequested = true;
						}
						ImGui::BeginDisabled(!importMats);
						ImGui::Checkbox(LOCAL_LABEL("ModelImport.OverwriteMaterials").c_str(), &m_Desc.OverwriteMaterials);
						ImGui::Checkbox(LOCAL_LABEL("ModelImport.BakeTexturesToDDS").c_str(), &m_Desc.BakeTexturesToDDS);
						if (ImGui::IsItemHovered())
						{
							ImGui::SetTooltip("%s", LOCAL_CSTR("ModelImport.BakeTexturesTooltip"));
						}
						ImGui::BeginDisabled(!m_Desc.BakeTexturesToDDS);
						const std::string modeLabels[] = {
							LOCAL("ModelImport.CompressionCpuStandard"), LOCAL("ModelImport.CompressionCpuFast"),
							LOCAL("ModelImport.CompressionGpu")
						};
						const char* modes[] = { modeLabels[0].c_str(), modeLabels[1].c_str(), modeLabels[2].c_str() };
						int mode = static_cast<int>(m_Desc.TextureCompression);
						if (ImGui::Combo(LOCAL_LABEL("ModelImport.TextureCompression").c_str(), &mode, modes, IM_ARRAYSIZE(modes)))
							m_Desc.TextureCompression = static_cast<ETextureCompressionMode>(mode);
						if (ImGui::IsItemHovered())
							ImGui::SetTooltip("%s", LOCAL_CSTR("ModelImport.TextureCompressionTooltip"));
						ImGui::EndDisabled();
						ImGui::EndDisabled();
					}

					ImGui::Spacing();

					DrawTextureReferences();
					ImGui::Spacing();

					// Animation & Rig 옵션
					if (ImGui::CollapsingHeader(LOCAL_LABEL("ModelImport.AnimationAndRig").c_str(), ImGuiTreeNodeFlags_DefaultOpen))
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

					ImGui::Spacing();

					// Auto LOD Generation 옵션
					if (ImGui::CollapsingHeader(LOCAL_LABEL("ModelImport.LODOptions").c_str(), ImGuiTreeNodeFlags_DefaultOpen))
					{
						ImGui::Checkbox(LOCAL_LABEL("ModelImport.GenerateLODs").c_str(), &m_Desc.GenerateLODs);
						if (m_Desc.GenerateLODs)
						{
							ImGui::Indent();
							ImGui::SliderFloat(LOCAL_LABEL("ModelImport.LOD1Reduction").c_str(), &m_Desc.LOD1Reduction, 0.10f, 0.90f, "%.2f");
							ImGui::SliderFloat(LOCAL_LABEL("ModelImport.LOD2Reduction").c_str(), &m_Desc.LOD2Reduction, 0.05f, 0.50f, "%.2f");
							ImGui::SliderFloat(LOCAL_LABEL("ModelImport.TargetError").c_str(), &m_Desc.TargetError, 0.005f, 0.10f, "%.3f");
							ImGui::Unindent();
						}
					}

					ImGui::Spacing();

					// Phase 6: Folder Organization (하이브리드 에셋 폴더 정리)
					if (ImGui::CollapsingHeader(LOCAL_LABEL("ModelImport.FolderOrganization").c_str(), ImGuiTreeNodeFlags_DefaultOpen))
					{
						ImGui::Checkbox(LOCAL_LABEL("ModelImport.CreateModelFolder").c_str(), &m_Desc.CreateModelSubfolder);
						if (m_Desc.CreateModelSubfolder)
						{
							ImGui::Indent();
							ImGui::Checkbox(LOCAL_LABEL("ModelImport.SubfolderMeshes").c_str(), &m_Desc.UseMeshSubfolder);
							ImGui::Checkbox(LOCAL_LABEL("ModelImport.SubfolderMaterials").c_str(), &m_Desc.UseMaterialSubfolder);
							ImGui::Checkbox(LOCAL_LABEL("ModelImport.SubfolderTextures").c_str(), &m_Desc.UseTextureSubfolder);
							ImGui::Checkbox(LOCAL_LABEL("ModelImport.SubfolderAnimations").c_str(), &m_Desc.UseAnimationSubfolder);
							ImGui::Unindent();
						}

						const auto targetLayout = Engine::ModelSubfolderHelper::ResolveLayout(
							m_DestDir, m_SourcePath.stem().wstring(),
							m_Desc.CreateModelSubfolder,
							m_Desc.UseMeshSubfolder, m_Desc.UseMaterialSubfolder,
							m_Desc.UseTextureSubfolder, m_Desc.UseAnimationSubfolder);

						const std::string previewPath = Engine::WStrToStr(targetLayout.ModelFilePath.wstring());
						ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
						ImGui::TextWrapped("%s: %s", LOCAL_CSTR("ModelImport.DestinationPath"), previewPath.c_str());
						ImGui::PopStyleColor();
						if (m_RedirectedFromReadOnly)
						{
							ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.78f, 0.25f, 1.0f));
							ImGui::TextWrapped(ICON_FA_TRIANGLE_EXCLAMATION " %s", LOCAL_CSTR("ContentBrowser.ReadOnlyProtected"));
							ImGui::PopStyleColor();
						}
					}
				}, previewFlags, /*reserveHalfForContent=*/false);

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			actions[1].Enabled = HasFlag(m_Desc.ImportOptions, EModelImportOption::Meshes);
			const int action = EditorUI::DialogButtonBar("ModelImportActions", actions, IM_ARRAYSIZE(actions));
			if (action >= 0)
			{
				m_ImportRequested = action == 1;
				m_CloseRequested = true;
				ImGui::CloseCurrentPopup();
			}

			if (!popupOpen)
				m_CloseRequested = true;

			ImGui::EndPopup();
		}
		else
		{
			m_CloseRequested = true;
		}
	}

	void ModelImportModal::DrawTransformOptions()
	{
		ImGui::TextWrapped("%s", LOCAL_CSTR("ModelImportModal.DccPreset"));
		const std::string presetLabelStrings[] = {
			LOCAL("ModelImportModal.PresetCustom"),
			LOCAL("ModelImportModal.PresetBlender"),
			LOCAL("ModelImportModal.PresetUnreal"),
			LOCAL("ModelImportModal.PresetUnity")
		};
		const char* presetLabels[] = {
			presetLabelStrings[0].c_str(),
			presetLabelStrings[1].c_str(),
			presetLabelStrings[2].c_str(),
			presetLabelStrings[3].c_str()
		};
		int currentPreset = static_cast<int>(m_Desc.DccPreset);
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (ImGui::Combo("###ModelImport.DccPresetCombo", &currentPreset, presetLabels, IM_ARRAYSIZE(presetLabels)))
		{
			const auto prevFlags = m_Desc.PostProcessFlags;
			ApplyDccPreset(static_cast<EModelImportDccPreset>(currentPreset));
			UpdatePreviewTransform();
			if (m_PreviewEntity)
				m_PreviewWidget.SetModelFocusTarget(*m_PreviewEntity, true);
			if (m_Desc.PostProcessFlags != prevFlags)
				m_PreviewReloadRequested = true;
		}

		// One-click quick DCC preset buttons
		const struct { const char* LabelKey; EModelImportDccPreset Preset; } quickPresets[] = {
			{ "ModelImportModal.QuickPresetBlender", EModelImportDccPreset::Blender },
			{ "ModelImportModal.QuickPresetUnreal", EModelImportDccPreset::Unreal },
			{ "ModelImportModal.QuickPresetUnity", EModelImportDccPreset::Unity }
		};
		const auto& style = ImGui::GetStyle();
		const float presetRightEdge = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
		for (size_t i = 0; i < IM_ARRAYSIZE(quickPresets); ++i)
		{
			const auto label = LOCAL_LABEL(quickPresets[i].LabelKey);
			const float buttonWidth = ImGui::CalcTextSize(label.c_str(), nullptr, true).x + style.FramePadding.x * 2.0f;
			if (i > 0 && ImGui::GetItemRectMax().x + style.ItemSpacing.x + buttonWidth <= presetRightEdge)
				ImGui::SameLine();
			const bool isSelected = (m_Desc.DccPreset == quickPresets[i].Preset);
			if (isSelected)
				ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
			if (ImGui::SmallButton(label.c_str()))
			{
				const auto prevFlags = m_Desc.PostProcessFlags;
				ApplyDccPreset(quickPresets[i].Preset);
				UpdatePreviewTransform();
				if (m_PreviewEntity)
					m_PreviewWidget.SetModelFocusTarget(*m_PreviewEntity, true);
				if (m_Desc.PostProcessFlags != prevFlags)
					m_PreviewReloadRequested = true;
			}
			if (isSelected)
				ImGui::PopStyleColor();
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::TextWrapped("%s", LOCAL_CSTR("ModelImport.Scale"));
		ImGui::SetNextItemWidth(-FLT_MIN);
		bool transformChanged = ImGui::DragFloat("###ModelImport.Scale", &m_Desc.UniformScale, 0.01f, 0.0001f, 1000.0f,
			"%.4f", ImGuiSliderFlags_AlwaysClamp);
		bool focusTargetRequested = false;
		if (transformChanged)
			m_Desc.DccPreset = EModelImportDccPreset::Custom;

		struct ScalePreset
		{
			const char* LabelKey;
			float Scale;
		};
		constexpr ScalePreset presets[] = {
			{ "ModelImport.ScalePoint01", 0.01f }, { "ModelImport.Scale1", 1.0f }, { "ModelImport.Scale100", 100.0f }
		};
		const float rightEdge = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
		for (const auto& preset : presets)
		{
			const auto label = LOCAL_LABEL(preset.LabelKey);
			const float buttonWidth = ImGui::CalcTextSize(label.c_str(), nullptr, true).x + style.FramePadding.x * 2.0f;
			if (&preset != &presets[0] && ImGui::GetItemRectMax().x + style.ItemSpacing.x + buttonWidth <= rightEdge)
				ImGui::SameLine();

			if (ImGui::SmallButton(label.c_str()))
			{
				m_Desc.UniformScale = preset.Scale;
				m_Desc.DccPreset = EModelImportDccPreset::Custom;
				transformChanged = true;
				focusTargetRequested = true;
			}
		}

		ImGui::Spacing();
		ImGui::TextWrapped("%s", LOCAL_CSTR("ModelImport.Rotation"));
		constexpr const char* axisIds[] = { "##RotationX", "##RotationY", "##RotationZ" };
		constexpr const char* axisFormats[] = { "X %.1f deg", "Y %.1f deg", "Z %.1f deg" };
		const float axisWidth = ImGui::CalcTextSize("X -360.0 deg").x + style.FramePadding.x * 2.0f;
		const bool axesFitOnOneLine = ImGui::GetContentRegionAvail().x >= axisWidth * 3.0f + style.ItemInnerSpacing.x * 2.0f;
		for (size_t axis = 0; axis < m_Desc.RotationOffset.size(); ++axis)
		{
			if (axesFitOnOneLine && axis != 0)
				ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
			ImGui::SetNextItemWidth(axesFitOnOneLine ? axisWidth : -FLT_MIN);
			const bool rotChanged = ImGui::DragFloat(axisIds[axis], &m_Desc.RotationOffset[axis], 1.0f, -360.0f, 360.0f, axisFormats[axis]);
			if (rotChanged)
			{
				m_Desc.DccPreset = EModelImportDccPreset::Custom;
				transformChanged = true;
			}
		}

		// Quick-rotate buttons (Pitch +90, Pitch 180, Yaw 180, Reset)
		const float quickRotRightEdge = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
		const auto pitch90Label = LOCAL("ModelImportModal.PitchPlus90") + "###PitchPlus90Btn";
		const auto pitch180Label = LOCAL("ModelImport.Pitch180") + "###Pitch180Btn";
		const auto yaw180Label = LOCAL("ModelImport.Yaw180") + "###Yaw180Btn";
		const auto resetLabel = LOCAL("ModelImport.Reset") + "###TransformResetBtn";

		if (ImGui::SmallButton(pitch90Label.c_str()))
		{
			float nextPitch = m_Desc.RotationOffset[0] + 90.0f;
			if (nextPitch > 180.0f) nextPitch -= 360.0f;
			m_Desc.RotationOffset[0] = nextPitch;
			m_Desc.DccPreset = EModelImportDccPreset::Custom;
			transformChanged = true;
		}

		const auto sameLineIfFits = [&style, quickRotRightEdge](const std::string& label)
			{
				const float width = ImGui::CalcTextSize(label.c_str(), nullptr, true).x + style.FramePadding.x * 2.0f;
				if (ImGui::GetItemRectMax().x + style.ItemSpacing.x + width <= quickRotRightEdge)
					ImGui::SameLine();
			};

		sameLineIfFits(pitch180Label);
		const bool pitchActive = m_Desc.FlipPitch;
		if (pitchActive)
			ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
		if (ImGui::SmallButton(pitch180Label.c_str()))
		{
			m_Desc.FlipPitch = !m_Desc.FlipPitch;
			m_Desc.DccPreset = EModelImportDccPreset::Custom;
			transformChanged = true;
		}
		if (pitchActive)
			ImGui::PopStyleColor();

		sameLineIfFits(yaw180Label);
		const bool yawActive = m_Desc.FlipHeading;
		if (yawActive)
			ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
		if (ImGui::SmallButton(yaw180Label.c_str()))
		{
			m_Desc.FlipHeading = !m_Desc.FlipHeading;
			m_Desc.DccPreset = EModelImportDccPreset::Custom;
			transformChanged = true;
		}
		if (yawActive)
			ImGui::PopStyleColor();

		sameLineIfFits(resetLabel);
		if (ImGui::SmallButton(resetLabel.c_str()))
		{
			m_Desc.UniformScale = 1.0f;
			m_Desc.RotationOffset = {};
			m_Desc.FlipPitch = false;
			m_Desc.FlipHeading = false;
			m_Desc.DccPreset = EModelImportDccPreset::Custom;
			transformChanged = true;
			focusTargetRequested = true;
		}

		if (transformChanged)
			UpdatePreviewTransform();
		if (focusTargetRequested && m_PreviewEntity)
			m_PreviewWidget.SetModelFocusTarget(*m_PreviewEntity, true);

		ImGui::Spacing();
		ImGui::Separator();
		const auto drawPostProcessOption = [this](const char* labelKey, EModelPostProcessFlag flag)
			{
				bool enabled = HasFlag(m_Desc.PostProcessFlags, flag);
				if (!ImGui::Checkbox(LOCAL_LABEL(labelKey).c_str(), &enabled))
					return false;

				if (enabled) m_Desc.PostProcessFlags |= flag;
				else m_Desc.PostProcessFlags &= ~flag;
				m_Desc.DccPreset = EModelImportDccPreset::Custom;
				m_PreviewReloadRequested = true;
				return true;
			};
		if (drawPostProcessOption("ModelImport.LeftHanded", EModelPostProcessFlag::MakeLeftHanded))
		{
			if (HasFlag(m_Desc.PostProcessFlags, EModelPostProcessFlag::MakeLeftHanded))
				m_Desc.PostProcessFlags |= EModelPostProcessFlag::FlipWindingOrder;
			else
				m_Desc.PostProcessFlags &= ~EModelPostProcessFlag::FlipWindingOrder;
		}
		drawPostProcessOption("ModelImport.FlipUVs", EModelPostProcessFlag::FlipUVs);
		drawPostProcessOption("ModelImport.FlipWinding", EModelPostProcessFlag::FlipWindingOrder);
	}

	void ModelImportModal::DrawTextureReferences()
	{
		if (!ImGui::CollapsingHeader(LOCAL_LABEL("ModelImport.AlbedoReferences").c_str(),
			ImGuiTreeNodeFlags_DefaultOpen))
			return;

		if (m_PreviewReloadRequested || m_AsyncPreview || m_PreviewBuilding)
		{
			ImGui::TextWrapped("%s", LOCAL_CSTR("ModelImport.PreviewPending"));
			return;
		}

		if (!HasFlag(m_Desc.ImportOptions, EModelImportOption::Materials))
		{
			ImGui::TextWrapped("%s", LOCAL_CSTR("ModelImport.TextureMaterialsDisabled"));
			return;
		}

		if (!m_PreviewReady)
			ImGui::TextWrapped("%s", LOCAL_CSTR("ModelImport.PreviewFailed"));

		if (m_AlbedoPreviewInfo.empty())
		{
			ImGui::TextWrapped("%s", LOCAL_CSTR("ModelImport.TextureInfoEmpty"));
			return;
		}

		for (size_t i = 0; i < m_AlbedoPreviewInfo.size(); ++i)
		{
			const auto& info = m_AlbedoPreviewInfo[i];
			const std::string materialName = info.MaterialName.empty()
				? LOCAL_FMT("ModelImport.TextureMaterialFallback", i + 1) : info.MaterialName;

			ImGui::PushID(static_cast<int>(i));

			const auto flags = i == 0 ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None;
			const float labelWidth = ImGui::GetContentRegionAvail().x - ImGui::GetTreeNodeToLabelSpacing();
			const bool detailsOpen = ImGui::TreeNodeEx("##AlbedoReference", flags, "%s", materialName.c_str());
			if (ImGui::CalcTextSize(materialName.c_str()).x > labelWidth)
				ImGui::SetItemTooltip("%s", materialName.c_str());
			if (detailsOpen)
			{
				const auto drawDetails = [&info]()
					{
						const std::string reference = info.Reference.empty()
							? LOCAL("ModelImport.TextureNone") : info.Reference;

						const std::string resolved = info.Embedded
							? LOCAL("ModelImport.TextureEmbeddedLocation")
							: info.ResolvedPath.empty() ? LOCAL("ModelImport.TextureNone") : info.ResolvedPath;

						ImGui::TextWrapped("%s: %s", LOCAL_CSTR("ModelImport.TextureReference"), reference.c_str());
						ImGui::TextWrapped("%s: %s", LOCAL_CSTR("ModelImport.TextureResolved"), resolved.c_str());
						ImGui::TextWrapped("%s: %s", LOCAL_CSTR("ModelImport.TextureStatus"), LOCAL_CSTR(info.StatusKey));
					};

				const float fontSize = ImGui::GetFontSize();
				const float thumbnailSize = fontSize * 6.0f;
				const float availableWidth = ImGui::GetContentRegionAvail().x;
				const bool useColumns = availableWidth >= thumbnailSize + fontSize * 12.0f;

				if (useColumns)
				{
					if (ImGui::BeginTable("##AlbedoDetails", 2, ImGuiTableFlags_SizingFixedFit))
					{
						ImGui::TableSetupColumn("##Thumbnail", ImGuiTableColumnFlags_WidthFixed, thumbnailSize);
						ImGui::TableSetupColumn("##Details", ImGuiTableColumnFlags_WidthStretch);
						ImGui::TableNextRow();

						ImGui::TableSetColumnIndex(0);
						DrawAlbedoThumbnail(i, thumbnailSize);

						ImGui::TableSetColumnIndex(1);
						drawDetails();

						ImGui::EndTable();
					}
				}
				else
				{
					DrawAlbedoThumbnail(i, std::min(thumbnailSize, std::max(availableWidth, 1.0f)));
					ImGui::Spacing();
					drawDetails();
				}

				ImGui::TreePop();
			}

			ImGui::PopID();
		}
	}

	void ModelImportModal::DrawAlbedoThumbnail(size_t materialIndex, float maxEdge)
	{
		const auto* material = materialIndex < m_TempMaterials.size()
			? m_TempMaterials[materialIndex].Get() : nullptr;

		const auto* texture = m_PreviewReady && material ? material->GetTextureBySlot(0) : nullptr;
		auto* rhiTexture = texture ? texture->GetRHITexture() : nullptr;

		if (!rhiTexture || rhiTexture->GetDimension() != Engine::ETextureDimension::Texture2D ||
			rhiTexture->GetWidth() == 0 || rhiTexture->GetHeight() == 0)
		{
			ImGui::TextDisabled("%s", LOCAL_CSTR("ModelImport.TextureNone"));
			return;
		}

		const ImTextureID textureId = ImGuiManager::Get().GetImGuiTextureID(rhiTexture);
		if (!textureId)
		{
			ImGui::TextDisabled("%s", LOCAL_CSTR("ModelImport.TextureNone"));
			return;
		}

		const float width = static_cast<float>(rhiTexture->GetWidth());
		const float height = static_cast<float>(rhiTexture->GetHeight());

		const auto fitSize = [width, height](float edge)
			{
				const float scale = edge / std::max(width, height);
				return ImVec2(width * scale, height * scale);
			};

		ImGui::Image(textureId, fitSize(maxEdge));

		if (ImGui::IsItemHovered())
		{
			ImGui::BeginTooltip();
			ImGui::Image(textureId, fitSize(ImGui::GetFontSize() * 18.0f));
			ImGui::TextDisabled(LOCAL_CSTR("ModelImportModal.TextureDimensions"), rhiTexture->GetWidth(), rhiTexture->GetHeight());
			ImGui::EndTooltip();
		}
	}

	void ModelImportModal::CaptureState(EditorWorkspace& workspace) const
	{
		IImGui::CaptureState(workspace);
		workspace.SetSplitter("ModelImport.SplitterWidth", m_SplitterWidth);
	}

	void ModelImportModal::RestoreState(const EditorWorkspace& workspace)
	{
		IImGui::RestoreState(workspace);
		m_SplitterWidth = workspace.GetSplitter("ModelImport.SplitterWidth", 380.0f);
	}
}