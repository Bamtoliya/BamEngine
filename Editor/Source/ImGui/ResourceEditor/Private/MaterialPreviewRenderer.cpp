#include "MaterialPreviewRenderer.h"
#include "InputManager.h"
#include "RenderTargetManager.h"
#include "RenderPassManager.h"
#include "ResourceManager.h"
#include "Renderer.h"
#include "CameraBuffer.h"
#include "TimeManager.h"
#include "ImGuiManager.h"
#include "Logger.h"
#include "Vertex.h"

MaterialPreviewRenderer::~MaterialPreviewRenderer()
{
	Free();
}

void MaterialPreviewRenderer::Initialize(uint32 width, uint32 height)
{
	m_Width = width;
	m_Height = height;
	auto& rtm = Engine::RenderTargetManager::Get();
	// 1. Color RenderTarget 생성
	Engine::RenderTargetDesc colorDesc = {};
	colorDesc.name = L"MaterialPreview_Color";
	colorDesc.textureDesc.width = m_Width;
	colorDesc.textureDesc.height = m_Height;
	colorDesc.textureDesc.mipLevels = 1;
	colorDesc.textureDesc.arraySize = 1;
	colorDesc.textureDesc.sampleCount = Engine::ETextureSampleCount::TextureSampleCount1;
	colorDesc.textureDesc.dimension = Engine::ETextureDimension::Texture2D;
	colorDesc.textureDesc.format = Engine::ETextureFormat::R8G8B8A8_UNORM;
	colorDesc.textureDesc.usage = Engine::ETextureUsage::RenderTarget | Engine::ETextureUsage::Sampler;
	colorDesc.textureDesc.clearColor = vec4(0.12f, 0.13f, 0.15f, 1.0f);
	colorDesc.fixedSize = true;
	m_ColorRT = rtm.CreateRenderTarget(&colorDesc);
	// 2. Depth RenderTarget 생성
	Engine::RenderTargetDesc depthDesc = {};
	depthDesc.name = L"MaterialPreview_Depth";
	depthDesc.textureDesc.width = m_Width;
	depthDesc.textureDesc.height = m_Height;
	depthDesc.textureDesc.mipLevels = 1;
	depthDesc.textureDesc.arraySize = 1;
	depthDesc.textureDesc.sampleCount = Engine::ETextureSampleCount::TextureSampleCount1;
	depthDesc.textureDesc.dimension = Engine::ETextureDimension::Texture2D;
	depthDesc.textureDesc.format = Engine::ETextureFormat::D24_UNORM_S8_UINT;
	depthDesc.textureDesc.usage = Engine::ETextureUsage::DepthStencilTarget | Engine::ETextureUsage::Sampler;
	depthDesc.textureDesc.clearDepth = 1.0f;
	depthDesc.textureDesc.clearStencil = 0;
	depthDesc.fixedSize = true;
	m_DepthRT = rtm.CreateRenderTarget(&depthDesc);
	// 3. RenderPass 등록 (씬 자동 수집을 피하기 위해 passType을 Custom으로 등록)
	auto& rpm = Engine::RenderPassManager::Get();
	m_RenderPassID = rpm.RegisterRenderPass(
		L"MaterialPreview_Pass",
		{}, L"",
		Engine::ERenderPassLoadOperation::RPLO_Clear,
		Engine::ERenderPassStoreOperation::RPSO_Store,
		Engine::ERenderPassLoadOperation::RPLO_Clear,
		Engine::ERenderPassStoreOperation::RPSO_Store,
		vec4(0.12f, 0.13f, 0.15f, 1.0f),
		500,
		Engine::ERenderSortType::FrontToBack,
		Engine::ERenderPassType::Custom,
		Engine::EBlendMode::Opaque | Engine::EBlendMode::Masked | Engine::EBlendMode::AlphaBlend | Engine::EBlendMode::Additive | Engine::EBlendMode::Forward
	);
	m_RenderPass = rpm.GetRenderPassByID(m_RenderPassID);
	LoadPreviewMeshes();
}

