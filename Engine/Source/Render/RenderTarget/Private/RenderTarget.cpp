#pragma once

#include "RenderTarget.h"
#include "Runtime.h"
#include "Renderer.h"


#pragma region Constructor&Destructor
EResult RenderTarget::Initialize(void* arg) 
{
	if (!arg) return EResult::InvalidArgument;
	
	CAST_DESC
	m_Desc.Format = desc->Format;
	m_Desc.Usage = desc->Usage;
	m_Desc.BindFlag = desc->BindFlag;
	m_Desc.Type = desc->Type;
	m_Desc.TextureType = desc->TextureType;
	m_Desc.Width = desc->Width;
	m_Desc.Height = desc->Height;
	m_Desc.ClearColor = desc->ClearColor;
	m_Desc.Name = desc->Name;

	RHI* rhi = Renderer::Get().GetRHI();
	if(m_Desc.Type == ERenderTargetType::DepthStencil)
	{
		m_Texture = rhi->CreateDepthStencilTexture(desc, m_Desc.Width, m_Desc.Height, 1, 1);
	}
	else
		m_Texture = rhi->CreateRenderTargetTexture(desc, m_Desc.Width, m_Desc.Height, 1, 1);
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
	Safe_Release(m_Texture);
}
#pragma endregion

#pragma region Texture Management
EResult RenderTarget::Resize(uint32 width, uint32 height)
{
	if (m_Desc.Width == width && m_Desc.Height == height)
		return EResult::Success;
	m_Desc.Width = width;
	m_Desc.Height = height;
	Safe_Release(m_Texture);
	RHI* rhi = Renderer::Get().GetRHI();
	m_Texture = rhi->CreateRenderTargetTexture(&m_Desc, m_Desc.Width, m_Desc.Height, 1, 1);
	return EResult::Success;
}
#pragma endregion

