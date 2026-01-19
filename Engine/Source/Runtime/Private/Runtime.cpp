#pragma once
#include "Runtime.h"

#include "GameObject.h"
#include "MeshRenderer.h"
#include "Mesh.h"

IMPLEMENT_SINGLETON(Runtime)

#pragma region Constructor&Destructor
EResult Runtime::Initialize(void* arg)
{
#if defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	RUNTIMEDESC* pRuntimeDesc = reinterpret_cast<RUNTIMEDESC*>(arg);
	RENDERERDESC RendererDesc = pRuntimeDesc->RendererDesc;

	m_Renderer = Renderer::Create(&RendererDesc);
	if (!m_Renderer) return EResult::Fail;
	m_TimeManager = TimeManager::Create();
	if (!m_TimeManager) return EResult::Fail;
	m_ResourceManager = ResourceManager::Create();
	if (!m_ResourceManager) return EResult::Fail;
	m_PrototypeManager = PrototypeManager::Create();
	if (!m_PrototypeManager) return EResult::Fail;

	{
		// [삼각형 생성 로직 시작] -----------------------------------------

		// 1. SDL_Vertex 형식으로 데이터 정의 (Position, Color, TexCoord)
		static SDL_Vertex vertices[] =
		{
			{ { 400.0f, 150.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f } }, // 빨강
			{ { 200.0f, 450.0f }, { 0.0f, 1.0f, 0.0f, 1.0f }, { 0.0f, 0.0f } }, // 초록
			{ { 600.0f, 450.0f }, { 0.0f, 0.0f, 1.0f, 1.0f }, { 0.0f, 0.0f } }  // 파랑
		};

		// 2. 메쉬 생성 정보 설정
		MESHDESC meshInfo = {};
		meshInfo.VertexData = vertices;
		meshInfo.VertexCount = 3;
		meshInfo.VertexStride = sizeof(SDL_Vertex);

		// 3. 메쉬 객체 생성
		Mesh* triangleMesh = Mesh::Create(&meshInfo);

		// 4. 게임 오브젝트 생성
		m_TestObject = GameObject::Create();
		if (m_TestObject && triangleMesh)
		{
			// 5. MeshRenderer 컴포넌트 부착 및 메쉬 설정
			MeshRenderer* meshRenderer = dynamic_cast<MeshRenderer*>(MeshRenderer::Create());
			if (meshRenderer)
			{
				meshRenderer->SetMesh(triangleMesh); // 메쉬 전달
				m_TestObject->AddComponent(meshRenderer);
				Safe_Release(meshRenderer);
			}
		}
		Safe_Release(triangleMesh);
	}

	return EResult::Success;
}

void Runtime::Free()
{
	Safe_Release(m_TestObject);
	OnUIRender.Clear();
	Renderer::Destroy();
	TimeManager::Destroy();
	ResourceManager::Destroy();
	PrototypeManager::Destroy();
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
	// Fixed Update Logic Here
}
void Runtime::Update(f32 dt)
{

}
void Runtime::LateUpdate(f32 dt)
{
	// Late Update Logic Here
}
EResult Runtime::Render(f32 dt)
{
	if (IsFailure(Renderer::Get().BeginFrame()))
	{
		fmt::print(stderr, "Renderer BeginFrame Failed\n");
	}

	// Rendering Logic Here

	m_TestObject->Render(dt);

	if (OnUIRender.IsBound())
	{
		OnUIRender.Broadcast(dt);
	}


	if (IsFailure(Renderer::Get().EndFrame()))
	{
		fmt::print(stderr, "Renderer EndFrame Failed\n");
	}
	return EResult::Success;
}
#pragma endregion
