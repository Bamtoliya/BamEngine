#pragma once

#include "Editor_Includes.h"
#include "PostProcessPipeline.h"

BEGIN(Editor)

struct ViewportPassInfo
{
	wstring Name;
	Engine::RenderPassID PassID = INVALID_PASS_ID;
	bool    Enabled = true;
};

class PostProcessChain
{
public:
	PostProcessChain() = default;
	~PostProcessChain() = default;

public:
	void Initialize(const wstring& prefix) { m_Pipeline.Initialize(prefix); }
	void Free() { m_Pipeline.Free(); }

	void PrepareResources(uint32 width, uint32 height) { m_Pipeline.PrepareResources(width, height); }
	void Resize(uint32 width, uint32 height) { m_Pipeline.Resize(width, height); }

	// 입력 RT를 받아 최종 출력 RT 반환
	wstring ExecuteChain(f32 dt, const wstring& inputRT) { return m_Pipeline.Execute(dt, inputRT); }
	void DrawImGuiMenu();

	Engine::PostProcessPipeline& GetPipeline() { return m_Pipeline; }
	const Engine::PostProcessPipeline& GetPipeline() const { return m_Pipeline; }

private:
	Engine::PostProcessPipeline m_Pipeline;
};

END