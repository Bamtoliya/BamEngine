#pragma once

#include "PostProcessInterface.h"

BEGIN(Engine)

class RHIBuffer;
class RHIPipeline;

// 톤매핑 전 HDR 장면의 밝기 히스토그램으로 노출을 정하고 시간에 걸쳐 맞춘 뒤 장면 색에 곱한다.
// 히스토그램과 이전 노출은 효과(=뷰)마다 GPU에 둔다. Compute UAV를 쓰므로 DirectX12에서만 동작한다.
class ENGINE_API AutoExposureEffect : public IPostProcessEffect
{
public:
	AutoExposureEffect();
	virtual ~AutoExposureEffect();

public:
	virtual void Initialize(const wstring& prefix) override;
	virtual void Free() override;
	virtual EResult SubmitPass(f32 dt, RenderPassID passID, const wstring& sourceRT) override;
	virtual void SetEnabled(bool enabled) override;
	virtual bool WritesHdr() const override { return true; }
	virtual void ResetHistory() override { m_ResetPending = true; }

public:
	virtual const std::unordered_map<string, MaterialParameter>& GetParameters() const override { return m_Parameters; }
	virtual void SetFloat(const string& name, f32 value) override;
	virtual void ResetParameter(const string& name) override;
	virtual void ResetAllParameters() override;

private:
	void AddFloat(const string& name, f32 defaultVal, f32 minVal, f32 maxVal, const string& localKey);
	// 셰이더·파이프라인·히스토그램 버퍼·노출 텍스처를 처음 쓸 때 만든다. 하나라도 없으면 false.
	bool EnsureResources();

private:
	wstring m_Prefix;
	std::unordered_map<string, MaterialParameter> m_Parameters;

	f32 m_MinEV = -4.0f;
	f32 m_MaxEV = 4.0f;
	f32 m_CompensationEV = 0.0f;
	f32 m_SpeedToLight = 3.0f;
	f32 m_SpeedToDark = 1.0f;

	wstring m_ExposureRTName;            // 1x1 R32_FLOAT, 이전 프레임 노출(EV)
	RHIBuffer* m_HistogramBuffer = nullptr;
	RHIPipeline* m_HistogramPipeline = nullptr;
	RHIPipeline* m_AdaptPipeline = nullptr;
	RHIPipeline* m_ApplyPipeline = nullptr;
	bool m_ResourcesUnavailable = false; // 셰이더가 없거나 백엔드가 지원하지 않으면 다시 시도하지 않는다.
	bool m_ResetPending = true;
};

END
