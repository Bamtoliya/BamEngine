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
	void SubmitGrid(class Camera* camera, bool isOrthographic, const wstring& colorRTName, const wstring& depthStencilName);
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