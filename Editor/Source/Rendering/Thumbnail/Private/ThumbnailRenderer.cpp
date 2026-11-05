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
	m_StudioLights[0] = createDirLight(vec3(30.0f, 25.0f, 0.0f), vec3(1.0f, 0.98f, 0.95f), true);
	m_StudioLights[1] = createDirLight(vec3(15.0f, -45.0f, 0.0f), vec3(0.85f, 0.92f, 1.0f), false);
	m_StudioLights[2] = createDirLight(vec3(45.0f, 160.0f, 0.0f), vec3(0.95f, 0.98f, 1.0f), true);
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

	// 스튜디오 카메라 설정 (스튜디오 구체를 정면 약간 위에서 응시)
	const vec3 target = vec3(0.0f);
	const f32 distance = 3.5f;
	const f32 pitch = 0.25f;
	const f32 yaw = 0.5f;

	vec3 offset;
	offset.x = distance * cosf(pitch) * sinf(yaw);
	offset.y = distance * sinf(pitch);
	offset.z = -distance * cosf(pitch) * cosf(yaw);

	const vec3 eye = target + offset;
	const mat4 view = glm::lookAtLH(eye, target, vec3(0.0f, 1.0f, 0.0f));
	const mat4 proj = glm::perspectiveLH_ZO(glm::radians(45.0f), 1.0f, 0.1f, 100.0f);

	CameraBuffer cameraBuffer = {};
	cameraBuffer.viewMatrix = view;
	cameraBuffer.invViewMatrix = glm::inverse(view);
	cameraBuffer.projMatrix = proj;
	cameraBuffer.invProjMatrix = glm::inverse(proj);
	cameraBuffer.viewProjMatrix = proj * view;
	cameraBuffer.invViewProjMatrix = glm::inverse(cameraBuffer.viewProjMatrix);
	cameraBuffer.cameraPosition = eye;
	cameraBuffer.time = 0.0f;

	// 128x128 오프스크린 파이프라인에 카메라 버퍼 및 씬 등록
	auto* transComp = registry.try_get<Engine::WorldTransformComponent>(m_MeshEntity);
	if (transComp == nullptr)
		return false;

	m_Pipeline.Submit(cameraBuffer, m_Scene);

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

CameraBuffer ThumbnailRenderer::CalculateAutoFramingCamera(const vec3& bMin, const vec3& bMax) const
{
	const vec3 center = (bMin + bMax) * 0.5f;
	const vec3 extents = (bMax - bMin) * 0.5f;
	float radius = glm::length(extents);
	if (radius < 0.001f || !std::isfinite(radius)) radius = 1.0f;
	const float fovY = glm::radians(45.0f);
	const float distance = (radius / std::sin(fovY * 0.5f)) * 1.35f;
	const float pitch = glm::radians(22.0f);
	const float yaw = glm::radians(35.0f);
	vec3 offset;
	offset.x = distance * std::cos(pitch) * std::sin(yaw);
	offset.y = distance * std::sin(pitch);
	offset.z = -distance * std::cos(pitch) * std::cos(yaw);
	const vec3 eye = center + offset;
	const mat4 view = glm::lookAtLH(eye, center, vec3(0.0f, 1.0f, 0.0f));
	const mat4 proj = glm::perspectiveLH_ZO(fovY, 1.0f, std::max(distance * 0.01f, 0.05f), distance * 15.0f);
	CameraBuffer camBuf{};
	camBuf.viewMatrix = view;
	camBuf.invViewMatrix = glm::inverse(view);
	camBuf.projMatrix = proj;
	camBuf.invProjMatrix = glm::inverse(proj);
	camBuf.viewProjMatrix = proj * view;
	camBuf.invViewProjMatrix = glm::inverse(camBuf.viewProjMatrix);
	camBuf.cameraPosition = eye;
	camBuf.time = 0.0f;
	return camBuf;
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
	const CameraBuffer camBuf = CalculateAutoFramingCamera(mesh->GetMin(), mesh->GetMax());
	m_Pipeline.Submit(camBuf, m_Scene);
	if (mesh->IsSkinned())
	{
		Engine::SkinnedDrawCommand cmd{};
		cmd.mesh = mesh;
		cmd.material = material;
		cmd.worldMatrix = mat4(1.0f);
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
	vec3 totalMin(std::numeric_limits<float>::max());
	vec3 totalMax(std::numeric_limits<float>::lowest());
	bool hasValidBounds = false;
	for (size_t i = 0; i < nodes.size(); ++i)
	{
		for (uint32 meshIdx : nodes[i].MeshIndices)
		{
			if (meshIdx < meshes.size() && meshes[meshIdx].IsValid() && meshes[meshIdx].Get())
			{
				Engine::Mesh* m = meshes[meshIdx].Get();
				const vec3 mMin = m->GetMin();
				const vec3 mMax = m->GetMax();
				const mat4& M = worldTransforms[i];
				const vec3 corners[8] = {
					vec3(mMin.x, mMin.y, mMin.z), vec3(mMax.x, mMin.y, mMin.z),
					vec3(mMin.x, mMax.y, mMin.z), vec3(mMax.x, mMax.y, mMin.z),
					vec3(mMin.x, mMin.y, mMax.z), vec3(mMax.x, mMin.y, mMax.z),
					vec3(mMin.x, mMax.y, mMax.z), vec3(mMax.x, mMax.y, mMax.z)
				};
				for (int c = 0; c < 8; ++c)
				{
					const vec3 wPos = vec3(M * vec4(corners[c], 1.0f));
					totalMin = glm::min(totalMin, wPos);
					totalMax = glm::max(totalMax, wPos);
				}
				hasValidBounds = true;
			}
		}
	}
	if (!hasValidBounds) { totalMin = vec3(-1.0f); totalMax = vec3(1.0f); }
	const CameraBuffer camBuf = CalculateAutoFramingCamera(totalMin, totalMax);
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
				m_Pipeline.Submit(camBuf, m_Scene);

			if (mesh->IsSkinned())
			{
				Engine::SkinnedDrawCommand cmd{};
				cmd.mesh = mesh;
				cmd.material = material;
				cmd.worldMatrix = worldTransforms[i];
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