#pragma once

#include "RenderPass.h"

#pragma region Constructor&Destructor
EResult RenderPass::Initialize(void* arg)
{
	if (!arg) return EResult::InvalidArgument;
	CAST_DESC
	m_Desc.priority = desc->priority;
	m_Desc.ID = desc->ID;
	m_Desc.name = desc->name;
	m_Desc.depthStencilName = desc->depthStencilName;
	m_Desc.renderTargetNames = desc->renderTargetNames;
	m_Desc.sortType = desc->sortType;
	m_Desc.loadOperation = desc->loadOperation;
	m_Desc.storeOperation = desc->storeOperation;
	m_Desc.stencilLoadOperation = desc->stencilLoadOperation;
	m_Desc.stencilStoreOperation = desc->stencilStoreOperation;
	m_Desc.overrideClearColor = desc->overrideClearColor;
	m_Desc.passType = desc->passType;
	m_Desc.acceptedBlendModes = desc->acceptedBlendModes;
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