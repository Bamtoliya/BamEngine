#pragma once

#include "RenderTarget.h"
#include "Runtime.h"
#include "Renderer.h"


#pragma region Constructor&Destructor
EResult RenderTarget::Initialize(void* arg) 
{
	if (!arg) return EResult::InvalidArgument;
	
	CAST_DESC
	m_Width = desc->Width;
	m_Height = desc->Height;
	RHI* rhi = Renderer::Get().GetRHI();
	m_Textures.push_back(rhi->CreateRenderTargetTexture(desc, m_Width, m_Height, 1, 1));
	m_DepthStencilTexture = rhi->CreateDepthStencilTexture(desc, m_Width, m_Height, 1, 1);
	return EResult::Success;
}

RenderTarget* RenderTarget::Create(void* arg)
{
	RenderTarget* instance = new RenderTarget();
	if (IsFailure(instance->Initialize(arg)))
	{
		Safe_Release(instance);
		return nullptr;
	}
	return instance;
}

void RenderTarget::Free()
{
	RELEASE_VECTOR(m_Textures);
	Safe_Release(m_DepthStencilTexture);
}
#pragma endregion

#pragma region Texture Management
EResult RenderTarget::Resize(uint32 width, uint32 height)
{
	if (m_Width == width && m_Height == height)
		return EResult::Success;
	m_Width = width;
	m_Height = height;
	//Pool 고려 해봐야함
	RELEASE_VECTOR(m_Textures);
	RHI* rhi = Renderer::Get().GetRHI();
	m_Textures.push_back(rhi->CreateRenderTargetTexture(nullptr, m_Width, m_Height, 1, 1));
	return EResult::Success;
}
#pragma endregion

