#pragma once

#include "RenderPass.h"

#pragma region Constructor&Destructor
EResult RenderPass::Initialize(void* arg)
{
	if (!arg) return EResult::InvalidArgument;
	CAST_DESC
	m_Desc = *desc;
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
