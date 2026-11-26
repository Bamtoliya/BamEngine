#include "ThumbnailRenderer.h"
#include "MaterialPreviewRenderer.h"
#include "ComponentEnable.h"
#include "Scene.h"
#include "TransformComponent.h"
#include "WorldTransformComponent.h"
#include "FlagComponent.h"
#include "StaticMeshRendererComponent.h"
#include "SkinnedMeshRendererComponent.h"
#include "Entity.h"
#include "LightComponent.h"
#include "TransformSystem.h"
#include "LightSystem.h"
#include "ResourceStreamer.h"
#include "ResourceManager.h"
#include "RenderTargetManager.h"
#include "RenderTarget.h"
#include "Renderer.h"
#include "Shader.h"
#include "RenderPassManager.h"
#include "RenderPass.h"
#include "EditorSettingsService.h"
#include "PreviewCameraDefaults.h"
#include "AnimationEvaluator.h"
#include "MaterialInstance.h"
#include "PlatformPath.h"
#include <archive/BeveArchive.h>
#include <archive/JsonArchive.h>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <limits>

namespace
{
	constexpr const wchar_t* DefaultMaterialKey = L"Resources/Material/DefaultMaterial.bammat.json";
	constexpr const wchar_t* ThumbnailMaterialKey = L"Editor.ThumbnailPreviewMaterial";
	constexpr float ThumbnailMetallic = 0.2f;
}

IMPLEMENT_SINGLETON(ThumbnailRenderer)

EResult ThumbnailRenderer::Initialize(void* arg)
{
	auto& manager = Engine::ResourceManager::Get();
	const auto vs = manager.GetResourceHandle<Engine::Shader>(L"FullscreenQuadVS");
	const auto ps = manager.GetResourceHandle<Engine::Shader>(L"LightingPS");

	if (!vs.Get() || !ps.Get())
		return EResult::FileNotFound;

	if (!vs->GetRHIShader() || !ps->GetRHIShader())
		return EResult::Fail;

	m_SphereMesh = manager.GetResourceHandle<Engine::Mesh>(L"SphereMesh");
	if (!m_SphereMesh.Get())
		return EResult::FileNotFound;

	m_MaterialBallMesh = Editor::MaterialPreviewRenderer::GetOrLoadMaterialBallPreviewMesh();

	auto baseMaterial = manager.GetResourceHandle<Engine::Material>(DefaultMaterialKey);
	if (!baseMaterial.IsValid())
	{
		auto loaded = manager.LoadFile(DefaultMaterialKey);
		if (loaded.IsValid()) baseMaterial = Engine::ResourceHandle<Engine::Material>(loaded);
	}
	if (!baseMaterial.Get())
		return EResult::FileNotFound;

	m_PreviewMaterial = manager.GetResourceHandle<Engine::MaterialInterface>(ThumbnailMaterialKey);
	if (!m_PreviewMaterial.IsValid())
	{
		auto* instance = Engine::MaterialInstance::Create(baseMaterial);
		if (!instance)
			return EResult::Fail;

		instance->AddFloat("Metallic", ThumbnailMetallic);
		m_PreviewMaterial = manager.AddResource<Engine::MaterialInterface>(ThumbnailMaterialKey, instance);
		if (!m_PreviewMaterial.Get())
			return EResult::Fail;
	}

	SetupStudioScene();
	if (m_Scene == nullptr || m_MeshEntity == entt::null)
	{
		Free();
		return EResult::Fail;
	}

	if (!m_Pipeline.Initialize(L"ThumbPreview", m_Size, m_Size, false))
	{
		Free();
		return EResult::Fail;
	}

	m_PostProcess.Initialize(L"ThumbPreview");
	m_PostProcess.PrepareResources(m_Size, m_Size);

	return EResult::Success;
}

void ThumbnailRenderer::Free()
{
	m_PostProcess.Free();
	m_OutputRTName.clear();

	if (!m_Pipeline.GetOwnedRTNames().empty())
		m_Pipeline.Free();

	Engine::Safe_Release(m_Scene);
	m_Scene = nullptr;

	m_MeshEntity = entt::null;
	m_MaterialBallMesh = {};
	m_SphereMesh = {};
	m_PreviewMaterial = {};
}

