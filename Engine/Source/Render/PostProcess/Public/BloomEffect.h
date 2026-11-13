#pragma once

#include "PostProcessInterface.h"
#include <array>

BEGIN(Engine)

class RHIPipeline;

class ENGINE_API BloomEffect : public IPostProcessEffect
{
public:
	static constexpr uint32 BLOOM_MIP_LEVELS = 4; // 1/2, 1/4, 1/8, 1/16 피라미드

public:
	BloomEffect();
	virtual ~BloomEffect();

public:
	virtual void Initialize(const wstring& prefix) override;
	virtual void Free() override;
	virtual EResult SubmitPass(f32 dt, RenderPassID passID, const wstring& sourceRT) override;
	virtual void SetEnabled(bool enabled) override;

public:
	// 공통 파라미터 인터페이스 구현
	virtual const std::unordered_map<string, MaterialParameter>& GetParameters() const override { return m_Parameters; }
	virtual void SetFloat(const string& name, f32 value) override;
	virtual void ResetParameter(const string& name) override;
	virtual void ResetAllParameters() override;

private:
	void AddFloat(const string& name, f32 defaultVal, f32 minVal, f32 maxVal, const string& localKey);
	void CreateMipRenderTargets(uint32 baseWidth, uint32 baseHeight);

private:
	wstring m_Prefix;

	// 파라미터 컨테이너
	std::unordered_map<string, MaterialParameter> m_Parameters;

	// 캐시 변수
	f32 m_Threshold = 1.0f;
	f32 m_SoftKnee = 0.5f;
	f32 m_Intensity = 1.0f;
	f32 m_Scatter = 1.0f;

	// U자 밉 렌더타깃 (1/2, 1/4, 1/8, 1/16)
	wstring m_DownRTNames[BLOOM_MIP_LEVELS];
	uint32  m_MipWidths[BLOOM_MIP_LEVELS] = { 0 };
	uint32  m_MipHeights[BLOOM_MIP_LEVELS] = { 0 };

	// 서브 렌더패스 IDs
	RenderPassID m_PrefilterPassID = INVALID_PASS_ID;
	std::array<RenderPassID, BLOOM_MIP_LEVELS - 1> m_DownPassIDs = {
		INVALID_PASS_ID, INVALID_PASS_ID, INVALID_PASS_ID
	};
	std::array<RenderPassID, BLOOM_MIP_LEVELS - 1> m_UpPassIDs = {
		INVALID_PASS_ID, INVALID_PASS_ID, INVALID_PASS_ID
	};

	// 파이프라인 PSOs
	RHIPipeline* m_PrefilterPipeline = nullptr;
	RHIPipeline* m_DownsamplePipeline = nullptr;
	RHIPipeline* m_UpsamplePipeline = nullptr;
	RHIPipeline* m_CompositePipeline = nullptr;
};

END