void MaterialPreviewRenderer::Free()
{
	m_SphereMesh = {};
	m_CubeMesh = {};
	m_PlaneMesh = {};
}


TODO("절차적 생성이 아닌 외부 메쉬 소재로 바꾸거나 이미 로드된 메쉬를 사용하도록 바꿀 예정")
void MaterialPreviewRenderer::LoadPreviewMeshes()
{
	auto& rm = Engine::ResourceManager::Get();
	// 1. 구체 메쉬: Application에서 이미 정상 생성된 표준 SphereMesh 핸들 우선 재사용
	m_SphereMesh = rm.GetResourceHandle<Engine::Mesh>(L"SphereMesh");
	if (!m_SphereMesh.IsValid())
	{
		const uint32 slices = 24, stacks = 16;
		const float radius = 1.0f;
		vector<Engine::VertexPosition> positions;
		vector<Engine::VertexMaterial> materials;
		vector<uint32> indices;
		for (uint32 i = 0; i <= stacks; ++i)
		{
			float phi = glm::pi<float>() * static_cast<float>(i) / static_cast<float>(stacks);
			float y = radius * std::cos(phi);
			float rSin = radius * std::sin(phi);
			for (uint32 j = 0; j <= slices; ++j)
			{
				float theta = 2.0f * glm::pi<float>() * static_cast<float>(j) / static_cast<float>(slices);
				vec3 pos(rSin * std::cos(theta), y, rSin * std::sin(theta));
				vec3 normal = glm::normalize(pos);
				vec2 uv(static_cast<float>(j) / static_cast<float>(slices), static_cast<float>(i) / static_cast<float>(stacks));
				vec3 tangent = glm::normalize(vec3(-std::sin(theta), 0.0f, std::cos(theta)));
				vec3 binormal = glm::cross(normal, tangent);
				positions.push_back({ pos });
				Engine::VertexMaterial mat{};
				mat.normal = normal;
				mat.texCoord = uv;
				mat.tangent = tangent;
				mat.binormal = binormal;
				mat.color = vec4(1.0f);
				materials.push_back(mat);
			}
		}
		for (uint32 i = 0; i < stacks; ++i)
		{
			for (uint32 j = 0; j < slices; ++j)
			{
				uint32 first = i * (slices + 1) + j;
				uint32 second = first + slices + 1;
				// D3D12 규약에 일치하는 반시계(CCW) 와인딩
				indices.push_back(first);
				indices.push_back(second);
				indices.push_back(first + 1);
				indices.push_back(second);
				indices.push_back(second + 1);
				indices.push_back(first + 1);
			}
		}
		Engine::MeshCreateDesc sphereDesc = {};
		sphereDesc.Streams[(uint32)Engine::EMeshStream::Position] = { positions.data(), static_cast<uint32>(positions.size()), sizeof(Engine::VertexPosition) };
		sphereDesc.Streams[(uint32)Engine::EMeshStream::Material] = { materials.data(), static_cast<uint32>(materials.size()), sizeof(Engine::VertexMaterial) };
		sphereDesc.IndexData = indices.data();
		sphereDesc.IndexCount = static_cast<uint32>(indices.size());
		sphereDesc.IndexStride = sizeof(uint32);
		sphereDesc.BoundingBoxMin = vec3(-radius);
		sphereDesc.BoundingBoxMax = vec3(radius);
		m_SphereMesh = rm.AddResource<Engine::Mesh>(L"MaterialPreview_Sphere", Engine::Mesh::Create(&sphereDesc));
	}
	// 2. 큐브 메쉬: 기등록된 CubeMesh 사용
	m_CubeMesh = rm.GetResourceHandle<Engine::Mesh>(L"CubeMesh");
	// 3. 플레인 메쉬: -Z(카메라)를 향하는 CCW 와인딩 평면 메쉬
	m_PlaneMesh = rm.GetResourceHandle<Engine::Mesh>(L"MaterialPreview_Plane");
	if (!m_PlaneMesh.IsValid())
	{
		vector<Engine::VertexPosition> positions = {
			{ { -1.0f,  1.0f, 0.0f } },
			{ { -1.0f, -1.0f, 0.0f } },
			{ {  1.0f, -1.0f, 0.0f } },
			{ {  1.0f,  1.0f, 0.0f } }
		};
		vector<Engine::VertexMaterial> materials = {
			{ { 0, 0, -1 }, { 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 }, vec4(1.0f) },
			{ { 0, 0, -1 }, { 0, 1 }, { 1, 0, 0 }, { 0, 1, 0 }, vec4(1.0f) },
			{ { 0, 0, -1 }, { 1, 1 }, { 1, 0, 0 }, { 0, 1, 0 }, vec4(1.0f) },
			{ { 0, 0, -1 }, { 1, 0 }, { 1, 0, 0 }, { 0, 1, 0 }, vec4(1.0f) }
		};
		vector<uint32> indices = { 0, 1, 2, 0, 2, 3 };
		Engine::MeshCreateDesc planeDesc = {};
		planeDesc.Streams[(uint32)Engine::EMeshStream::Position] = { positions.data(), static_cast<uint32>(positions.size()), sizeof(Engine::VertexPosition) };
		planeDesc.Streams[(uint32)Engine::EMeshStream::Material] = { materials.data(), static_cast<uint32>(materials.size()), sizeof(Engine::VertexMaterial) };
		planeDesc.IndexData = indices.data();
		planeDesc.IndexCount = static_cast<uint32>(indices.size());
		planeDesc.IndexStride = sizeof(uint32);
		planeDesc.BoundingBoxMin = vec3(-1.0f, -1.0f, 0.0f);
		planeDesc.BoundingBoxMax = vec3(1.0f, 1.0f, 0.0f);
		m_PlaneMesh = rm.AddResource<Engine::Mesh>(L"MaterialPreview_Plane", Engine::Mesh::Create(&planeDesc));
	}
}