void ThumbnailRenderer::SetupStudioScene()
{
	if (m_Scene) return;
	Engine::SceneCreateDesc desc{};
	desc.name = L"ThumbnailPreview_Scene";
	m_Scene = Engine::Scene::Create(&desc);
	if (!m_Scene) return;
	auto& registry = m_Scene->GetRegistry();
	// 방향성 조명 생성 헬퍼 람다
	auto createDirLight = [&](const vec3& euler, const vec3& color, bool specular) -> entt::entity
		{
			entt::entity e = registry.create();
			auto& trans = registry.emplace<Engine::TransformComponent>(e);
			registry.emplace<Engine::WorldTransformComponent>(e);
			auto& flag = registry.emplace<Engine::FlagComponent>(e);
			flag.flags = Engine::EEntityFlag::Active | Engine::EEntityFlag::Visible;
			auto& comp = registry.emplace<Engine::LightComponent>(e);
			comp.LightType = Engine::ELightType::Directional;
			comp.Color = color;
			comp.Flags = Engine::ELightFlags::UseInDeferredRendering | Engine::ELightFlags::AffectDiffuse;
			if (specular) comp.Flags |= Engine::ELightFlags::AffectSpecular;
			(void)trans.SetEulerRotation(euler);
			return e;
		};
	// 1. 스튜디오 3점 조명(Key, Fill, Rim Light) 배치
	// 1) Key Light (주광): 카메라 시선(정면 우측 상단)에서 모델 앞면을 화사하게 비춤
	m_StudioLights[0] = createDirLight(vec3(30.0f, 195.0f, 0.0f), vec3(1.0f, 0.98f, 0.95f), true);
	m_StudioLights[1] = createDirLight(vec3(15.0f, 120.0f, 0.0f), vec3(0.85f, 0.92f, 1.0f), false);
	m_StudioLights[2] = createDirLight(vec3(45.0f, 20.0f, 0.0f), vec3(0.95f, 0.98f, 1.0f), true);
	// 2. 프리뷰 대상 구체(Sphere) 메쉬 엔티티 생성
	m_MeshEntity = registry.create();
	registry.emplace<Engine::TransformComponent>(m_MeshEntity);
	registry.emplace<Engine::WorldTransformComponent>(m_MeshEntity);
	auto& meshFlag = registry.emplace<Engine::FlagComponent>(m_MeshEntity);
	meshFlag.flags = Engine::EEntityFlag::Active | Engine::EEntityFlag::Visible;
	auto& meshComp = registry.emplace<Engine::StaticMeshRendererComponent>(m_MeshEntity);
	meshComp.meshHandle = m_MaterialBallMesh.IsValid() ? m_MaterialBallMesh : m_SphereMesh;
	meshComp.castShadow = true;
	// 계층 구조 및 라이트 GPU 버퍼 캐시 갱신
	RefreshStudioLights();
}

void ThumbnailRenderer::RefreshStudioLights()
{
	auto& registry = m_Scene->GetRegistry();
	Engine::TransformSystem::UpdateHierarchy(registry);

	const auto& intensities = EditorSettingsService::Get().GetPreferences().PreviewLightIntensity;
	for (std::size_t i = 0; i < m_StudioLights.size(); ++i)
	{
		if (auto* light = registry.try_get<Engine::LightComponent>(m_StudioLights[i]))
			light->Intensity = intensities[i];
	}

	Engine::LightSystem::UpdateLights(registry);
}

