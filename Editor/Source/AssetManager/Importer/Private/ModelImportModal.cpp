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
#include "ModelImportTask.h"
#include "Model.h"
#include "TransformSystem.h"
#include "BoundingBox.h"
#include "Components/Core/WorldTransformComponent.h"
#include "MaterialInstance.h"
#include "Texture.h"
#include "ImGuiManager.h"

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
		if (m_Open || m_CloseRequested || sourcePath.empty()) return;

		m_AlbedoPreviewInfo.clear();

		m_SourcePath = sourcePath;
		m_DestDir = destDir;
		m_Desc = ModelImporter::GetDefaultDesc(sourcePath);
		m_Desc.Overwrite = true;

		m_PreviewReady = false;
		m_PreviewReloadRequested = true;
		m_CloseRequested = false;
		m_ImportRequested = false;
		m_OpenRequested = true;
		m_Open = true;
	}

	void ModelImportModal::ClearPreview()
	{
		m_PreviewReady = false;

		if (m_PreviewEntity)
		{
			auto* scene = m_PreviewWidget.GetScene();
			if (!scene || !scene->DestroyEntity(*m_PreviewEntity))
			{
				BAM_LOG(Error, "Editor", "Asset", "Model import preview entity cleanup failed.");
				return;
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

	bool ModelImportModal::CreatePreviewMaterials(const PreparedModelData& data, Engine::ModelCreateDesc& desc, const Engine::ResourceHandle<Engine::Material>& baseMaterial)
	{
		if (!HasFlag(m_Desc.ImportOptions, EModelImportOption::Materials) || data.Materials.empty())
			return true;

		if (!baseMaterial.IsValid() || !baseMaterial.Get())
			return false;

		desc.MeshMaterialIndices.reserve(data.Meshes.size());

		m_AlbedoPreviewInfo.resize(data.Materials.size());

		for (size_t i = 0; i < data.Materials.size(); ++i)
		{
			const auto& source = data.Materials[i];
			auto& info = m_AlbedoPreviewInfo[i];

			info.MaterialName = source.Name;
			info.Reference = source.BaseColorTexture.Path;
			info.Embedded = source.BaseColorTexture.IsEmbedded;
		}

		for (const auto& mesh : data.Meshes)
		{
			if (mesh.MaterialIndex >= data.Materials.size())
			{
				BAM_LOG(Error, "Editor", "Asset", "Preview mesh references an invalid material index.");
				return false;
			}

			desc.MeshMaterialIndices.push_back(mesh.MaterialIndex);
		}

		desc.Materials.reserve(data.Materials.size());
		m_TempMaterials.reserve(data.Materials.size());
		m_TempTextures.reserve(data.Materials.size());

		const std::wstring keyPrefix = L"__ModelImportPreview_" +
			std::to_wstring(reinterpret_cast<std::uintptr_t>(this));

		auto& manager = Engine::ResourceManager::Get();

		for (size_t i = 0; i < data.Materials.size(); ++i)
		{
			const auto& source = data.Materials[i];

			for (const float channel : source.BaseColorFactor)
			{
				if (!std::isfinite(channel))
				{
					BAM_LOG(Error, "Editor", "Asset", "Preview material contains a non-finite base color.");
					return false;
				}
			}

			if (!std::isfinite(source.RoughnessFactor) || !std::isfinite(source.MetallicFactor))
			{
				BAM_LOG(Error, "Editor", "Asset", "Preview material contains non-finite PBR factors.");
				return false;
			}

			auto* instance = Engine::MaterialInstance::Create(baseMaterial);
			if (!instance) return false;

			const auto& color = source.BaseColorFactor;
			instance->SetFloat4("BaseColor", Engine::vec4(color[0], color[1], color[2], color[3]));
			instance->SetFloat("Roughness", source.RoughnessFactor);
			instance->SetFloat("Metallic", source.MetallicFactor);

			const std::wstring key = keyPrefix + L"_Material_" + std::to_wstring(i);
			auto handle = manager.AddResource<Engine::MaterialInstance>(key, instance);

			if (!handle.IsValid() || handle.Get() != instance)
			{
				BAM_LOG(Error, "Editor", "Asset", "Preview material registration failed.");
				return false;
			}

			m_TempMaterials.push_back(handle);
			desc.Materials.emplace_back(handle);

			auto& textureInfo = m_AlbedoPreviewInfo[i];
			textureInfo.StatusKey = "ModelImport.TextureLoadFailed";

			const auto& albedo = source.BaseColorTexture;
			if (albedo.IsEmbedded)
			{
				if (albedo.EmbeddedData.empty())
				{
					BAM_LOG(Error, "Editor", "Asset", "Embedded albedo has no supported encoded image data.");
					return false;
				}

				const std::wstring textureKey = keyPrefix + L"_Albedo_" + std::to_wstring(i);
				auto* texture = Engine::Texture::CreateFromEncodedMemory(albedo.EmbeddedData, textureKey);

				if (!texture)
				{
					BAM_LOG(Error, "Editor", "Asset", "Embedded albedo preview creation failed.");
					return false;
				}

				auto textureHandle = manager.AddResource<Engine::Texture>(textureKey, texture);
				if (!textureHandle.IsValid() || textureHandle.Get() != texture)
				{
					BAM_LOG(Error, "Editor", "Asset", "Embedded albedo preview registration failed.");
					return false;
				}

				m_TempTextures.push_back(textureHandle);
				instance->SetTextureBySlot(0, textureHandle);
			}
			else if (!albedo.Path.empty())
			{
				const auto texturePath = ModelImporter::ResolveExternalTexturePath(
					data.SourcePath, m_DestDir, albedo.Path);

				if (texturePath.empty())
				{
					BAM_LOG(Warn, "Editor", "Asset", "External albedo not found; using base color: {}", albedo.Path);
				}
				else
				{
					try
					{
						textureInfo.ResolvedPath = Engine::WStrToStr(texturePath.wstring());
						const auto rawHandle = manager.LoadFile(texturePath.wstring());
						auto* texture = dynamic_cast<Engine::Texture*>(manager.GetResource(rawHandle));

						if (!texture || !texture->GetRHITexture())
						{
							BAM_LOG(Error, "Editor", "Asset", "External albedo preview loading failed: {}",
								Engine::WStrToStr(texturePath.wstring()));
							return false;
						}

						const Engine::ResourceHandle<Engine::Texture> textureHandle(rawHandle);
						instance->SetTextureBySlot(0, textureHandle);
					}
					catch (const std::filesystem::filesystem_error& exception)
					{
						BAM_LOG(Error, "Editor", "Asset", "External albedo loading failed: {}", exception.what());
						return false;
					}
				}
			}

			if (albedo.IsEmbedded || !textureInfo.ResolvedPath.empty())
				textureInfo.StatusKey = "ModelImport.TextureLoaded";
			else if (albedo.Path.empty())
				textureInfo.StatusKey = "ModelImport.TextureUnused";
			else
				textureInfo.StatusKey = "ModelImport.TextureMissing";
		}

		return true;
	}

	void ModelImportModal::LoadPreviewModel(bool focusCamera)
	{
		m_AlbedoPreviewInfo.clear();
		ClearPreview();

		auto* scene = m_PreviewWidget.GetScene();
		if (!scene || m_PreviewEntity) return;

		const auto fail = [this](const char* stage)
			{
				BAM_LOG(Error, "Editor", "Asset", "Model import preview failed: {}", stage);
				ClearPreview();
			};

		ModelImportRequest request;
		request.SourcePath = m_SourcePath;
		request.Settings = m_Desc;
		request.Settings.SaveToFile = false;
		request.Settings.ImportOptions = EModelImportOption::Meshes;
		if (HasFlag(m_Desc.ImportOptions, EModelImportOption::Materials))
			request.Settings.ImportOptions |= EModelImportOption::Materials;
		request.Settings.UniformScale = 1.0f;
		request.Settings.RotationOffset = { 0.0f, 0.0f, 0.0f };

		ModelImportTask task(std::move(request));
		if (task.Prepare() != EResult::Success)
		{
			fail(task.GetErrorMessage().c_str());
			return;
		}

		auto* prepared = task.GetPreparedModel();
		if (!prepared || prepared->Nodes.empty() || prepared->Meshes.empty())
		{
			fail("prepared model is empty");
			return;
		}

		auto& manager = Engine::ResourceManager::Get();
		const auto materialRaw = manager.LoadFile(L"Resources/Material/GBufferMaterial.bammat.json");

		if (!dynamic_cast<Engine::Material*>(manager.GetResource(materialRaw)))
		{
			fail("default material is unavailable or has an invalid type");
			return;
		}

		const Engine::ResourceHandle<Engine::Material> baseMaterial(materialRaw);
		const Engine::ResourceHandle<Engine::MaterialInterface> material(baseMaterial);

		Engine::ModelCreateDesc modelDesc;
		modelDesc.Key = L"__ModelImportPreview";
		modelDesc.Nodes.reserve(prepared->Nodes.size());
		modelDesc.Meshes.reserve(prepared->Meshes.size());
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

			modelDesc.Nodes.push_back(std::move(node));
		}

		const std::wstring keyPrefix = L"__ModelImportPreview_" +
			std::to_wstring(reinterpret_cast<std::uintptr_t>(this));

		for (size_t i = 0; i < prepared->Meshes.size(); ++i)
		{
			auto& source = prepared->Meshes[i];
			if (source.Positions.empty() || source.Indices.empty())
			{
				fail("prepared mesh is empty");
				return;
			}

			const auto vertexCount = static_cast<Engine::uint32>(source.Positions.size());

			Engine::MeshCreateDesc meshDesc{};
			auto& positionStream = meshDesc.Streams[static_cast<Engine::uint32>(Engine::EMeshStream::Position)];
			positionStream = { source.Positions.data(), vertexCount, sizeof(Engine::VertexPosition) };

			auto& materialStream = meshDesc.Streams[static_cast<Engine::uint32>(Engine::EMeshStream::Material)];
			materialStream = { source.Attributes.data(), vertexCount, sizeof(Engine::VertexMaterial) };

			meshDesc.IndexData = source.Indices.data();
			meshDesc.IndexCount = static_cast<Engine::uint32>(source.Indices.size());
			meshDesc.IndexStride = sizeof(Engine::uint32);
			meshDesc.BoundingBoxMin = source.BoundsMin;
			meshDesc.BoundingBoxMax = source.BoundsMax;

			auto* mesh = Engine::Mesh::Create(&meshDesc);
			if (!mesh)
			{
				fail("mesh creation failed");
				return;
			}

			const std::wstring key = keyPrefix + L"_Mesh_" + std::to_wstring(i);
			auto handle = manager.AddResource<Engine::Mesh>(key, mesh);

			if (!handle.IsValid() || handle.Get() != mesh)
			{
				fail("temporary mesh registration failed");
				return;
			}

			m_TempMeshes.push_back(handle);
			modelDesc.Meshes.push_back(std::move(handle));
		}

		if (!CreatePreviewMaterials(*prepared, modelDesc, baseMaterial))
		{
			fail("preview material creation failed");
			return;
		}

		const auto releaseResource = [](Engine::Resource* resource)
			{
				if (!resource) return;
				resource->Free();
				delete resource;
			};

		std::unique_ptr<Engine::Resource, decltype(releaseResource)> model(Engine::Model::Create(&modelDesc), releaseResource);

		if (!model)
		{
			fail("temporary model creation failed");
			return;
		}

		const auto& previewModel = *static_cast<Engine::Model*>(model.get());
		if (scene->InstantiateModel(previewModel, material, m_PreviewEntity) != EResult::Success)
		{
			fail("model hierarchy placement failed");
			return;
		}

		if (!UpdatePreviewTransform())
		{
			fail("preview transform application failed");
			return;
		}

		auto& registry = m_PreviewWidget.GetRegistry();
		auto view = registry.view<Engine::StaticMeshRendererComponent, Engine::WorldTransformComponent>();

		Engine::BoundingBox bounds;
		bool hasBounds = false;

		for (auto entity : view)
		{
			const auto& renderer = view.get<Engine::StaticMeshRendererComponent>(entity);
			const auto& world = view.get<Engine::WorldTransformComponent>(entity);
			const auto* mesh = renderer.meshHandle.Get();
			if (!mesh) continue;

			const Engine::BoundingBox localBounds(mesh->GetMin(), mesh->GetMax());
			Engine::BoundingBox worldBounds;
			if (!localBounds.TryTransformToAABB(world.worldMatrix, worldBounds))
			{
				fail("preview bounds transformation failed");
				return;
			}

			if (!hasBounds)
			{
				bounds = worldBounds;
				hasBounds = true;
			}
			else
			{
				bounds.min = glm::min(bounds.min, worldBounds.min);
				bounds.max = glm::max(bounds.max, worldBounds.max);
			}
		}

		if (!hasBounds)
		{
			fail("preview contains no renderable meshes");
			return;
		}

		if (focusCamera)
			m_PreviewWidget.FocusTarget(bounds.Center(), std::max(glm::length(bounds.Extent()) * 3.0f, 2.0f));

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

		const auto rotX = glm::angleAxis(glm::radians(m_Desc.RotationOffset[0]), Engine::vec3(1.0f, 0.0f, 0.0f));
		const auto rotY = glm::angleAxis(glm::radians(m_Desc.RotationOffset[1]), Engine::vec3(0.0f, 1.0f, 0.0f));
		const auto rotZ = glm::angleAxis(glm::radians(m_Desc.RotationOffset[2]), Engine::vec3(0.0f, 0.0f, 1.0f));

		if (!candidate.SetLocalRotation(rotZ * rotY * rotX))
			return false;

		return Engine::TransformSystem::TryCommitTransform(registry, entity, std::move(candidate));
	}

	void ModelImportModal::Update(f32 dt)
	{
		if (!m_Open) return;

		if (m_CloseRequested)
		{
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

		if (m_PreviewReloadRequested)
		{
			m_PreviewReloadRequested = false;
			const bool focusCamera = m_PreviewEntity == nullptr;
			LoadPreviewModel(focusCamera);
		}

		if (!m_PreviewReady) return;

		m_PreviewWidget.BeginFrame(dt);
		m_PreviewWidget.Submit(dt);
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
			// ── [스플리터] 공통 헬퍼 적용 ──
			float optionsWidth = (std::max)(minRight, availWidth - m_SplitterWidth - 4.0f);
			Editor::SplitterV("##ImportModalSplitter", &m_SplitterWidth, &optionsWidth, minLeft, minRight, availHeight);
			ImGui::SameLine();
			if (ImGui::BeginChild("##ImportOptionsPane", ImVec2(0.0f, availHeight), true))
			{
				const std::string sourceName = Engine::WStrToStr(m_SourcePath.filename().wstring());
				const std::string sourcePath = Engine::WStrToStr(m_SourcePath.wstring());

				ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), ICON_FA_CUBE " %s", sourceName.c_str());
				ImGui::TextWrapped("%s", sourcePath.c_str());

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

						m_PreviewReloadRequested = true;
					}

					bool flipUVs = HasFlag(m_Desc.PostProcessFlags, EModelPostProcessFlag::FlipUVs);
					if (ImGui::Checkbox(LOCAL_LABEL("ModelImport.FlipUVs").c_str(), &flipUVs))
					{
						if (flipUVs) m_Desc.PostProcessFlags |= EModelPostProcessFlag::FlipUVs;
						else m_Desc.PostProcessFlags &= ~EModelPostProcessFlag::FlipUVs;

						m_PreviewReloadRequested = true;
					}

					bool flipWinding = HasFlag(m_Desc.PostProcessFlags, EModelPostProcessFlag::FlipWindingOrder);
					if (ImGui::Checkbox(LOCAL_LABEL("ModelImport.FlipWinding").c_str(), &flipWinding))
					{
						if (flipWinding) m_Desc.PostProcessFlags |= EModelPostProcessFlag::FlipWindingOrder;
						else m_Desc.PostProcessFlags &= ~EModelPostProcessFlag::FlipWindingOrder;

						m_PreviewReloadRequested = true;
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

						m_PreviewReloadRequested = true;
					}
				}

				ImGui::Spacing();

				DrawTextureReferences();
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
			const bool canImport = m_PreviewReady && !m_PreviewReloadRequested &&
				HasFlag(m_Desc.ImportOptions, EModelImportOption::Meshes);

			ImGui::BeginDisabled(!canImport);

			const std::string importLabel = ICON_FA_DOWNLOAD " " + LOCAL_LABEL("ModelImport.ImportBtn");
			if (ImGui::Button(importLabel.c_str(), ImVec2(120.0f, 0.0f)))
			{
				m_ImportRequested = true;
				m_CloseRequested = true;
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndDisabled();

			ImGui::SameLine(0.0f, 10.0f);
			if (ImGui::Button(LOCAL_CSTR("EditorCommon.Cancel"), ImVec2(100.0f, 0.0f)))
			{
				m_ImportRequested = false;
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

	void ModelImportModal::DrawTextureReferences()
	{
		if (!ImGui::CollapsingHeader(LOCAL_LABEL("ModelImport.AlbedoReferences").c_str(),
			ImGuiTreeNodeFlags_DefaultOpen))
			return;

		if (m_PreviewReloadRequested)
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
			if (ImGui::TreeNodeEx("##AlbedoReference", flags, "%s", materialName.c_str()))
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
		const auto* rhiTexture = texture ? texture->GetRHITexture() : nullptr;

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
			ImGui::TextDisabled("%u x %u", rhiTexture->GetWidth(), rhiTexture->GetHeight());
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