#pragma once

#include "Base.h"
#include "RenderPass.h"
#include "RHIPipeline.h"

BEGIN(Editor)
class Grid : public Base
{
public:
	Grid() = default;
	~Grid() = default;
public:
	void Initialize();
	void Free();
	void SubmitGrid(RenderPassID renderPassID, bool isOrthographic);
public:
	void Show() { m_Visible = true; }
	void Hide() { m_Visible = false; }
	void Toggle() { m_Visible = !m_Visible; }
public:
	void SetOrthographic() { m_IsOrthographic = true; }
	void SetPerspective() { m_IsOrthographic = false; }
public:
	void EnableDepthTest() { m_DepthTest = true; }
	void DisableDepthTest() { m_DepthTest = false; }

private:
	bool m_Visible = true;
	bool m_ShowAxis = true;
	bool m_IsOrthographic = true;
	bool m_DepthTest = true;
private:
	uint32 m_GridSize = 100;
};
END