bool ThumbnailRenderer::SubmitMaterial(Engine::MaterialInterface* material)
{
	if (material == nullptr) return false;
	Engine::TransformSystem::UpdateHierarchy(m_Scene->GetRegistry());
	Engine::LightSystem::UpdateLights(m_Scene->GetRegistry());

	// 1. 머티리얼이 GeometryPass(디퍼드 지오메트리 렌더링)용 셰이더를 지원하는지 검증
	// (SpriteMaterial, UIMaterial, SkyMaterial 또는 손상된 머티리얼은 GeometryPass 불가)
	auto* vs = material->GetVertexShader(Engine::ERenderPassType::Geometry);
	auto* ps = material->GetPixelShader(Engine::ERenderPassType::Geometry);
	if (!vs || !ps || !vs->GetRHIShader() || !ps->GetRHIShader())
		return false;

	auto& registry = m_Scene->GetRegistry();
	auto* meshComp = registry.try_get<Engine::StaticMeshRendererComponent>(m_MeshEntity);
	if (!meshComp) return false;

	Engine::Mesh* mesh = meshComp->meshHandle.Get();
	if (!mesh) return false;

	// 2. 엔진의 공식 검증 함수로 파이프라인 생성 가능 여부 최종 확인
	auto* geomPass = Engine::RenderPassManager::Get().GetRenderPassByID(m_Pipeline.GetGeometryPassID());
	if (geomPass == nullptr ||
		Engine::Renderer::Get().ValidateMeshMaterial(mesh, material, geomPass) != Engine::EResult::Success)
		return false;

	// 대상 머티리얼 바인딩
	auto handle = Engine::ResourceManager::Get().GetResourceHandle<Engine::MaterialInterface>(material->GetKey());
	meshComp->materialHandle = handle;

	// 트랜스폼 및 조명 시스템 갱신
	RefreshStudioLights();

	const auto camera = CalculateAutoFramingCamera({ mesh->GetMin(), mesh->GetMax() });
	if (!camera)
		return false;

	// 128x128 오프스크린 파이프라인에 카메라 버퍼 및 씬 등록
	auto* transComp = registry.try_get<Engine::WorldTransformComponent>(m_MeshEntity);
	if (transComp == nullptr)
		return false;

		SubmitFrame(*camera);

	Engine::StaticDrawCommand cmd{};
	cmd.mesh = mesh;
	if (material) Engine::ResourceStreamer::EnsureMaterialTextures(*material);
	cmd.material = material;
	cmd.worldMatrix = transComp->worldMatrix;
	cmd.pickingId = 0;
	Engine::Renderer::Get().SubmitStatic(cmd, m_Pipeline.GetGeometryPassID());

	return true;
}

Engine::MaterialInterface* ThumbnailRenderer::GetDefaultPreviewMaterial() const
{
	return m_PreviewMaterial.Get();
}

