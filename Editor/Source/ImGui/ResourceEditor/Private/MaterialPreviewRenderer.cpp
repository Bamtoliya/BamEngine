#include "MaterialPreviewRenderer.h"
#include "RenderTargetManager.h"
#include "ResourceManager.h"
#include "Renderer.h"
#include "CameraBuffer.h"
#include "TimeManager.h"
#include "ImGuiManager.h"
#include "Scene.h"
#include "Components/Core/TransformComponent.h"
#include "Components/Core/WorldTransformComponent.h"
#include "Components/Core/FlagComponent.h"
#include "Components/Render/StaticMeshRendererComponent.h"
#include "Components/Render/LightComponent.h"
#include "TransformSystem.h"
#include "LightSystem.h"

MaterialPreviewRenderer::~MaterialPreviewRenderer()
{
	Free();
}

void MaterialPreviewRenderer::Initialize(uint32 width, uint32 height)
{
	m_Width = width;
	m_Height = height;
	// 1. 프리뷰 전용 파이프라인 초기화 (acceptSceneMeshes=false 로 메인 씬 유입 차단)
	m_Pipeline.Initialize(L"MatPreview", m_Width, m_Height, false);
	// 2. 프리뷰 메쉬 리소스 로드
	auto& rm = Engine::ResourceManager::Get();
	m_SphereMesh = rm.GetResourceHandle<Engine::Mesh>(L"SphereMesh");
	m_CubeMesh = rm.GetResourceHandle<Engine::Mesh>(L"CubeMesh");
	m_PlaneMesh = rm.GetResourceHandle<Engine::Mesh>(L"QuadMesh");
	// 3. 프리뷰 전용 Scene 및 엔티티 구축
	SetupPreviewScene();
}

void MaterialPreviewRenderer::Free()
{
	m_Pipeline.Free();

	if (m_PreviewScene)
	{
		Engine::Safe_Release(m_PreviewScene);
		m_PreviewScene = nullptr;
	}
	m_MeshEntity = entt::null;
	m_LightEntity = entt::null;
}

void MaterialPreviewRenderer::SetupPreviewScene()
{
	if (m_PreviewScene) return;

	Engine::SceneCreateDesc desc{};
	desc.name = L"MaterialPreview_Scene";
	m_PreviewScene = Engine::Scene::Create(&desc);
	if (!m_PreviewScene) return;

	auto& registry = m_PreviewScene->GetRegistry();

	// 1. 스튜디오 Directional Light 엔티티 생성 (트랜스폼 회전각으로 조명 방향 제어)
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
	// 우상단에서 대각선 아래로 비추는 스튜디오 조명 각도 설정 (Pitch 45도, Yaw -30도)
	(void)lightTrans.SetEulerRotation(vec3(45.0f, -30.0f, 0.0f));

	// 2. 프리뷰 대상 메쉬 엔티티 생성
	m_MeshEntity = registry.create();
	registry.emplace<Engine::TransformComponent>(m_MeshEntity);
	registry.emplace<Engine::WorldTransformComponent>(m_MeshEntity);
	auto& flag = registry.emplace<Engine::FlagComponent>(m_MeshEntity);
	flag.flags = Engine::EEntityFlag::Active | Engine::EEntityFlag::Visible;

	auto& meshComp = registry.emplace<Engine::StaticMeshRendererComponent>(m_MeshEntity);
	meshComp.meshHandle = m_SphereMesh;
	meshComp.castShadow = true;

	// 계층 구조 및 라이트 GPU 버퍼 캐시 갱신
	Engine::TransformSystem::UpdateHierarchy(registry);
	Engine::LightSystem::UpdateLights(registry);
}

void MaterialPreviewRenderer::UpdatePreviewMesh(Engine::MaterialInterface* material)
{
	if (!m_PreviewScene || m_MeshEntity == entt::null) return;

	auto& registry = m_PreviewScene->GetRegistry();
	auto* meshComp = registry.try_get<Engine::StaticMeshRendererComponent>(m_MeshEntity);
	if (!meshComp) return;

	switch (m_CurrentMeshType)
	{
	case EPreviewMeshType::Sphere: meshComp->meshHandle = m_SphereMesh; break;
	case EPreviewMeshType::Cube:   meshComp->meshHandle = m_CubeMesh;   break;
	case EPreviewMeshType::Plane:  meshComp->meshHandle = m_PlaneMesh;  break;
	default: break;
	}

	if (material)
	{
		auto handle = Engine::ResourceManager::Get().GetResourceHandle<Engine::MaterialInterface>(material->GetKey());
		if (meshComp->materialHandle != handle)
		{
			meshComp->materialHandle = handle;
		}
	}
}

