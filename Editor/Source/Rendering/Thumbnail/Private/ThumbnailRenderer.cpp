#include "ThumbnailRenderer.h"
#include "Scene.h"
#include "TransformComponent.h"
#include "WorldTransformComponent.h"
#include "FlagComponent.h"
#include "StaticMeshRendererComponent.h"
#include "LightComponent.h"
#include "TransformSystem.h"
#include "LightSystem.h"
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

	return EResult::Success;
}

void ThumbnailRenderer::Free()
{
	if (!m_Pipeline.GetOwnedRTNames().empty())
		m_Pipeline.Free();

	Engine::Safe_Release(m_Scene);
	m_Scene = nullptr;

	m_MeshEntity = entt::null;
	m_SphereMesh = {};
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
	meshComp.meshHandle = m_SphereMesh;
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

	m_Pipeline.Submit(*camera, m_Scene);

	Engine::StaticDrawCommand cmd{};
	cmd.mesh = mesh;
	cmd.material = material;
	cmd.worldMatrix = transComp->worldMatrix;
	cmd.pickingId = 0;
	Engine::Renderer::Get().SubmitStatic(cmd, m_Pipeline.GetGeometryPassID());

	return true;
}

Engine::MaterialInterface* ThumbnailRenderer::GetDefaultPreviewMaterial() const
{
	auto& rm = Engine::ResourceManager::Get();
	auto handle = rm.GetResourceHandle<Engine::MaterialInterface>(L"Resources/Material/PBRMaterial.bammat.json");
	if (!handle.IsValid() || !handle.Get())
		handle = rm.GetResourceHandle<Engine::MaterialInterface>(L"Resources/Material/GBufferMaterial.bammat.json");
	if (!handle.IsValid() || !handle.Get())
	{
		auto loaded = rm.LoadFile(L"Resources/Material/PBRMaterial.bammat.json");
		if (loaded.IsValid()) handle = Engine::ResourceHandle<Engine::MaterialInterface>(loaded);
	}
	return handle.Get();
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
	m_Pipeline.Submit(*camera, m_Scene);
	if (mesh->IsSkinned())
	{
		Engine::SkinnedDrawCommand cmd{};
		cmd.mesh = mesh;
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
		cmd.material = material;
		cmd.worldMatrix = mat4(1.0f);
		cmd.pickingId = 0;
		Engine::Renderer::Get().SubmitStatic(cmd, m_Pipeline.GetGeometryPassID());
	}
	return true;
}
bool ThumbnailRenderer::SubmitModel(Engine::Model* model)
{
	if (model == nullptr) return false;
	RefreshStudioLights();
	const auto& nodes = model->GetNodes();
	const auto& meshes = model->GetMeshes();
	if (meshes.empty()) return false;
	std::vector<mat4> worldTransforms(nodes.size(), mat4(1.0f));
	for (size_t i = 0; i < nodes.size(); ++i)
	{
		const auto& node = nodes[i];
		worldTransforms[i] = (node.ParentIndex >= 0 && static_cast<size_t>(node.ParentIndex) < i)
			? worldTransforms[node.ParentIndex] * node.LocalTransform : node.LocalTransform;
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
				m_Pipeline.Submit(*camera, m_Scene);

			if (mesh->IsSkinned())
			{
				Engine::SkinnedDrawCommand cmd{};
				cmd.mesh = mesh;
				cmd.material = material;
				if (material) material->BuildMaterialConstants(cmd.materialConstants);
				cmd.worldMatrix = m_BindPoseMatrices.empty() ? worldTransforms[i] : mat4(1.0f);
				cmd.boneMatrices = m_BindPoseMatrices.empty() ? nullptr : &m_BindPoseMatrices;
				cmd.pickingId = 0;
				renderer.SubmitSkinned(cmd, m_Pipeline.GetGeometryPassID());
			}
			else
			{
				Engine::StaticDrawCommand cmd{};
				cmd.mesh = mesh;
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

Engine::RHITexture* ThumbnailRenderer::GetFinalTexture() const
{
	auto* rt = Engine::RenderTargetManager::Get().GetRenderTarget(m_Pipeline.GetFinalColorRTName());
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
