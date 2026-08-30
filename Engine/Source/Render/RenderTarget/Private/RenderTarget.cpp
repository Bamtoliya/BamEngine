#pragma once

#include "RenderTarget.h"
#include "Runtime.h"
#include "Renderer.h"


#pragma region Constructor&Destructor
EResult RenderTarget::Initialize(void* arg) 
{
	if (!arg) return EResult::InvalidArgument;
	
	CAST_DESC
	m_Desc.format = desc->format;
	m_Desc.usage = desc->usage;
	m_Desc.bindFlag = desc->bindFlag;
	m_Desc.type = desc->type;
	m_Desc.dimension = desc->dimension;
	m_Desc.width = desc->width;
	m_Desc.height = desc->height;
	m_Desc.clearColor = desc->clearColor;
	m_Desc.name = desc->name;

	RHI* rhi = Renderer::Get().GetRHI();
	if(m_Desc.type == ERenderTargetType::DepthStencil)
	{
		m_Texture = rhi->CreateDepthStencilTexture(desc, m_Desc.width, m_Desc.height, 1, 1);
	}
	else
		m_Texture = rhi->CreateRenderTargetTexture(desc, m_Desc.width, m_Desc.height, 1, 1);
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
	if (m_Desc.width == width && m_Desc.height == height)
		return EResult::Success;
	m_Desc.width = width;
	m_Desc.height = height;
	Safe_Release(m_Texture);
	RHI* rhi = Renderer::Get().GetRHI();
	if (HasFlag(m_Desc.usage, ETextureUsage::DepthStencilTarget))
	{
		m_Texture = rhi->CreateDepthStencilTexture(&m_Desc, m_Desc.width, m_Desc.height, 1, 1);
	}
	else 
	{
		m_Texture = rhi->CreateRenderTargetTexture(&m_Desc, m_Desc.width, m_Desc.height, 1, 1);
	}
	
	return EResult::Success;
}
#pragma endregion