void MaterialPreviewRenderer::Submit(Engine::MaterialInterface* material, f32 dt)
{
	if (!material || !m_PreviewScene) return;
	// 1. 메쉬 엔티티의 머티리얼과 메쉬 타입 동기화
	UpdatePreviewMesh(material);
	// 2. 트랜스폼 계층 및 라이트 캐시 최신화
	auto& registry = m_PreviewScene->GetRegistry();
	Engine::TransformSystem::UpdateHierarchy(registry);
	Engine::LightSystem::UpdateLights(registry);
	// 3. 자동 회전
	if (m_AutoRotate)
	{
		m_Yaw += m_AutoRotateSpeed * dt;
	}
	// 4. 카메라 좌표 및 뷰/투영 행렬 계산
	vec3 offset;
	offset.x = m_Distance * cosf(m_Pitch) * sinf(m_Yaw);
	offset.y = m_Distance * sinf(m_Pitch);
	offset.z = -m_Distance * cosf(m_Pitch) * cosf(m_Yaw);
	vec3 eye = m_Target + offset;
	mat4 view = glm::lookAtLH(eye, m_Target, vec3(0.0f, 1.0f, 0.0f));
	mat4 proj = glm::perspectiveLH_ZO(glm::radians(45.0f), static_cast<f32>(m_Width) / static_cast<f32>(m_Height), 0.1f, 100.0f);
	CameraBuffer cameraBuffer = {};
	cameraBuffer.viewMatrix = view;
	cameraBuffer.invViewMatrix = glm::inverse(view);
	cameraBuffer.projMatrix = proj;
	cameraBuffer.invProjMatrix = glm::inverse(proj);
	cameraBuffer.viewProjMatrix = proj * view;
	cameraBuffer.invViewProjMatrix = glm::inverse(cameraBuffer.viewProjMatrix);
	cameraBuffer.cameraPosition = eye;
	cameraBuffer.time = static_cast<f32>(Engine::TimeManager::Get().GetTotalTime());
	// 5. 프리뷰 전용 디퍼드 파이프라인에 카메라 등록
	m_Pipeline.Submit(cameraBuffer, m_PreviewScene);
	// 6. [핵심] 프리뷰 메쉬 드로우 커맨드를 프리뷰의 GeometryPass에 직접 제출!
	auto* meshComp = registry.try_get<Engine::StaticMeshRendererComponent>(m_MeshEntity);
	auto* transComp = registry.try_get<Engine::WorldTransformComponent>(m_MeshEntity);
	if (meshComp && transComp)
	{
		Engine::Mesh* mesh = meshComp->meshHandle.Get();
		Engine::MaterialInterface* mat = meshComp->materialHandle.Get();
		if (mesh && mat)
		{
			Engine::StaticDrawCommand cmd{};
			cmd.mesh = mesh;
			cmd.material = mat;
			cmd.worldMatrix = transComp->worldMatrix;
			cmd.pickingId = 0;
			Engine::Renderer::Get().SubmitStatic(cmd, m_Pipeline.GetGeometryPassID());
		}
	}
}