std::optional<CameraBuffer> ThumbnailRenderer::CalculateAutoFramingCamera(const Engine::BoundingBox& bounds) const
{
	constexpr float fieldOfView = Editor::PreviewCameraDefaults::FieldOfViewDegrees;
	const auto framing = Editor::PreviewCameraDefaults::CalculateFraming(bounds, 1.0f, fieldOfView,
		Editor::PreviewCameraDefaults::ThumbnailFramingMargin);
	if (!framing)
		return std::nullopt;

	const vec3 eye = framing->Center - Editor::PreviewCameraDefaults::GetForward() * framing->Distance;
	const mat4 view = glm::lookAtLH(eye, framing->Center, vec3(0.0f, 1.0f, 0.0f));
	const mat4 proj = glm::perspectiveLH_ZO(glm::radians(fieldOfView), 1.0f, framing->NearPlane, framing->FarPlane);
	return CameraBuffer::FromMatrices(view, proj, eye, 0.0f);
}
void ThumbnailRenderer::SubmitFrame(const CameraBuffer& camera)
{
	m_Pipeline.Submit(camera, m_Scene);
	m_OutputRTName = m_PostProcess.Execute(0.0f, m_Pipeline.GetFinalColorRTName());
}
bool ThumbnailRenderer::SubmitMesh(Engine::Mesh* mesh, Engine::MaterialInterface* material)
{
	if (mesh == nullptr) return false;
	RefreshStudioLights();
	if (!material)
	{
		auto& rm = Engine::ResourceManager::Get();
		auto skinMat = rm.GetResourceHandle<Engine::MaterialInterface>(L"Resources/Material/SkinningMaterial.bammat.json");
		if (mesh->IsSkinned() && skinMat.IsValid()) material = skinMat.Get();
		else material = GetDefaultPreviewMaterial();
	}
	if (!material) return false;
	auto* geomPass = Engine::RenderPassManager::Get().GetRenderPassByID(m_Pipeline.GetGeometryPassID());
	if (geomPass == nullptr ||
		Engine::Renderer::Get().ValidateMeshMaterial(mesh, material, geomPass) != Engine::EResult::Success)
		return false;

	m_BindPoseMatrices.clear();
	Engine::BoundingBox renderBounds(mesh->GetMin(), mesh->GetMax());

	if (mesh->IsSkinned())
	{
		auto& rm = Engine::ResourceManager::Get();
		const std::filesystem::path meshPath(mesh->GetPath().empty() ? mesh->GetKey() : mesh->GetPath());
		if (!meshPath.empty())
		{
			const auto parentDir = meshPath.parent_path();
			const auto stem = meshPath.stem().wstring();
			const auto underscorePos = stem.find(L'_');
			const auto modelPrefix = underscorePos != std::wstring::npos ? stem.substr(0, underscorePos) : stem;

			const auto candidatePath = parentDir / (modelPrefix + L"_Skeleton.bamskel");
			auto skelHandle = rm.GetResourceHandle<Engine::Skeleton>(candidatePath.wstring());
			if (!skelHandle.IsValid())
			{
				auto loaded = rm.LoadFile(candidatePath.wstring());
				if (loaded.IsValid()) skelHandle = Engine::ResourceHandle<Engine::Skeleton>(loaded);
			}

			if (skelHandle.IsValid() && skelHandle.Get() && mesh->IsSkinCompatibleWith(*skelHandle.Get()))
			{
				Engine::AnimationEvaluator::EvaluateBindPoseSkinningMatrices(*skelHandle.Get(), m_BindPoseMatrices);
				if (!m_BindPoseMatrices.empty())
				{
					Engine::BoundingBox transformedBounds;
					if (renderBounds.TryTransformToAABB(m_BindPoseMatrices[0], transformedBounds))
						renderBounds = transformedBounds;
				}
			}
		}
	}

	const auto camera = CalculateAutoFramingCamera(renderBounds);
	if (!camera)
		return false;
		SubmitFrame(*camera);
	if (mesh->IsSkinned())
	{
		Engine::SkinnedDrawCommand cmd{};
		cmd.mesh = mesh;
		if (material) Engine::ResourceStreamer::EnsureMaterialTextures(*material);
		cmd.material = material;
		if (material) material->BuildMaterialConstants(cmd.materialConstants);
		cmd.worldMatrix = mat4(1.0f);
		cmd.boneMatrices = m_BindPoseMatrices.empty() ? nullptr : &m_BindPoseMatrices;
		cmd.pickingId = 0;
		Engine::Renderer::Get().SubmitSkinned(cmd, m_Pipeline.GetGeometryPassID());
	}
	else
	{
		Engine::StaticDrawCommand cmd{};
		cmd.mesh = mesh;
		if (material) Engine::ResourceStreamer::EnsureMaterialTextures(*material);
		cmd.material = material;
		cmd.worldMatrix = mat4(1.0f);
		cmd.pickingId = 0;
		Engine::Renderer::Get().SubmitStatic(cmd, m_Pipeline.GetGeometryPassID());
	}
	return true;
}
bool ThumbnailRenderer::SubmitModel(Engine::Model* model, f32 yawDegrees)
{
	if (model == nullptr) return false;
	RefreshStudioLights();
	const auto& nodes = model->GetNodes();
	const auto& meshes = model->GetMeshes();
	if (meshes.empty()) return false;
	Engine::TransformComponent rootTransform{};
	if (!rootTransform.SetEulerRotation(model->GetPlacementEulerDegrees()))
		return false;

	const mat4 yawMatrix = (yawDegrees != 0.0f)
		? glm::rotate(mat4(1.0f), glm::radians(yawDegrees), vec3(0.0f, 1.0f, 0.0f))
		: mat4(1.0f);
	const mat4 placementMatrix = yawMatrix * rootTransform.GetLocalMatrix();
	std::vector<mat4> worldTransforms(nodes.size(), mat4(1.0f));
	for (size_t i = 0; i < nodes.size(); ++i)
	{
		const auto& node = nodes[i];
		worldTransforms[i] = (node.ParentIndex >= 0 && static_cast<size_t>(node.ParentIndex) < i)
			? worldTransforms[node.ParentIndex] * node.LocalTransform : placementMatrix * node.LocalTransform;
	}
	Engine::BoundingBox bounds;
	bool hasValidBounds = false;
	for (size_t i = 0; i < nodes.size(); ++i)
	{
		for (uint32 meshIdx : nodes[i].MeshIndices)
		{
			const auto* mesh = meshIdx < meshes.size() ? meshes[meshIdx].Get() : nullptr;
			if (!mesh)
				continue;

			Engine::BoundingBox worldBounds;
			if (!Engine::BoundingBox(mesh->GetMin(), mesh->GetMax()).TryTransformToAABB(worldTransforms[i], worldBounds))
				return false;

			bounds.min = hasValidBounds ? glm::min(bounds.min, worldBounds.min) : worldBounds.min;
			bounds.max = hasValidBounds ? glm::max(bounds.max, worldBounds.max) : worldBounds.max;
			hasValidBounds = true;
		}
	}
	if (!hasValidBounds)
		return false;
	const auto camera = CalculateAutoFramingCamera(bounds);
	if (!camera)
		return false;
	auto* defaultMat = GetDefaultPreviewMaterial();
	auto& rm = Engine::ResourceManager::Get();
	auto skinMatHandle = rm.GetResourceHandle<Engine::MaterialInterface>(L"Resources/Material/SkinningMaterial.bammat.json");
	if (!skinMatHandle.IsValid())
	{
		auto loaded = rm.LoadFile(L"Resources/Material/SkinningMaterial.bammat.json");
		if (loaded.IsValid()) skinMatHandle = Engine::ResourceHandle<Engine::MaterialInterface>(loaded);
	}
	auto* geomPass = Engine::RenderPassManager::Get().GetRenderPassByID(m_Pipeline.GetGeometryPassID());
	if (geomPass == nullptr)
		return false;

	auto& renderer = Engine::Renderer::Get();
	bool submitted = false;

	m_BindPoseMatrices.clear();
	const auto skeletonHandle = model->GetSkeleton();
	if (skeletonHandle.IsValid() && skeletonHandle.Get())
	{
		Engine::AnimationEvaluator::EvaluateBindPoseSkinningMatrices(*skeletonHandle.Get(), m_BindPoseMatrices);
	}

	for (size_t i = 0; i < nodes.size(); ++i)
	{
		for (uint32 meshIdx : nodes[i].MeshIndices)
		{
			if (meshIdx >= meshes.size())
				continue;

			auto* mesh = meshes[meshIdx].Get();
			if (mesh == nullptr)
				continue;

			const auto materialHandle = model->GetMeshMaterial(meshIdx);
			auto* material = materialHandle.Get();

			if (material == nullptr)
				material = mesh->IsSkinned() && skinMatHandle.IsValid() ? skinMatHandle.Get() : defaultMat;

			if (material == nullptr ||
				renderer.ValidateMeshMaterial(mesh, material, geomPass) != Engine::EResult::Success)
				continue;

			if (!submitted)
				SubmitFrame(*camera);

			if (mesh->IsSkinned())
			{
				Engine::SkinnedDrawCommand cmd{};
				cmd.mesh = mesh;
				if (material) Engine::ResourceStreamer::EnsureMaterialTextures(*material);
				cmd.material = material;
				if (material) material->BuildMaterialConstants(cmd.materialConstants);
				cmd.worldMatrix = m_BindPoseMatrices.empty() ? worldTransforms[i] : placementMatrix;
				cmd.boneMatrices = m_BindPoseMatrices.empty() ? nullptr : &m_BindPoseMatrices;
				cmd.pickingId = 0;
				renderer.SubmitSkinned(cmd, m_Pipeline.GetGeometryPassID());
			}
			else
			{
				Engine::StaticDrawCommand cmd{};
				cmd.mesh = mesh;
				if (material) Engine::ResourceStreamer::EnsureMaterialTextures(*material);
				cmd.material = material;
				cmd.worldMatrix = worldTransforms[i];
				cmd.pickingId = 0;
				renderer.SubmitStatic(cmd, m_Pipeline.GetGeometryPassID());
			}

			submitted = true;
		}
	}

	return submitted;
}

