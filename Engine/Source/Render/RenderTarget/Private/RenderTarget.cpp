#pragma once

#include "RenderTarget.h"
#include "Runtime.h"
#include "Renderer.h"


#pragma region Constructor&Destructor
EResult RenderTarget::Initialize(void* arg) 
{
	if (!arg) return EResult::InvalidArgument;
	CAST_DESC
	m_Name = desc->name;
	m_FixedSize = desc->fixedSize;
	m_SizeScale = desc->sizeScale;
	RHI* rhi = Renderer::Get().GetRHI();
	m_Texture = rhi->CreateTexture(desc->textureDesc);
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
	if (m_Texture->GetWidth() == width && m_Texture->GetHeight() == height)
		return EResult::Success;
	RHITextureDesc desc = m_Texture->GetDesc();
	desc.width = width;
	desc.height = height;
	Safe_Release(m_Texture);
	RHI* rhi = Renderer::Get().GetRHI();
	m_Texture = rhi->CreateTexture(desc);
	return EResult::Success;
}
#pragma endregion

