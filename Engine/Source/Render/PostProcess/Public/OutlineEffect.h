#pragma once

#include "PostProcessInterface.h"

BEGIN(Engine)

class RHIPipeline;

// 화면 공간 깊이·노멀 불연속성 기반 아웃라인 후처리 효과.
// 장면 입력(G-Buffer 위치·알베도, 선택적 법선, 카메라)이 없는 프레임은 건너뛴다.
class ENGINE_API OutlineEffect : public IPostProcessEffect
{
public:
	OutlineEffect();
	virtual ~OutlineEffect();

public:
	virtual void Initialize(const wstring& prefix) override;
	virtual void Free() override;
	virtual EResult SubmitPass(f32 dt, RenderPassID passID, const wstring& sourceRT) override;
	virtual void SetSceneInputs(const PostProcessSceneInputs* inputs) override;

public:
	virtual const std::unordered_map<string, MaterialParameter>& GetParameters() const override { return m_Parameters; }
	virtual void SetFloat(const string& name, f32 value) override;
	virtual void SetFloat4(const string& name, const vec4& value) override;
	virtual void ResetParameter(const string& name) override;
	virtual void ResetAllParameters() override;

private:
	void AddFloat(const string& name, f32 defaultValue, f32 minValue, f32 maxValue, const string& localKey);
	void AddFloat4(const string& name, const vec4& defaultValue, const string& localKey, bool isColor);
	void SyncSettings();

private:
	std::unordered_map<string, MaterialParameter> m_Parameters;

	f32  m_Thickness = 1.0f;
	f32  m_DepthThreshold = 0.1f;
	f32  m_NormalThreshold = 0.4f;
	vec4 m_OutlineColor{ 0.0f, 0.0f, 0.0f, 1.0f };
	f32  m_Intensity = 1.0f;

	PostProcessSceneInputs m_SceneInputs;
	bool m_HasSceneInputs = false;

	RHIPipeline* m_Pipeline = nullptr;
};

END