void MaterialPreviewRenderer::Submit(Engine::MaterialInterface* material, f32 dt)
{
	if (!material || !m_RenderPass || !m_ColorRT || !m_DepthRT) return;
	if (!material->HasPassShader(Engine::ERenderPassType::Custom))
	{
		auto& rm = Engine::ResourceManager::Get();
		auto vsHandle = rm.GetResourceHandle<Engine::Shader>(RESOURCE_PATH L"Shader/GBufferVS");
		auto psHandle = rm.GetResourceHandle<Engine::Shader>(RESOURCE_PATH L"Shader/MaterialPreviewPS");
		material->SetPassShader(Engine::ERenderPassType::Custom, vsHandle, psHandle);
	}

	m_RenderPass->SetColorAttachments({ L"MaterialPreview_Color" });
	m_RenderPass->SetDepthStencilAttachment(L"MaterialPreview_Depth");
	if (m_AutoRotate)
	{
		m_Yaw += m_AutoRotateSpeed * dt;
	}
	vec3 offset;
	offset.x = m_Distance * cosf(m_Pitch) * sinf(m_Yaw);
	offset.y = m_Distance * sinf(m_Pitch);
	offset.z = -m_Distance * cosf(m_Pitch) * cosf(m_Yaw);
	vec3 eye = m_Target + offset;
	mat4 view = glm::lookAtLH(eye, m_Target, vec3(0.0f, 1.0f, 0.0f));
	mat4 proj = glm::perspectiveLH_ZO(glm::radians(45.0f), 1.0f, 0.1f, 100.0f);
	CameraBuffer cameraBuffer = {};
	cameraBuffer.viewMatrix = view;
	cameraBuffer.invViewMatrix = glm::inverse(view);
	cameraBuffer.projMatrix = proj;
	cameraBuffer.invProjMatrix = glm::inverse(proj);
	cameraBuffer.viewProjMatrix = proj * view;
	cameraBuffer.invViewProjMatrix = glm::inverse(cameraBuffer.viewProjMatrix);
	cameraBuffer.cameraPosition = eye;
	cameraBuffer.time = static_cast<f32>(Engine::TimeManager::Get().GetTotalTime());
	Engine::Renderer::Get().RegisterViewportCamera(cameraBuffer, m_RenderPass);
	Engine::Mesh* mesh = nullptr;
	switch (m_CurrentMeshType)
	{
	case EPreviewMeshType::Sphere: mesh = m_SphereMesh.Get(); break;
	case EPreviewMeshType::Cube:   mesh = m_CubeMesh.Get();   break;
	case EPreviewMeshType::Plane:  mesh = m_PlaneMesh.Get();  break;
	default: break;
	}
	if (mesh)
	{
		Engine::StaticDrawCommand command;
		command.mesh = mesh;
		command.material = material;
		command.worldMatrix = glm::identity<mat4>();
		command.pickingId = 0;
		Engine::Renderer::Get().SubmitStatic(command, m_RenderPassID);
	}
}