void MaterialPreviewRenderer::DrawUI(Engine::MaterialInterface* material)
{
	auto& rtm = Engine::RenderTargetManager::Get();
	auto* colorRT = rtm.GetRenderTarget(m_Pipeline.GetFinalColorRTName());
	if (!colorRT || !colorRT->GetTexture()) return;

	ImTextureID texID = ImGuiManager::Get().GetImGuiTextureID(colorRT->GetTexture());
	if (!texID) return;

	// 1. 상단 뷰포트 툴바
	if (ImGui::Button(m_CurrentMeshType == EPreviewMeshType::Sphere ? "[Sphere]" : "Sphere"))
		m_CurrentMeshType = EPreviewMeshType::Sphere;
	ImGui::SameLine();
	if (ImGui::Button(m_CurrentMeshType == EPreviewMeshType::Cube ? "[Cube]" : "Cube"))
		m_CurrentMeshType = EPreviewMeshType::Cube;
	ImGui::SameLine();
	if (ImGui::Button(m_CurrentMeshType == EPreviewMeshType::Plane ? "[Plane]" : "Plane"))
		m_CurrentMeshType = EPreviewMeshType::Plane;
	ImGui::SameLine();
	if (ImGui::Button(m_AutoRotate ? ICON_FA_PAUSE : ICON_FA_PLAY))
		m_AutoRotate = !m_AutoRotate;
	ImGui::SameLine();
	if (ImGui::Button(ICON_FA_ROTATE_LEFT "##ResetView"))
	{
		m_Target = vec3(0.0f);
		m_Yaw = 0.5f;
		m_Pitch = 0.25f;
		m_Distance = 3.5f;
		m_AutoRotate = true;
	}

	ImGui::Separator();

	// 2. 가용 영역 전체를 채우는 3D 뷰포트 이미지 (중앙 정렬)
	ImVec2 avail = ImGui::GetContentRegionAvail();
	float size = std::max(std::min(avail.x, avail.y), 150.0f);
	ImVec2 previewSize(size, size);

	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail.x - size) * 0.5f);
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (avail.y - size) * 0.5f);

	ImVec2 cursorPos = ImGui::GetCursorScreenPos();
	ImDrawList* drawList = ImGui::GetWindowDrawList();

	ImGui::Image(texID, previewSize);

	ImVec2 bbMin = cursorPos;
	ImVec2 bbMax = ImVec2(cursorPos.x + previewSize.x, cursorPos.y + previewSize.y);
	drawList->AddRect(bbMin, bbMax, IM_COL32(65, 70, 80, 255), 4.0f, 0, 1.5f);

	// 3. 마우스 & 키보드 카메라 조작
	if (ImGui::IsItemHovered())
	{
		ImGuiIO& io = ImGui::GetIO();
		ImVec2 delta = io.MouseDelta;

		vec3 offset;
		offset.x = m_Distance * cosf(m_Pitch) * sinf(m_Yaw);
		offset.y = m_Distance * sinf(m_Pitch);
		offset.z = -m_Distance * cosf(m_Pitch) * cosf(m_Yaw);
		vec3 eye = m_Target + offset;

		vec3 camForward = glm::normalize(m_Target - eye);
		vec3 fwd = glm::normalize(vec3(camForward.x, 0.0f, camForward.z));
		vec3 right = glm::normalize(glm::cross(vec3(0.0f, 1.0f, 0.0f), fwd));
		vec3 up = vec3(0.0f, 1.0f, 0.0f);

		if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
		{
			m_AutoRotate = false;
			m_Yaw += delta.x * 0.015f;
			m_Pitch = glm::clamp(m_Pitch - delta.y * 0.015f, -1.5f, 1.5f);
		}

		if (ImGui::IsMouseDragging(ImGuiMouseButton_Right))
		{
			m_AutoRotate = false;
			m_Yaw += delta.x * 0.015f;
			m_Pitch = glm::clamp(m_Pitch - delta.y * 0.015f, -1.5f, 1.5f);
		}

		if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
		{
			float dt = io.DeltaTime;
			float moveDist = m_MoveSpeed * dt;

			if (ImGui::IsKeyDown(ImGuiKey_W)) m_Target += fwd * moveDist;
			if (ImGui::IsKeyDown(ImGuiKey_S)) m_Target -= fwd * moveDist;
			if (ImGui::IsKeyDown(ImGuiKey_D)) m_Target += right * moveDist;
			if (ImGui::IsKeyDown(ImGuiKey_A)) m_Target -= right * moveDist;
			if (ImGui::IsKeyDown(ImGuiKey_E)) m_Target += up * moveDist;
			if (ImGui::IsKeyDown(ImGuiKey_Q)) m_Target -= up * moveDist;
		}

		if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle) || (io.KeyShift && ImGui::IsMouseDragging(ImGuiMouseButton_Left)))
		{
			m_AutoRotate = false;
			float panSpeed = m_Distance * 0.0025f;
			m_Target -= right * (delta.x * panSpeed);
			m_Target += up * (delta.y * panSpeed);
		}

		if (io.MouseWheel != 0.0f)
		{
			m_Distance = glm::clamp(m_Distance - io.MouseWheel * 0.25f, 0.5f, 15.0f);
		}

		if (ImGui::IsKeyPressed(ImGuiKey_F))
		{
			m_Target = vec3(0.0f);
			m_Distance = 3.5f;
			m_Yaw = 0.5f;
			m_Pitch = 0.25f;
			m_AutoRotate = true;
		}

		ImGui::SetTooltip("L-Drag: Orbit | R-Drag+WASD: Fly | M-Drag: Pan | Wheel: Zoom | [F]: Reset");
	}
}

Engine::RHITexture* MaterialPreviewRenderer::GetPreviewRHITexture() const
{
	auto* rt = Engine::RenderTargetManager::Get().GetRenderTarget(m_Pipeline.GetFinalColorRTName());
	return rt ? rt->GetTexture() : nullptr;
}