bool ThumbnailRenderer::SubmitPrefab(const std::filesystem::path& prefabPath)
{
	std::error_code ec;
	if (prefabPath.empty() || !std::filesystem::is_regular_file(prefabPath, ec)) return false;

	RefreshStudioLights();

	std::string ext = Engine::WStrToStr(prefabPath.extension().wstring());
	for (char& ch : ext) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));

	std::unique_ptr<archive::IArchive> ar;
	if (ext == ".json" || ext == ".prefab") ar = std::make_unique<archive::JsonArchive>();
	else ar = std::make_unique<archive::BeveArchive>();

	if (!ar->LoadFromFile(prefabPath)) return false;

	struct SceneDeleter { void operator()(Engine::Scene* s) const { if (s) s->Release(); } };
	std::unique_ptr<Engine::Scene, SceneDeleter> tempScene(Engine::Scene::Create());
	if (!tempScene) return false;

	Engine::Entity* rootEntity = nullptr;
	if (!tempScene->InstantiatePrefabArchive(*ar, rootEntity, nullptr) || !rootEntity) return false;

	auto& registry = tempScene->GetRegistry();
	if (!Engine::TransformSystem::UpdateHierarchy(registry)) return false;

	Engine::BoundingBox bounds;
	bool hasValidBounds = false;
	const auto accumulateBounds = [&](const Engine::Mesh* mesh, const mat4& worldMatrix)
		{
			if (!mesh) return;
			Engine::BoundingBox worldBounds;
			if (!Engine::BoundingBox(mesh->GetMin(), mesh->GetMax()).TryTransformToAABB(worldMatrix, worldBounds)) return;
			bounds.min = hasValidBounds ? glm::min(bounds.min, worldBounds.min) : worldBounds.min;
			bounds.max = hasValidBounds ? glm::max(bounds.max, worldBounds.max) : worldBounds.max;
			hasValidBounds = true;
		};

	for (auto e : registry.view<Engine::StaticMeshRendererComponent, Engine::WorldTransformComponent>())
	{
		if (!Engine::IsComponentEnabled<Engine::StaticMeshRendererComponent>(registry, e)) continue;
		accumulateBounds(registry.get<Engine::StaticMeshRendererComponent>(e).meshHandle.Get(),
			registry.get<Engine::WorldTransformComponent>(e).worldMatrix);
	}
	for (auto e : registry.view<Engine::SkinnedMeshRendererComponent, Engine::WorldTransformComponent>())
	{
		if (!Engine::IsComponentEnabled<Engine::SkinnedMeshRendererComponent>(registry, e)) continue;
		accumulateBounds(registry.get<Engine::SkinnedMeshRendererComponent>(e).meshHandle.Get(),
			registry.get<Engine::WorldTransformComponent>(e).worldMatrix);
	}

	if (!hasValidBounds) return false;

	const auto camera = CalculateAutoFramingCamera(bounds);
	if (!camera) return false;

	auto* geomPass = Engine::RenderPassManager::Get().GetRenderPassByID(m_Pipeline.GetGeometryPassID());
	if (geomPass == nullptr) return false;

	auto* defaultMat = GetDefaultPreviewMaterial();
	auto& rm = Engine::ResourceManager::Get();
	auto skinMatHandle = rm.GetResourceHandle<Engine::MaterialInterface>(L"Resources/Material/SkinningMaterial.bammat.json");
	if (!skinMatHandle.IsValid())
	{
		auto loaded = rm.LoadFile(L"Resources/Material/SkinningMaterial.bammat.json");
		if (loaded.IsValid()) skinMatHandle = Engine::ResourceHandle<Engine::MaterialInterface>(loaded);
	}

	auto& renderer = Engine::Renderer::Get();
	bool submitted = false;

	for (auto e : registry.view<Engine::StaticMeshRendererComponent, Engine::WorldTransformComponent>())
	{
		if (!Engine::IsComponentEnabled<Engine::StaticMeshRendererComponent>(registry, e)) continue;
		const auto& mr = registry.get<Engine::StaticMeshRendererComponent>(e);
		const auto& wt = registry.get<Engine::WorldTransformComponent>(e);
		auto* mesh = mr.meshHandle.Get();
		auto* mat = mr.materialHandle.Get() ? mr.materialHandle.Get() : defaultMat;
		if (!mesh || !mat || renderer.ValidateMeshMaterial(mesh, mat, geomPass) != Engine::EResult::Success) continue;

		if (!submitted) SubmitFrame(*camera);
		Engine::StaticDrawCommand cmd{};
		cmd.mesh = mesh;
		if (mat) Engine::ResourceStreamer::EnsureMaterialTextures(*mat);
		cmd.material = mat;
		cmd.worldMatrix = wt.worldMatrix;
		cmd.pickingId = 0;
		renderer.SubmitStatic(cmd, m_Pipeline.GetGeometryPassID());
		submitted = true;
	}

	m_BindPoseMatrices.clear();
	for (auto e : registry.view<Engine::SkinnedMeshRendererComponent, Engine::WorldTransformComponent>())
	{
		if (!Engine::IsComponentEnabled<Engine::SkinnedMeshRendererComponent>(registry, e)) continue;
		const auto& smr = registry.get<Engine::SkinnedMeshRendererComponent>(e);
		const auto& wt = registry.get<Engine::WorldTransformComponent>(e);
		auto* mesh = smr.meshHandle.Get();
		auto* mat = smr.materialHandle.Get() ? smr.materialHandle.Get()
			: (skinMatHandle.IsValid() ? skinMatHandle.Get() : defaultMat);
		if (!mesh || !mat || renderer.ValidateMeshMaterial(mesh, mat, geomPass) != Engine::EResult::Success) continue;

		if (m_BindPoseMatrices.empty() && smr.skeletonHandle.IsValid() && smr.skeletonHandle.Get())
			Engine::AnimationEvaluator::EvaluateBindPoseSkinningMatrices(*smr.skeletonHandle.Get(), m_BindPoseMatrices);

		if (!submitted) SubmitFrame(*camera);
		Engine::SkinnedDrawCommand cmd{};
		cmd.mesh = mesh;
		if (mat) Engine::ResourceStreamer::EnsureMaterialTextures(*mat);
		cmd.material = mat;
		if (mat) mat->BuildMaterialConstants(cmd.materialConstants);
		cmd.worldMatrix = wt.worldMatrix;
		cmd.boneMatrices = m_BindPoseMatrices.empty() ? nullptr : &m_BindPoseMatrices;
		cmd.pickingId = 0;
		renderer.SubmitSkinned(cmd, m_Pipeline.GetGeometryPassID());
		submitted = true;
	}

	return submitted;
}

