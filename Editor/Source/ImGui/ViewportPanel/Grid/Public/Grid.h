#pragma once

#include "Editor_Includes.h"

BEGIN(Editor)
class Grid : public Base
{
public:
	Grid() = default;
	~Grid() = default;
public:
	void Initialize(const wstring& prefix);
private:
	void PrepareShaders();
	void PrepareRenderPass(const wstring& prefix);
public:
	void Free();
	// depthJitterNDC: 깊이 타깃을 그린 카메라의 TAA 지터. 격자 무늬는 원래 카메라로 그리고, 깊이만 깊이 타깃과 같은 화면 위치로 계산한다.
	void SubmitGrid(const CameraBuffer& cameraBuffer, bool isOrthographic, const wstring& colorRTName, const wstring& depthStencilName, const vec2& depthJitterNDC = vec2(0.0f));
public:
	bool* GetVisible() { return &m_Visible; }
	void Show() { m_Visible = true; }
	void Hide() { m_Visible = false; }
	void Toggle() { m_Visible = !m_Visible; }
public:
	void EnableDepthTest() { m_DepthTest = true; }
	void DisableDepthTest() { m_DepthTest = false; }

private:
	bool m_Visible = true;
	bool m_ShowAxis = true;
	bool m_DepthTest = true;
private:
	uint32 m_GridSize = 100;
private:
	RenderPassID m_GridPassID = { INVALID_PASS_ID };
};
END