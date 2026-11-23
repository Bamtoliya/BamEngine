#pragma once

#include "Engine_Includes.h"
#include "RenderPassTypes.h"
#include "MaterialInterface.h"

BEGIN(Engine)

class IPostProcessEffect
{
public:
	IPostProcessEffect(const wstring& name, const string& displayNameKey = "")
		: m_Name(name), m_DisplayNameKey(displayNameKey), m_Enabled(false) {
	}
	virtual ~IPostProcessEffect() = default;

public:
	virtual void Initialize(const wstring& prefix) = 0;
	virtual void Free() = 0;
	virtual EResult SubmitPass(f32 dt, RenderPassID passID, const wstring& sourceRT) = 0;
	virtual void SetEnabled(bool enable) { m_Enabled = enable; }
	// 톤매핑 전이라 1보다 밝은 값을 쓰는 효과. PostProcessPipeline이 HDR 타깃에 쓰게 한다.
	virtual bool WritesHdr() const { return false; }
	// 이전 프레임 값을 쓰는 효과가 그 값을 버린다. 씬 전환·크기 변경에서 PostProcessPipeline이 호출한다.
	virtual void ResetHistory() {}

public:
	const wstring& GetName() const { return m_Name; }
	const string& GetDisplayNameKey() const { return m_DisplayNameKey; }
	bool IsEnabled() const { return m_Enabled; }
	void Toggle() { m_Enabled = !m_Enabled; }

public:
	// ── 공통 파라미터 제어 인터페이스 (모든 후처리 효과 일원화) ──
	virtual const std::unordered_map<string, MaterialParameter>& GetParameters() const = 0;
	virtual void SetFloat(const string& name, f32 value) {}
	virtual void SetFloat2(const string& name, const vec2& value) {}
	virtual void SetFloat3(const string& name, const vec3& value) {}
	virtual void SetFloat4(const string& name, const vec4& value) {}
	virtual void SetInt(const string& name, int32 value) {}
	virtual void SetBool(const string& name, bool value) {}
	virtual void ResetParameter(const string& name) {}
	virtual void ResetAllParameters() {}

protected:
	wstring m_Name;
	string  m_DisplayNameKey;
	bool    m_Enabled;
};

END