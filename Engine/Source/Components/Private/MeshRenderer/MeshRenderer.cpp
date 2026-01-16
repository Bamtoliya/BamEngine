#pragma once
#include "MeshRenderer.h"
#include "Renderer.h"

#pragma region Contructor&Destructor
EResult MeshRenderer::Initialize(void* arg)
{
	if (IsFailure(__super::Initialize(arg)))
		return EResult::Fail;
	SDL_Vertex vertices[] =
	{
		{ { 400.0f, 150.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f } },
		{ { 200.0f, 450.0f }, { 0.0f, 1.0f, 0.0f, 1.0f }, { 0.0f, 0.0f } },
		{ { 600.0f, 450.0f }, { 0.0f, 0.0f, 1.0f, 1.0f }, { 0.0f, 0.0f } }
	};

	m_VertexCount = 3;

	RHI* rhi = Renderer::Get().GetRHI();
	if (rhi)
	{
		// 정점 버퍼 생성
		m_VertexBuffer = rhi->CreateVertexBuffer(vertices, sizeof(vertices), sizeof(SDL_Vertex));
		if (!m_VertexBuffer) return EResult::Fail;
	}

	return EResult::Success;
}

Component* MeshRenderer::Create(void* arg)
{
	return nullptr;
}

Component* MeshRenderer::Clone(GameObject* owner, void* arg)
{
	return nullptr;
}

void MeshRenderer::Free()
{
	__super::Free();
}
#pragma endregion

#pragma region Loop
void MeshRenderer::Update(f32 dt)
{
}

EResult MeshRenderer::Render(f32 dt)
{
	RHI* rhi = Renderer::Get().GetRHI();
	if (!rhi || !m_VertexBuffer) return EResult::Fail;
	rhi->BindVertexBuffer(m_VertexBuffer);
	rhi->Draw(m_VertexCount);
	return EResult::Success;
}
#pragma endregion