Engine::RHITexture* ThumbnailRenderer::GetFinalTexture() const
{
	const auto& name = m_OutputRTName.empty() ? m_Pipeline.GetFinalColorRTName() : m_OutputRTName;
	auto* rt = Engine::RenderTargetManager::Get().GetRenderTarget(name);
	return rt ? rt->GetTexture() : nullptr;
}

Engine::RHITexture* ThumbnailRenderer::CreateThumbnailTexture() const
{
	auto* finalTex = GetFinalTexture();
	if (!finalTex) return nullptr;

	RHITextureDesc desc = {};
	desc.width = m_Size;
	desc.height = m_Size;
	desc.format = finalTex->GetFormat();
	desc.dimension = Engine::ETextureDimension::Texture2D;
	desc.mipLevels = 1;
	desc.arraySize = 1;
	desc.sampleCount = Engine::ETextureSampleCount::TextureSampleCount1;

	auto* rhi = Engine::Renderer::Get().GetRHI();
	if (!rhi) return nullptr;

	Engine::RHITexture* newTex = rhi->CreateTexture(desc);
	if (!newTex) return nullptr;

	if (Engine::IsFailure(rhi->BlitTexture(finalTex, newTex)))
	{
		Engine::Safe_Release(newTex);
		return nullptr;
	}

	return newTex;
}
