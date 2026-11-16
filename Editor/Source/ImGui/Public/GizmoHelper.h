#pragma once

#include "Editor_Includes.h"
#include "CameraBuffer.h"

#pragma push_macro("COLOR")
#undef COLOR
#include "ImGuizmo.h"
#pragma pop_macro("COLOR")

#include <imgui.h>

namespace Editor
{
	// 기즈모를 호스팅하는 뷰포트들이 구현하는 인터페이스
	class IGizmoHost
	{
	public:
		virtual ~IGizmoHost() = default;
		virtual void SetGizmoOperation(ImGuizmo::OPERATION op) = 0;
		virtual ImGuizmo::OPERATION GetGizmoOperation() const = 0;
		virtual void ToggleGizmoMode() = 0;
		virtual ImGuizmo::MODE GetGizmoMode() const = 0;
		virtual void ToggleGizmoSnap() = 0;
		virtual bool GetGizmoUseSnap() const = 0;
	};

	struct GizmoResult
	{
		bool Changed = false;
		bool DragStarted = false;
		bool DragEnded = false;
		bool IsUsing = false;
		bool IsOver = false;

		Engine::vec3 Position{ 0.0f };
		Engine::vec3 Rotation{ 0.0f };
		Engine::vec3 Scale{ 1.0f };
	};

	struct GizmoParams
	{
		ImGuizmo::OPERATION Operation = ImGuizmo::TRANSLATE;
		ImGuizmo::MODE Mode = ImGuizmo::LOCAL;
		bool UseSnap = false;
		float CustomSnapStep = 0.0f;
		bool AllowInput = true;
		Engine::mat4 ParentWorldMatrix{ 1.0f };
	};

	class GizmoHelper
	{
	public:
		// 1. 활성 기즈모 호스트 (ShortcutManager 단축키 바인딩용)
		static void SetActiveHost(IGizmoHost* host) { s_ActiveHost = host; }
		static IGizmoHost* GetActiveHost() { return s_ActiveHost; }

		// 2. 뷰포트 사각형 및 카메라 매트릭스 셋업
		static void BeginViewport(const CameraBuffer& camera, const ImVec2& screenOrigin, const ImVec2& size, bool isOrtho = false);

		// 3. 기즈모 조작 실행
		static GizmoResult Manipulate(const char* gizmoId, Engine::mat4& inOutWorldMatrix,
			const GizmoParams& params, bool& inOutWasUsingTracker);

		// 4. 블렌더 스타일 플로팅 오버레이 툴바
		static void DrawGizmoToolbar(IGizmoHost* host, const ImVec2& screenOrigin, const ImVec2& size, bool vertical = true);

		// 5. 3D 사영 및 와이어프레임 헬퍼
		static bool WorldToScreen(const Engine::vec3& worldPos, const Engine::mat4& viewProj,
			const ImVec2& screenOrigin, const ImVec2& size, ImVec2& outScreenPos);

		static void DrawWireCircle(ImDrawList* drawList, const Engine::vec3& center,
			const Engine::vec3& uAxis, const Engine::vec3& vAxis, float radius,
			const Engine::mat4& viewProj, const ImVec2& screenOrigin, const ImVec2& size,
			ImU32 color, int segments = 32, float thickness = 1.5f);

		static void DrawWireSphere(ImDrawList* drawList, const Engine::vec3& center, float radius,
			const Engine::mat4& viewProj, const ImVec2& screenOrigin, const ImVec2& size,
			ImU32 color, int segments = 32, float thickness = 1.5f);

		static void DrawWireBox(ImDrawList* drawList, const Engine::vec3& center, const Engine::vec3& halfExtent,
			const Engine::mat4& orientation, const Engine::mat4& viewProj,
			const ImVec2& screenOrigin, const ImVec2& size, ImU32 color, float thickness = 1.5f);

	private:
		inline static IGizmoHost* s_ActiveHost = nullptr;
		inline static CameraBuffer s_Camera{};
		inline static ImVec2 s_ScreenOrigin{ 0.0f, 0.0f };
		inline static ImVec2 s_ViewportSize{ 0.0f, 0.0f };
		inline static bool s_IsOrtho = false;
	};
}