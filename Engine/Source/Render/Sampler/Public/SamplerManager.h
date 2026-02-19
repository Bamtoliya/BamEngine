#pragma once

#include "Base.h"
#include "RHISampler.h"

BEGIN(Engine)
class ENGINE_API SamplerManager final : public Base
{
	DECLARE_SINGLETON(SamplerManager)
#pragma region Constructor&Destructor
private:
	SamplerManager() {}
	SamplerManager(RHI* rhi) : m_RHI(rhi) { Safe_AddRef(m_RHI); }
	virtual ~SamplerManager() = default;
	EResult Initialize(void* arg = nullptr);
public:
	static SamplerManager* Create(RHI* rhi, void* arg = nullptr);
	virtual void Free() override;
#pragma endregion

#pragma region Sampler Management
public:
	RHISampler* CreateSampler(const wstring& key, const tagRHISamplerDesc& desc);
	RHISampler* GetSampler(const wstring& key) const;
	EResult RemoveSampler(const wstring& key);
	RHISampler* GetDefaultSampler() const;
#pragma endregion

#pragma region Variable
private:
	map<wstring, RHISampler*> m_Samplers;
	RHISampler* m_DefaultSampler = { nullptr };
	RHI* m_RHI = { nullptr };
#pragma endregion
};
END