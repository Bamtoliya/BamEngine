#pragma once

#include "FloatCurve.h"
#include <string>
#include <functional>

namespace Editor
{
	class CurveEditorModal
	{
	public:
		CurveEditorModal() = default;
		~CurveEditorModal() = default;

		// 커브 편집 모달 열기 (대상 커브 포인터 및 변경 완료 콜백)
		void Open(const std::string& title, Engine::FloatCurve* targetCurve, std::function<void()> onApplied = nullptr);

		// ImGui 렌더 루프에서 호출 (모달이 열려 있을 때 팝업 창 렌더링). 변경 적용 시 true 반환
		bool Draw();

		bool IsOpen() const { return m_IsOpen; }

	private:
		bool DrawGraphCanvas(Engine::FloatCurve& curve, float width, float height);
		bool DrawKeyDetail(Engine::FloatCurve& curve);
		void FitView(const Engine::FloatCurve& curve);

	private:
		bool m_IsOpen = false;
		bool m_JustOpened = false;
		std::string m_Title = "Curve Editor";
		Engine::FloatCurve* m_TargetCurve = nullptr;
		Engine::FloatCurve m_WorkingCurve; // 취소 시 원본 복원을 위한 로컬 작업용 복사본
		std::function<void()> m_OnApplied = nullptr;

		int m_SelectedKeyIndex = -1;
		float m_ViewTimeMin = 0.0f;
		float m_ViewTimeMax = 1.0f;
		float m_ViewValueMin = 0.0f;
		float m_ViewValueMax = 1.0f;
	};
}