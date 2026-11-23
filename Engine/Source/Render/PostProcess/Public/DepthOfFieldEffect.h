#pragma once

#include "PostProcessInterface.h"
#include <array>

BEGIN(Engine)

class RHIPipeline;

// 피사계 심도. 절반 해상도에서 앞뒤를 따로 흐린 뒤 전체 해상도 CoC로 합성한다.
// 장면 입력(G-Buffer 위치·알베도, 카메라)이 없는 프레임은 건너뛴다.
class ENGINE_API DepthOfFieldEffect : public IPostProcessEffect
{
public:
	DepthOfFieldEffect();
	virtual ~DepthOfFieldEffect();

public:
	virtual void Initialize(const wstring& prefix) override;
	virtual void Free() override;
	virtual EResult SubmitPass(f32 dt, RenderPassID passID, const wstring& sourceRT) override;
	virtual void SetEnabled(bool enabled) override;
	virtual bool WritesHdr() const override { return true; }
	virtual void GetPrePasses(vector<RenderPassID>& outPasses) const override;
	virtual void SetSceneInputs(const PostProcessSceneInputs* inputs) override;

public:
	virtual const std::unordered_map<string, MaterialParameter>& GetParameters() const override { return m_Parameters; }
	virtual void SetFloat(const string& name, f32 value) override;
	virtual void SetInt(const string& name, int32 value) override;
	virtual void ResetParameter(const string& name) override;
	virtual void ResetAllParameters() override;

	const DepthOfFieldSettings& GetSettings() const { return m_Settings; }

private:
	// 보조 패스 실행 순서
	enum EPrePass : uint32 { Prefilter, NearTile, NearDilate, FarBlur, NearBlur, PrePassCount };

	void AddFloat(const string& name, f32 defaultValue, f32 minValue, f32 maxValue, const string& localKey);
	void SyncSettings();
	bool EnsureTargets(uint32 width, uint32 height);
	void SetPrePassesEnabled(bool enabled);

private:
	wstring m_Prefix;
	std::unordered_map<string, MaterialParameter> m_Parameters;
	DepthOfFieldSettings m_Settings;

	PostProcessSceneInputs m_SceneInputs;
	bool m_HasSceneInputs = false;

	// 절반 해상도 색·CoC, 앞쪽 CoC 타일 두 장, 뒤쪽·앞쪽 흐림 결과
	wstring m_HalfRTName;
	wstring m_TileRTName;
	wstring m_DilatedTileRTName;
	wstring m_FarRTName;
	wstring m_NearRTName;

	std::array<RenderPassID, PrePassCount> m_PrePassIDs = {
		INVALID_PASS_ID, INVALID_PASS_ID, INVALID_PASS_ID, INVALID_PASS_ID, INVALID_PASS_ID
	};

	RHIPipeline* m_PrefilterPipeline = nullptr;
	RHIPipeline* m_TilePipeline = nullptr;
	RHIPipeline* m_DilatePipeline = nullptr;
	RHIPipeline* m_BlurPipeline = nullptr;
	RHIPipeline* m_CompositePipeline = nullptr;
};

END
