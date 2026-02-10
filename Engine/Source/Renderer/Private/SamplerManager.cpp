#pragma once

IMPLEMENT_SINGLETON(SamplerManager)

#include "SamplerManager.h"

#pragma region Constructor&Destructor
EResult SamplerManager::Initialize(void* arg)
{
	tagRHISamplerDesc defaultDesc;
	defaultDesc.MinFilter = ESamplerFilter::Linear;
	defaultDesc.MagFilter = ESamplerFilter::Linear;
	defaultDesc.MipFilter = ESamplerFilter::Linear;
	defaultDesc.AddressU = ESamplerAddressMode::Wrap;
	defaultDesc.AddressV = ESamplerAddressMode::Wrap;
	defaultDesc.AddressW = ESamplerAddressMode::Wrap;

	m_DefaultSampler = CreateSampler(L"LinearWrap", defaultDesc);
	Safe_AddRef(m_DefaultSampler);
	if (!m_DefaultSampler) return EResult::Fail;

	// 자주 쓰이는 프리셋 추가
	tagRHISamplerDesc linearClampDesc;
	linearClampDesc.MinFilter = ESamplerFilter::Linear;
	linearClampDesc.MagFilter = ESamplerFilter::Linear;
	linearClampDesc.AddressU = ESamplerAddressMode::Clamp;
	linearClampDesc.AddressV = ESamplerAddressMode::Clamp;
	linearClampDesc.AddressW = ESamplerAddressMode::Clamp;
	CreateSampler(L"LinearClamp", linearClampDesc);

	tagRHISamplerDesc pointWrapDesc;
	pointWrapDesc.MinFilter = ESamplerFilter::Point;
	pointWrapDesc.MagFilter = ESamplerFilter::Point;
	pointWrapDesc.AddressU = ESamplerAddressMode::Wrap;
	pointWrapDesc.AddressV = ESamplerAddressMode::Wrap;
	pointWrapDesc.AddressW = ESamplerAddressMode::Wrap;
	CreateSampler(L"PointWrap", pointWrapDesc);

	return EResult::Success;
}
SamplerManager* SamplerManager::Create(RHI* rhi, void* arg)
{
	if (m_instance) return m_instance;

	SamplerManager* instance = new SamplerManager(rhi);
	if (IsFailure(instance->Initialize(arg)))
	{
		Safe_Release(instance);
		return nullptr;
	}
	m_instance = instance;
	return instance;
}
void SamplerManager::Free()
{
	for (auto& pair : m_Samplers)
	{
		Safe_Release(pair.second);
	}
	m_Samplers.clear();
	Safe_Release(m_DefaultSampler);
	Safe_Release(m_RHI);
	__super::Free();
}
#pragma endregion

#pragma region Sampler Management
RHISampler* SamplerManager::CreateSampler(const wstring& name, const tagRHISamplerDesc& desc)
{
	auto it = m_Samplers.find(name);
	if (it != m_Samplers.end())
	{
		return it->second;
	}
	RHISampler* sampler = m_RHI->CreateSampler(desc);
	if (sampler)
	{
		m_Samplers[name] = sampler;
		return sampler;
	}
	return nullptr;
}
RHISampler* SamplerManager::GetSampler(const wstring& name) const
{
	auto it = m_Samplers.find(name);
	if (it != m_Samplers.end())
	{
		return it->second;
	}
	return nullptr;
}
EResult SamplerManager::RemoveSampler(const wstring& name)
{
	auto it = m_Samplers.find(name);
	if (it != m_Samplers.end())
	{
		Safe_Release(it->second);
		m_Samplers.erase(it);
		return EResult::Success;
	}
	return EResult::Fail;
}
RHISampler* SamplerManager::GetDefaultSampler() const
{
	return m_DefaultSampler;
}
#pragma endregion