void MaterialPreviewRenderer::DrawUI(Engine::MaterialInterface* material)
{
	if (!m_ColorRT || !m_ColorRT->GetTexture()) return;
	ImTextureID texID = ImGuiManager::Get().GetImGuiTextureID(m_ColorRT->GetTexture());
	if (!texID) return;
	// 1. 상단 뷰포트 툴바
	if (ImGui::RadioButton(LOCAL_LABEL("MaterialPreview.Sphere").c_str(), m_CurrentMeshType == EPreviewMeshType::Sphere))
		m_CurrentMeshType = EPreviewMeshType::Sphere;
	ImGui::SameLine();
	if (ImGui::RadioButton(LOCAL_LABEL("MaterialPreview.Cube").c_str(), m_CurrentMeshType == EPreviewMeshType::Cube))
		m_CurrentMeshType = EPreviewMeshType::Cube;
	ImGui::SameLine();
	if (ImGui::RadioButton(LOCAL_LABEL("MaterialPreview.Plane").c_str(), m_CurrentMeshType == EPreviewMeshType::Plane))
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
	// 2. 가용 영역 전체를 채우는 3D 뷰포트 이미지 (종횡비 1:1 유지 및 중앙 정렬)
	ImVec2 avail = ImGui::GetContentRegionAvail();
	float size = std::max(std::min(avail.x, avail.y), 150.0f);
	ImVec2 previewSize(size, size);
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail.x - size) * 0.5f);
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (avail.y - size) * 0.5f);
	ImVec2 cursorPos = ImGui::GetCursorScreenPos();
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	// 이미지 렌더링
	ImGui::Image(texID, previewSize);
	// 스튜디오 테두리 프레임
	ImVec2 bbMin = cursorPos;
	ImVec2 bbMax = ImVec2(cursorPos.x + previewSize.x, cursorPos.y + previewSize.y);
	drawList->AddRect(bbMin, bbMax, IM_COL32(65, 70, 80, 255), 4.0f, 0, 1.5f);
	// 3. 뷰포트 마우스 & 키보드 네비게이션
	if (ImGui::IsItemHovered())
	{
		ImGuiIO& io = ImGui::GetIO();
		const auto delta = InputManager::Get().GetMouseDelta();
		// 1. 현재 카메라 위치 및 시선 기준 벡터 정밀 계산 (카메라 로컬 좌표계)
		vec3 offset;
		offset.x = m_Distance * cosf(m_Pitch) * sinf(m_Yaw);
		offset.y = m_Distance * sinf(m_Pitch);
		offset.z = -m_Distance * cosf(m_Pitch) * cosf(m_Yaw);
		vec3 eye = m_Target + offset;
		// 카메라가 타겟을 바라보는 실제 전방 벡터 (Eye -> Target)
		vec3 camForward = glm::normalize(m_Target - eye);
		vec3 fwd = glm::normalize(vec3(camForward.x, 0.0f, camForward.z)); // 수평 전진 벡터
		vec3 right = glm::normalize(glm::cross(vec3(0.0f, 1.0f, 0.0f), fwd)); // 카메라 기준 실제 우측 벡터
		vec3 up = vec3(0.0f, 1.0f, 0.0f);
		// 2. 좌클릭 드래그: 오브젝트 중심 턴테이블 궤도 회전 (Orbit)
		if (InputManager::Get().IsMouseButtonDragging(EMouseButton::Left, 6.0f))
		{
			m_AutoRotate = false;
			m_Yaw += delta.x * 0.015f;
			m_Pitch = glm::clamp(m_Pitch - delta.y * 0.015f, -1.5f, 1.5f);
		}
		// 3. 우클릭 드래그: 시점 자유 회전 (Free Look)
		if (InputManager::Get().IsMouseButtonDragging(EMouseButton::Right, 6.0f))
		{
			m_AutoRotate = false;
			m_Yaw += delta.x * 0.015f;
			m_Pitch = glm::clamp(m_Pitch - delta.y * 0.015f, -1.5f, 1.5f);
		}
		// 4. 우클릭 유지 시 WASD / QE 비행 이동 (카메라 기준 정방향 이동)
		if (InputManager::Get().IsMouseButtonPressed(EMouseButton::Right))
		{
			float dt = io.DeltaTime;
			float moveDist = m_MoveSpeed * dt;
			// W: 시선 방향 앞으로 전진 | S: 뒤로 후진
			if (InputManager::Get().IsKeyButtonPressed(EKeyCode::W)) m_Target += fwd * moveDist;
			if (InputManager::Get().IsKeyButtonPressed(EKeyCode::S)) m_Target -= fwd * moveDist;
			// D: 카메라 우측으로 이동 | A: 카메라 좌측으로 이동
			if (InputManager::Get().IsKeyButtonPressed(EKeyCode::D)) m_Target += right * moveDist;
			if (InputManager::Get().IsKeyButtonPressed(EKeyCode::A)) m_Target -= right * moveDist;
			// Q: 위로 상승 | E: 아래로 하강
			if (InputManager::Get().IsKeyButtonPressed(EKeyCode::Q)) m_Target += up * moveDist;
			if (InputManager::Get().IsKeyButtonPressed(EKeyCode::E)) m_Target -= up * moveDist;
		}
		// 5. 휠 클릭 드래그 또는 Shift+좌클릭: 화면 평면 패닝 (Pan)
		if (InputManager::Get().IsMouseButtonDragging(EMouseButton::Middle, 6.0f) || (InputManager::Get().IsShiftPressed() && InputManager::Get().IsMouseButtonDragging(EMouseButton::Left, 6.0f)))
		{
			m_AutoRotate = false;
			float panSpeed = m_Distance * 0.0025f;
			m_Target -= right * (delta.x * panSpeed);
			m_Target += up * (delta.y * panSpeed);
		}
		// 6. 휠 스크롤: 줌 (Zoom)
		if (InputManager::Get().GetMouseScrollDeltaY() != 0.0f)
		{
			m_Distance = glm::clamp(m_Distance - InputManager::Get().GetMouseScrollDeltaY() * 0.25f, 0.5f, 15.0f);
		}
		// 7. [F] 키: 포커스 리셋
		if (InputManager::Get().IsKeyButtonDown(EKeyCode::F))
		{
			m_Target = vec3(0.0f);
			m_Distance = 3.5f;
			m_Yaw = 0.5f;
			m_Pitch = 0.25f;
			m_AutoRotate = true;
		}
		ImGui::SetTooltip("%s", LOCAL_CSTR("MaterialPreview.Navigation"));
	}
}

Engine::RHITexture* MaterialPreviewRenderer::GetPreviewRHITexture() const
{
	return m_ColorRT ? m_ColorRT->GetTexture() : nullptr;
}
