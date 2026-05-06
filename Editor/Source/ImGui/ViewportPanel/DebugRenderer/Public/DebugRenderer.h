#pragma once

#include "Editor_Includes.h"
#include "RenderPass.h"
#include "Vertex.h"

BEGIN(Editor)
class DebugRenderer : public Base
{
public:
	DebugRenderer() = default;
	~DebugRenderer() = default;
public:
	void Initialize(const wstring& prefix);
	void Free();
	void SubmitDebugDraw(class Camera* camera, const wstring& colorRTName);
private:
	void RegisterColliderDebug(class Camera*);
public:
	void DrawLine(const vec3& start, const vec3& end, const vec4& color);
	void DrawRect(const Engine::Rect& rect, const vec4& color);
	void DrawBox(const vec3& center, const vec3& extent, const vec4& color, const mat4& transform = mat4(1.0f));
	void DrawSphere(const vec3& center, float radius, const vec4& color);

public:
	bool* GetDrawCollidersPtr() { return &m_DrawColliders; }
	bool IsDrawColliders() const { return m_DrawColliders; }
	void SetDrawColliders(bool draw) { m_DrawColliders = draw; }
	void ShowCollider() { m_DrawColliders = true; }
	void HideCollider() { m_DrawColliders = false; }
	void ToggleDrawColliders() { m_DrawColliders = !m_DrawColliders; }

private:
	vector<Engine::DebugVertex> m_Vertices;
	class RHIBuffer* m_VertexBuffer = { nullptr };
	bool m_DrawColliders = { false };
	uint32 m_MaxLines = 1000;
	RenderPassID m_RenderPassID = { INVALID_PASS_ID };
};
END