#pragma once
#include "Runtime.h"

#include "GameObject.h"
#include "MeshRenderer.h"
#include "Mesh.h"
#include "SDLShader.h"

IMPLEMENT_SINGLETON(Runtime)

#pragma region Constructor&Destructor
EResult Runtime::Initialize(void* arg)
{
#if defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	RUNTIMEDESC* pRuntimeDesc = reinterpret_cast<RUNTIMEDESC*>(arg);
	tagRendererDesc RendererDesc = pRuntimeDesc->RendererDesc;

	if (!ReflectionRegistry::Create()) return EResult::Fail;
	InitEnumReflection();

	m_ComponentRegistry = ComponentRegistry::Create();
	if (!m_ComponentRegistry) return EResult::Fail;

	m_TimeManager = TimeManager::Create();
	if (!m_TimeManager) return EResult::Fail;

	m_RenderTargetManager = RenderTargetManager::Create();
	if (!m_RenderTargetManager) return EResult::Fail;
	m_RenderPassManager = RenderPassManager::Create();
	if (!m_RenderPassManager) return EResult::Fail;
	m_Renderer = Renderer::Create(&RendererDesc);
	if (!m_Renderer) return EResult::Fail;
	
	m_ResourceManager = ResourceManager::Create();
	if (!m_ResourceManager) return EResult::Fail;
	m_PrototypeManager = PrototypeManager::Create();
	if (!m_PrototypeManager) return EResult::Fail;

	m_LayerManager = LayerManager::Create();
	if (!m_LayerManager) return EResult::Fail;
	m_SceneManager = SceneManager::Create();
	if (!m_SceneManager) return EResult::Fail;

	m_LocalizationManager = LocalizationManager::Create();
	if (!m_LocalizationManager) return EResult::Fail;

	m_LocalizationManager->LoadData();

#pragma region Test
	{
		tagSDLShdaerCreateDesc shaderDesc = {};
		shaderDesc.ShaderType = EShaderType::Vertex;
		m_TestShader = SDLShader::Create(&shaderDesc);

		if (m_TestShader)
		{
			// 생성된 쉐이더를 RHI에 바인딩 (이후 모든 렌더링에 이 쉐이더 사용)
			Renderer::Get().GetRHI()->BindShader(m_TestShader);
		}
		else
		{
			return EResult::Fail;
		}
		// 1. 사각형 정점 데이터 정의 (화면 좌표계: Top-Left가 0,0 가정 시 대략적인 중앙 배치)
		// 색상은 SDLRendererRHI 구현에 따라 다를 수 있으나 Vertex 구조체에 맞춰 설정
		vector<Vertex> vertices = {
			// position (x, y, z)          // normal       // texCoord // tangent
			{ { -1.0f,  1.0f, 0.0f }, { 0, 0, -1 }, { 0, 0 }, { 0, 0, 0 } }, // Top-Left
			{ {  1.0f,  1.0f, 0.0f }, { 0, 0, -1 }, { 1, 0 }, { 0, 0, 0 } }, // Top-Right
			{ {  1.0f, -1.0f, 0.0f }, { 0, 0, -1 }, { 1, 1 }, { 0, 0, 0 } }, // Bottom-Right
			{ { -1.0f, -1.0f, 0.0f }, { 0, 0, -1 }, { 0, 1 }, { 0, 0, 0 } }  // Bottom-Left
		};

		// 인덱스 (사각형을 구성하는 두 개의 삼각형)
		vector<uint32> indices = {
			0, 1, 2, // 첫 번째 삼각형
			0, 2, 3  // 두 번째 삼각형
		};

		// 2. Mesh 생성
		tagMeshCreateInfo meshDesc = {};
		meshDesc.VertexData = vertices.data();
		meshDesc.VertexCount = static_cast<uint32>(vertices.size());
		meshDesc.VertexStride = sizeof(Vertex);
		meshDesc.IndexData = indices.data();
		meshDesc.IndexStride = sizeof(uint32);
		meshDesc.IndexCount = static_cast<uint32>(indices.size());

		Mesh* quadMesh = Mesh::Create(&meshDesc);

		// 3. GameObject 및 MeshRenderer 생성
		m_TestObject = GameObject::Create();
		MeshRenderer* meshRenderer = m_TestObject->AddComponent<MeshRenderer>(); // Registry 통해 생성
		meshRenderer->SetMesh(quadMesh);
		meshRenderer->SetRenderPassID(0);

		// Mesh는 Renderer가 내부적으로 RefCount를 관리하지 않는다면(현재 구조상) 
		// MeshRenderer가 소유하거나 별도로 관리해야 함. 여기서는 MeshRenderer에 넘긴 후 Release(소유권 이전 가정)
		Safe_Release(quadMesh);
	}
#pragma endregion


	return EResult::Success;
}

void Runtime::Free()
{
#ifdef _DEBUG
	Safe_Release(m_TestShader);
	Safe_Release(m_TestObject);
#endif
	TimeManager::Destroy();

	RenderTargetManager::Destroy();
	RenderPassManager::Destroy();
	Renderer::Destroy();
	
	ResourceManager::Destroy();
	PrototypeManager::Destroy();

	LayerManager::Destroy();
	SceneManager::Destroy();

	ComponentRegistry::Destroy();
	ReflectionRegistry::Destroy();
	LocalizationManager::Destroy();
}
#pragma endregion

#pragma region Loop
void Runtime::RunFrame(f32 dt)
{	
	FixedUpdate(dt);
	Update(dt);
	LateUpdate(dt);
	Render(dt);
}
void Runtime::FixedUpdate(f32 dt)
{
	SceneManager::Get().FixedUpdate(dt);
}
void Runtime::Update(f32 dt)
{
	SceneManager::Get().Update(dt);
}
void Runtime::LateUpdate(f32 dt)
{
#ifdef _DEBUG
	m_TestObject->LateUpdate(dt);
#endif
	SceneManager::Get().LateUpdate(dt);
}
EResult Runtime::Render(f32 dt)
{
	if (IsFailure(Renderer::Get().BeginFrame()))
	{
		fmt::print(stderr, "Renderer BeginFrame Failed\n");
	}

	if (IsFailure(Renderer::Get().Render(dt)))
	{
		fmt::print(stderr, "Renderer Render Failed\n");
	}

	if (IsFailure(Renderer::Get().EndFrame()))
	{
		fmt::print(stderr, "Renderer EndFrame Failed\n");
	}
	return EResult::Success;
}
#pragma endregion
