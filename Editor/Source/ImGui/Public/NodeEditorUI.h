#pragma once

#include <imgui.h>
#include <imgui_node_editor.h>
#include <string>
#include <vector>

namespace Editor
{
	enum class EWireStyle : int { Curved = 0, ElectronicHybrid = 1, Straight = 2 };
}

namespace Editor::NodeEditorUI
{
	namespace ed = ax::NodeEditor;

	// ── 1. 글로벌 테마 & 에디터 컨텍스트 설정 ──
	void ApplyDefaultTheme(ed::Style& style);
	ed::EditorContext* CreateDefaultEditor(const ed::Config* userConfig = nullptr);

	// ── 2. 노드 구조화 & 헤더 렌더링 ──
	// 노드 내부 상단에 호출하여 제목과 최소 가로폭을 확보하고, 헤더 하단 Y좌표를 반환
	float BeginNodeHeader(const char* title, float minWidth = 150.0f, const char* badge = nullptr, const ImVec4& badgeColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

	// ed::EndNode() 직후 호출하여 상단 라운딩 사각형 배경과 하단 분할선을 배경 드로우 리스트에 렌더링
	void DrawNodeHeader(ed::NodeId nodeId, const ImVec2& nodeMin, const ImVec2& nodeMax, float headerBottom, ImU32 headerColor);

	// ── 3. 노드 테두리 원형 핀 (언리얼 블루프린트 / 1번 사진 표준) ──
	enum class BorderSide
	{
		Left,
		Right
	};

	struct BorderPin
	{
		ed::PinId PinId;
		ed::PinKind Kind = ed::PinKind::Input;
		float CenterY = 0.0f;
		ImU32 Color = IM_COL32(220, 220, 220, 255);
		BorderSide Side = BorderSide::Left;

		BorderPin() = default;
		BorderPin(ed::PinId pinId, ed::PinKind kind, float centerY, ImU32 color)
			: PinId(pinId), Kind(kind), CenterY(centerY), Color(color),
			  Side(kind == ed::PinKind::Input ? BorderSide::Left : BorderSide::Right) {}
		BorderPin(ed::PinId pinId, ed::PinKind kind, float centerY, ImU32 color, BorderSide side)
			: PinId(pinId), Kind(kind), CenterY(centerY), Color(color), Side(side) {}

		BorderPin(int pinId, ed::PinKind kind, float centerY, ImU32 color)
			: PinId(static_cast<uintptr_t>(pinId)), Kind(kind), CenterY(centerY), Color(color),
			  Side(kind == ed::PinKind::Input ? BorderSide::Left : BorderSide::Right) {}
		BorderPin(int pinId, ed::PinKind kind, float centerY, ImU32 color, BorderSide side)
			: PinId(static_cast<uintptr_t>(pinId)), Kind(kind), CenterY(centerY), Color(color), Side(side) {}

		BorderPin(uintptr_t pinId, ed::PinKind kind, float centerY, ImU32 color)
			: PinId(pinId), Kind(kind), CenterY(centerY), Color(color),
			  Side(kind == ed::PinKind::Input ? BorderSide::Left : BorderSide::Right) {}
		BorderPin(uintptr_t pinId, ed::PinKind kind, float centerY, ImU32 color, BorderSide side)
			: PinId(pinId), Kind(kind), CenterY(centerY), Color(color), Side(side) {}
	};

	// 노드 테두리 좌/우에 걸쳐지는 원형 핀들을 일괄 렌더링 (ed::EndNode() 직후 호출)
	void DrawNodeBorderPins(const ImVec2& nodeMin, const ImVec2& nodeMax, const std::vector<BorderPin>& pins);

	// 단일 원형 핀 렌더링
	void DrawCirclePin(ed::PinId pinId, ed::PinKind kind, const ImVec2& center, float radius, ImU32 pinColor);

	// 아이콘 핀 (FontAwesome 화살표 / 실행 재생 아이콘 형태의 핀)
	void DrawIconPin(ed::PinId pinId, ed::PinKind kind, const char* icon, const ImVec4& color, const ImVec2& pivot = ImVec2(0.5f, 0.5f));

	// ── 4. 링크 / 와이어 렌더링 ──
	void DrawLink(ed::LinkId linkId, ed::PinId startPin, ed::PinId endPin, const ImVec4& color, float thickness, bool isFlowing = false);

	// ── 5. 표준 색상 팔레트 상수 ──
	namespace Colors
	{
		inline constexpr ImU32 HeaderDefault     = IM_COL32(40, 85, 145, 240);   // 표준 푸른색 헤더
		inline constexpr ImU32 HeaderEvent       = IM_COL32(185, 40, 40, 240);   // 이벤트 / 시작 노드 (빨간색)
		inline constexpr ImU32 HeaderEntry       = IM_COL32(30, 130, 60, 240);   // Entry 노드 (초록색)
		inline constexpr ImU32 HeaderAnyState    = IM_COL32(105, 66, 145, 240);  // AnyState 노드 (보라색)
		inline constexpr ImU32 HeaderState       = IM_COL32(38, 70, 110, 240);   // 일반 상태 노드 (차분한 네이비 블루)
		inline constexpr ImU32 HeaderDefaultState= IM_COL32(190, 110, 30, 240);  // 기본 상태 노드 (주황/호박색)
		inline constexpr ImU32 HeaderStateActive = IM_COL32(30, 145, 175, 240);  // 활성 재생 상태 (밝은 청록)
		inline constexpr ImU32 HeaderFunction    = IM_COL32(40, 135, 65, 240);   // 순수 함수 (녹색)
		inline constexpr ImU32 HeaderVariable    = IM_COL32(30, 130, 125, 240);  // 변수 (청록색)

		inline constexpr ImU32 PinInput          = IM_COL32(100, 180, 245, 255); // 상태 머신 입력 핀 (밝은 하늘색)
		inline constexpr ImU32 PinOutput         = IM_COL32(245, 175, 65, 255);  // 상태 머신 출력 핀 (황금빛 주황색)
		inline constexpr ImU32 PinEntry          = IM_COL32(75, 215, 115, 255);  // Entry 출력 핀 (에메랄드 그린)
		inline constexpr ImU32 PinAnyState       = IM_COL32(195, 130, 245, 255); // AnyState 출력 핀 (연보라)

		inline const ImVec4 WireDefault          = ImVec4(0.80f, 0.82f, 0.85f, 1.0f); // 일반 데이터 / 전이선
		inline const ImVec4 WireExec             = ImVec4(1.00f, 1.00f, 1.00f, 1.0f); // 실행선 (흰색)
		inline const ImVec4 WireSelected         = ImVec4(0.35f, 0.80f, 1.00f, 1.0f); // 선택된 와이어 (하늘색)
		inline const ImVec4 WireEntry            = ImVec4(0.30f, 0.85f, 0.45f, 1.0f); // Entry 전이 (에메랄드 그린)
		inline const ImVec4 WireAnyState         = ImVec4(0.75f, 0.50f, 0.95f, 1.0f); // AnyState 전이 (연보라)
	}
}
