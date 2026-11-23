#pragma once

#include "Engine_Includes.h"
#include "RenderPassTypes.h"
#include "MaterialInterface.h"
#include "CameraBuffer.h"
#include "DepthOfField.h"
#include <optional>

BEGIN(Engine)

// 장면 깊이가 필요한 효과(피사계 심도)가 읽는 입력. G-Buffer가 없는 썸네일·프리뷰는 넘기지 않는다.
struct PostProcessSceneInputs
{
	wstring PositionRT;  // G-Buffer 위치(월드 좌표)
	wstring DiffuseRT;   // G-Buffer 알베도. a가 0.01보다 작으면 하늘이다.
	CameraBuffer Camera{};
	// 메인 카메라가 피사계 심도를 덮어쓸 때만 값이 있다.
	std::optional<DepthOfFieldSettings> CameraDepthOfField;

	bool IsValid() const { return !PositionRT.empty() && !DiffuseRT.empty(); }
};

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
	// 실패하면 아무 명령도 제출하지 않아야 한다. PostProcessPipeline은 그 효과를 이번 프레임에서 건너뛴다.
	virtual EResult SubmitPass(f32 dt, RenderPassID passID, const wstring& sourceRT) = 0;
	virtual void SetEnabled(bool enable) { m_Enabled = enable; }
	// 톤매핑 전이라 1보다 밝은 값을 쓰는 효과. PostProcessPipeline이 HDR 타깃에 쓴다.
	virtual bool WritesHdr() const { return false; }
	// 슬롯 패스 앞에 실행해야 하는 보조 패스. 실행 순서대로 넣는다(같은 우선순위에서는 먼저 등록한 패스가 먼저 실행된다).
	// PostProcessPipeline이 체인 안의 위치에 맞춰 우선순위를 정한다.
	virtual void GetPrePasses(vector<RenderPassID>& outPasses) const {}
	// SubmitPass 직전에 호출된다. nullptr이면 장면 입력이 없다. 포인터는 이 프레임의 Execute 동안만 유효하다.
	virtual void SetSceneInputs(const PostProcessSceneInputs* inputs) {}

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
