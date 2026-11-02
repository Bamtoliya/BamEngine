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

IMPLEMENT_SINGLETON(ThumbnailRenderer)

EResult ThumbnailRenderer::Initialize(void* arg)
{
	// 스타트업 셰이더가 로드되기 전이므로 여기서 즉시 Setup하지 않고,
	// 첫 렌더링 요청 시점에 지연 초기화(Lazy Init)합니다.
	return EResult::Success;
}

void ThumbnailRenderer::Free()
{
	if (!m_Initialized) return;

	m_Pipeline.Free();

	if (m_Scene)
	{
		Engine::Safe_Release(m_Scene);
		m_Scene = nullptr;
	}

	m_MeshEntity = entt::null;
	m_LightEntity = entt::null;
	m_SphereMesh = {};
	m_Initialized = false;
}

void ThumbnailRenderer::Setup(uint32 size)
{
	if (m_Initialized) return;

	// 필수 셰이더 로드 여부 방어 검사 (아직 로드 전이면 초기화 보류)
	auto& rm = Engine::ResourceManager::Get();
	auto vs = rm.GetResourceHandle<Engine::Shader>(L"FullscreenQuadVS");
	auto ps = rm.GetResourceHandle<Engine::Shader>(L"LightingPS");
	if (!vs.IsValid() || !ps.IsValid() || !vs.Get() || !ps.Get())
		return;

	m_Size = size;

	// 1. 128x128 오프스크린 파이프라인 초기화 (acceptSceneMeshes=false 로 메인 씬 유입 차단)
	m_Pipeline.Initialize(L"ThumbPreview", m_Size, m_Size, false);

	// 2. 엔진 내장 구체(Sphere) 메쉬 로드
	m_SphereMesh = rm.GetResourceHandle<Engine::Mesh>(L"SphereMesh");

	// 3. 프리뷰 전용 독립 씬 및 스튜디오 조명/메쉬 구축
	SetupStudioScene();

	m_Initialized = true;
}

void ThumbnailRenderer::SetupStudioScene()
{
	if (m_Scene) return;

	Engine::SceneCreateDesc desc{};
	desc.name = L"ThumbnailPreview_Scene";
	m_Scene = Engine::Scene::Create(&desc);
	if (!m_Scene) return;

	auto& registry = m_Scene->GetRegistry();

	// 1. 스튜디오 조명 생성 (따뜻한 백색광, Pitch 45도, Yaw -30도의 입체감 있는 각도)
	m_LightEntity = registry.create();
	auto& lightTrans = registry.emplace<Engine::TransformComponent>(m_LightEntity);
	registry.emplace<Engine::WorldTransformComponent>(m_LightEntity);
	auto& lightFlag = registry.emplace<Engine::FlagComponent>(m_LightEntity);
	lightFlag.flags = Engine::EEntityFlag::Active | Engine::EEntityFlag::Visible;

	auto& lightComp = registry.emplace<Engine::LightComponent>(m_LightEntity);
	lightComp.LightType = Engine::ELightType::Directional;
	lightComp.Intensity = 2.5f;
	lightComp.Color = vec3(1.0f, 0.98f, 0.95f);
	lightComp.Flags = Engine::ELightFlags::UseInDeferredRendering | Engine::ELightFlags::AffectDiffuse | Engine::ELightFlags::AffectSpecular;
	(void)lightTrans.SetEulerRotation(vec3(45.0f, -30.0f, 0.0f));

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
	Engine::TransformSystem::UpdateHierarchy(registry);
	Engine::LightSystem::UpdateLights(registry);
}

bool ThumbnailRenderer::SubmitMaterial(Engine::MaterialInterface* material)
{
	// 최초 요청 시점에 안전하게 파이프라인 지연 생성
	if (!m_Initialized)
	{
		Setup(128);
	}

	if (!m_Initialized || !material || !m_Scene || m_MeshEntity == entt::null)
		return false;

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
	if (geomPass && Engine::IsFailure(Engine::Renderer::Get().ValidateMeshMaterial(mesh, material, geomPass)))
		return false;

	// 대상 머티리얼 바인딩
	auto handle = Engine::ResourceManager::Get().GetResourceHandle<Engine::MaterialInterface>(material->GetKey());
	meshComp->materialHandle = handle;

	// 트랜스폼 및 조명 시스템 갱신
	Engine::TransformSystem::UpdateHierarchy(registry);
	Engine::LightSystem::UpdateLights(registry);

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
	m_Pipeline.Submit(cameraBuffer, m_Scene);

	// acceptSceneMeshes=false 이므로 프리뷰 메쉬 드로우 커맨드를 파이프라인의 GeometryPass에 직접 제출
	auto* transComp = registry.try_get<Engine::WorldTransformComponent>(m_MeshEntity);
	if (transComp)
	{
		Engine::StaticDrawCommand cmd{};
		cmd.mesh = mesh;
		cmd.material = material;
		cmd.worldMatrix = transComp->worldMatrix;
		cmd.pickingId = 0;
		Engine::Renderer::Get().SubmitStatic(cmd, m_Pipeline.GetGeometryPassID());
	}

	return true;
}

Engine::RHITexture* ThumbnailRenderer::GetFinalTexture() const
{
	if (!m_Initialized) return nullptr;
	auto* rt = Engine::RenderTargetManager::Get().GetRenderTarget(m_Pipeline.GetFinalColorRTName());
	return rt ? rt->GetTexture() : nullptr;
}

Engine::RHITexture* ThumbnailRenderer::CreateThumbnailTexture() const
{
	if (!m_Initialized) return nullptr;
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