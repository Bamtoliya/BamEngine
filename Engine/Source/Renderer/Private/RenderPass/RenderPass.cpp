#pragma once

#include "RenderPass.h"

#pragma region Constructor&Destructor
EResult RenderPass::Initialize(void* arg)
{
	if (!arg) return EResult::InvalidArgument;
	CAST_DESC
	m_Desc.Priority = desc->Priority;
	m_Desc.ID = desc->ID;
	m_Desc.Name = desc->Name;
	m_Desc.DepthStencilName = desc->DepthStencilName;
	m_Desc.RenderTargetNames = desc->RenderTargetNames;
	m_Desc.SortType = desc->SortType;
	m_Desc.LoadOperation = desc->LoadOperation;
	m_Desc.StoreOperation = desc->StoreOperation;
	m_Desc.StencilLoadOperation = desc->StencilLoadOperation;
	m_Desc.StencilStoreOperation = desc->StencilStoreOperation;
	m_Desc.OverrideClearColor = desc->OverrideClearColor;
	return EResult::Success;
}
RenderPass* RenderPass::Create(void* arg)
{
	RenderPass* instance = new RenderPass();
	if (IsFailure(instance->Initialize(arg)))
	{
		Safe_Release(instance);
		return nullptr;
	}
	return instance;
}
#pragma endregion