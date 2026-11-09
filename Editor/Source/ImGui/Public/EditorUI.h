#pragma once

#include <imgui.h>

namespace EditorUI
{
    struct DialogAction
    {
        const char* LabelKey;
        bool Enabled = true;
        bool Destructive = false;
    };

    struct DialogButtonBarOptions
    {
        int CancelIndex = 0; // -1이면 Escape로 선택하지 않는다.
    };

    // 현재 가용 폭에서 줄바꿈을 포함한 버튼 바 높이. 본문 높이 예약에 사용한다.
    float GetDialogButtonBarHeight(const DialogAction* actions, int count);

    // 첫 항목은 취소, 마지막은 기본 확인이다. 반환값은 선택 번호 또는 -1이며 실제 처리는 호출자가 담당한다.
    int DialogButtonBar(const char* id, const DialogAction* actions, int count, const DialogButtonBarOptions& options = {});

    struct IconButtonOptions
    {
        bool Enabled = true;
        bool Active = false;
        bool SameLine = false;
        bool Compact = false; // 표/리스트 행의 작은 버튼.
        ImVec2 Size = ImVec2(0.0f, 0.0f); // 고정 영역에서 사용할 최소 크기. 아이콘/패딩보다 작아지지 않는다.
        const char* ShortcutHint = nullptr;
    };

    ImVec2 GetIconButtonSize(const char* icon, bool compact = false);
    // 동일한 크기의 아이콘 슬롯을 가용 폭에 맞춰 배치할 때의 전체 크기.
    ImVec2 MeasureIconGrid(ImVec2 buttonSize, int count, float availableWidth);
    bool SmallIconButton(const char* id, const char* icon, const char* tooltipKey, bool enabled = true);
    // 직전 항목 뒤에 필요한 폭이 남아 있을 때만 같은 줄로 이동한다.
    void SameLineIfFits(float width);
    bool IconButton(const char* id, const char* icon, const char* tooltipKey, const IconButtonOptions& options = {});

    // Draw 호출마다 지역 객체로 만든다. 입력 상태나 실행 핸들러를 보관하지 않는다.
    class ToolbarLayout
    {
    public:
        bool Icon(const char* id, const char* icon, const char* tooltipKey, const IconButtonOptions& options = {});
        bool Button(const char* id, const char* icon, const char* labelKey);
        void SetNextItemWidth(float preferredWidth);
        void Text(const char* text, bool disabled = true);

    private:
        bool m_HasItem = false;
    };

    struct SearchBarOptions
    {
        float Width = 0.0f; // 전체 너비. 0 이하면 가용 너비를 사용한다.
        bool FocusOnDraw = false;
    };

    struct SearchBarResult
    {
        bool Changed = false;
        bool Submitted = false;
        bool Cleared = false;
    };

    SearchBarResult DrawSearchBar(const char* id, char* buffer, size_t bufferSize,
        const char* hintKey = "EditorCommon.Search", const SearchBarOptions& options = {});

    // 버퍼는 호출자가 소유한다. 내용 변경 시 true를 반환하며 필터 갱신은 호출자가 담당한다.
    bool SearchBar(const char* id, char* buffer, size_t bufferSize, const char* hintKey = "EditorCommon.Search");

    // 남은 영역 중앙에 로캘 안내를 표시한다. 좁은 폭에서는 문구를 줄바꿈한다.
    void EmptyState(const char* messageKey);

    struct TableStyle
    {
        float PaddingXInFontUnits = 0.6f;
        float PaddingYInFontUnits = 0.4f;
        bool Resizable = true;
    };

    struct PropertyTableStyle : TableStyle
    {
        float LabelWidth = 180.0f;
    };

    // 열 구성은 호출자가 결정한다. 성공한 경우에만 EndSettingsTable을 호출한다.
    bool BeginSettingsTable(const char* id, int columns, ImGuiTableFlags flags, const TableStyle& style = {});
    void EndSettingsTable();

    // true를 반환했을 때만 내용을 그리고 EndPropertyTable을 호출한다.
    bool BeginPropertyTable(const char* id, const PropertyTableStyle& style = {});
    void EndPropertyTable();

    // 새 행의 첫 번째 열에 라벨을 그리고 두 번째 열로 이동한다.
    void PropertyRow(const char* labelKey);
}
