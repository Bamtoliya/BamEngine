#pragma once

#include "Editor_Includes.h"
#include "EnumBit.h"

namespace
{
	static constexpr uint32 MAX_PP_PASSES = 16;

	struct tagViewportPassInfo
	{
		wstring Name;
		bool	Enabled;
	};

	struct tagPPEffect
	{
		wstring Name;
		bool Enabled = false;
		RHIPipeline* Pipeline = nullptr;
		vector<uint8> Parameters;
	};

	struct tagToneMappingParams
	{
		f32 exposure = 1.0f;
		f32 gamma = 2.2f;
		f32 _pad0 = 0.f;
		f32 _pad1 = 0.f;
	};
}

// -------------------------------------------------------------
// [NEW] Object-Oriented PostProcess Architecture
// -------------------------------------------------------------

BEGIN(Editor)

class PostProcess
{
public:
	PostProcess(const wstring& name) : m_Name(name), m_Enabled(false) {}
	virtual ~PostProcess() = default;

public:
	virtual void Initialize(const wstring& prefix) = 0;
	virtual void Free() = 0;
	virtual EResult SubmitPass(f32 dt, Engine::RenderPassID passID, const wstring& sourceRT) = 0;
	virtual void DrawImGuiOptions() = 0;

public:
	const wstring& GetName() const { return m_Name; }
	bool IsEnabled() const { return m_Enabled; }
	void SetEnabled(bool enable) { m_Enabled = enable; }
	void Toggle() { m_Enabled = !m_Enabled; }

protected:
	wstring m_Name;
	bool m_Enabled;
};

// -------------------------------------------------------------

class ToneMapping : public PostProcess
{
public:
	ToneMapping() : PostProcess(L"Tone Mapping (ACES)") {}
	virtual ~ToneMapping() = default;

public:
	virtual void Initialize(const wstring& prefix) override;
	virtual void Free() override;
	virtual EResult SubmitPass(f32 dt, Engine::RenderPassID passID, const wstring& sourceRT) override;
	virtual void DrawImGuiOptions() override;

private:
	Engine::RHIPipeline* m_Pipeline = nullptr;
	tagToneMappingParams m_Params; // 기존 구조체 재활용 (추후 Engine쪽으로 옮겨도 됨)
};

// -------------------------------------------------------------

class PostProcessChain
{
public:
	PostProcessChain() = default;
	~PostProcessChain() = default;

public:
	void Initialize(const wstring& prefix);
	void Free();

	void AddPostProcess(PostProcess* effect);
	void ExecuteChain(f32 dt, const std::vector<Engine::RenderPassID>& passIDs, wstring& currentRT, const wstring* ppRTNames);
	void DrawImGuiMenu();

private:
	std::vector<PostProcess*> m_Effects;
};

END