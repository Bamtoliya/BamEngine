#pragma once

#include "Editor_Includes.h"
#include "ShortcutChord.h"
#include <functional>

BEGIN(Editor)

enum class EShortcutContext : uint8
{
    Global,       // 에디터 전역 (텍스트 입력 중이 아닐 때)
    Viewport,     // 3D/2D 뷰포트 패널 활성 시
    GraphEditor,  // 노드/스크립트 그래프 에디터 활성 시
    Hierarchy,    // 씬 하이어라키 패널 활성 시
    Count
};

struct EditorAction
{
    std::string Id;               // 고유 식별자 (예: "Edit.Undo")
    std::string DisplayNameKey;   // 로캘 키 (예: "Action_Edit_Undo")
    std::string CategoryKey;      // 카테고리 로캘 키 (예: "Category_Edit")
    EShortcutContext Context = EShortcutContext::Global;
    ShortcutChord DefaultChord;   // 기본 단축키
    std::function<void()> ExecuteHandler;
    std::function<bool()> CanExecute; // 실행 가능 여부 (미지정 시 항상 true)
};

END