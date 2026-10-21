#include "EditorActionRegistry.h"
#include "ShortcutManager.h"
#include "Application.h"
#include "ToolBar.h"
#include "PIEManager.h"

BEGIN(Editor)

void EditorActionRegistry::RegisterAll()
{
    auto& sm = ShortcutManager::Get();

    // [Edit Actions] --------------------------------------------------------------------------
    sm.RegisterAction({
        .Id = "Edit.Undo",
        .DisplayNameKey = "UI_UNDO",
        .CategoryKey = "UI_EDIT",
        .Context = EShortcutContext::Global,
        .DefaultChord = ShortcutChord(Engine::EKeyCode::Z, EModifierKey::Ctrl),
        .ExecuteHandler = []() { Application::Get().GetEditHistory().Undo(); },
        .CanExecute = []() {
            auto& h = Application::Get().GetEditHistory();
            return h.ShortcutsAllowed() && h.CanUndo();
        }
        });

    sm.RegisterAction({
        .Id = "Edit.Redo",
        .DisplayNameKey = "UI_REDO",
        .CategoryKey = "UI_EDIT",
        .Context = EShortcutContext::Global,
        .DefaultChord = ShortcutChord(Engine::EKeyCode::Y, EModifierKey::Ctrl),
        .ExecuteHandler = []() { Application::Get().GetEditHistory().Redo(); },
        .CanExecute = []() {
            auto& h = Application::Get().GetEditHistory();
            return h.ShortcutsAllowed() && h.CanRedo();
        }
        });

    // [File / Scene Actions] ------------------------------------------------------------------
    sm.RegisterAction({
        .Id = "File.NewScene",
        .DisplayNameKey = "UI_NEW_SCENE",
        .CategoryKey = "UI_SCENE",
        .Context = EShortcutContext::Global,
        .DefaultChord = ShortcutChord(Engine::EKeyCode::N, EModifierKey::Ctrl),
        .ExecuteHandler = []() { ToolBar::NewScene(); },
        .CanExecute = []() { return PIEManager::Get().CanEdit(); }
        });

    sm.RegisterAction({
        .Id = "File.SaveScene",
        .DisplayNameKey = "UI_SAVE_SCENE",
        .CategoryKey = "UI_SCENE",
        .Context = EShortcutContext::Global,
        .DefaultChord = ShortcutChord(Engine::EKeyCode::S, EModifierKey::Ctrl),
        .ExecuteHandler = []() { ToolBar::SaveScene(); },
        .CanExecute = []() { return PIEManager::Get().CanEdit(); }
        });

    sm.RegisterAction({
        .Id = "File.LoadScene",
        .DisplayNameKey = "UI_LOAD_SCENE",
        .CategoryKey = "UI_SCENE",
        .Context = EShortcutContext::Global,
        .DefaultChord = ShortcutChord(Engine::EKeyCode::O, EModifierKey::Ctrl),
        .ExecuteHandler = []() { ToolBar::LoadScene(); },
        .CanExecute = []() { return PIEManager::Get().CanEdit(); }
        });

    // 사용자 커스텀 오버라이드 단축키 불러오기
    sm.LoadFromFile();
}

END