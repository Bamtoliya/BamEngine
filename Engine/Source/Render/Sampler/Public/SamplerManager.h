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
	RHISampler* GetOrCreateSampler(const tagRHISamplerDesc& desc);
	RHISampler* GetDefaultSampler() const { return m_DefaultSampler; }
	void SetDefaultSampler(RHISampler* defaultSampler) { m_DefaultSampler = defaultSampler; }
#pragma endregion

#pragma region Variable
private:
	unordered_map<tagRHISamplerDesc, RHISampler*> m_Samplers;
	RHISampler* m_DefaultSampler = { nullptr };
	RHI* m_RHI = { nullptr };
#pragma endregion
};
END