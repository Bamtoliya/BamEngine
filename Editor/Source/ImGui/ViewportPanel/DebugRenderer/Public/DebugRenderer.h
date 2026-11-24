#pragma once

#include "Editor_Includes.h"
#include "RenderPass.h"
#include "Vertex.h"
#include "RHIBuffer.h"
#include "RenderTargetTypes.h"


BEGIN(Editor)
class DebugRenderer : public Base
{
public:
	DebugRenderer() = default;
	~DebugRenderer() = default;
public:
	void Initialize(const wstring& prefix);
	void Free();
	void SubmitDebugDraw(const CameraBuffer& cameraBuffer, const wstring& colorRTName);
private:
	void RegisterColliderDebug(const CameraBuffer& cameraBuffer);
	void RegisterGameplayDebug(const CameraBuffer& cameraBuffer);
	void RegisterNavGridDebug(const CameraBuffer& cameraBuffer);
public:
	void DrawLine(const vec3& start, const vec3& end, const vec4& color);
	void DrawRect(const Engine::Rect& rect, const vec4& color);
	void DrawBox(const vec3& center, const vec3& extent, const vec4& color, const mat4& transform = mat4(1.0f));
	void DrawSphere(const vec3& center, float radius, const vec4& color);
	void DrawCircleXZ(const vec3& center, float radius, const vec4& color);
	void DrawArcXZ(const vec3& center, float radius, float startAngleRad, float endAngleRad, const vec4& color, uint32 segments = 24);

public:
	bool* GetDrawCollidersPtr() { return &m_DrawColliders; }
	bool IsDrawColliders() const { return m_DrawColliders; }
	void SetDrawColliders(bool draw) { m_DrawColliders = draw; }
	void ShowCollider() { m_DrawColliders = true; }
	void HideCollider() { m_DrawColliders = false; }
	void ToggleDrawColliders() { m_DrawColliders = !m_DrawColliders; }
	bool* GetDrawGameplayGizmosPtr() { return &m_DrawGameplayGizmos; }
	bool IsDrawGameplayGizmos() const { return m_DrawGameplayGizmos; }
	void SetDrawGameplayGizmos(bool draw) { m_DrawGameplayGizmos = draw; }
	bool* GetShowNavGridOverlayPtr() { return &m_ShowNavGridOverlay; }
	bool GetShowNavGridOverlay() const { return m_ShowNavGridOverlay; }
	bool IsShowNavGridOverlay() const { return m_ShowNavGridOverlay; }
	void SetShowNavGridOverlay(bool show) { m_ShowNavGridOverlay = show; }
	void ToggleShowNavGridOverlay() { m_ShowNavGridOverlay = !m_ShowNavGridOverlay; }

private:
	vector<Engine::DebugVertex> m_Vertices;
	Engine::RHIBuffer* m_VertexBuffers[MAX_SWAPCHAIN_BUFFERS] = {};
	bool m_DrawColliders = { false };
	bool m_DrawGameplayGizmos = { true };
	bool m_ShowNavGridOverlay = { false };
	uint32 m_MaxLines = 65536;
	RenderPassID m_RenderPassID = { INVALID_PASS_ID };
};
END