#include "EditorActionRegistry.h"
#include "ShortcutManager.h"
#include "Application.h"
#include "ToolBar.h"
#include "PIEManager.h"
#include "SceneDocument.h"
#include "EntityClipboard.h"
#include "SelectionManager.h"
#include "SceneManager.h"

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
        },
        .AllowRepeat = true
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
        },
        .AllowRepeat = true
        });

    sm.RegisterAction({
    .Id = "Edit.Copy",
    .DisplayNameKey = "EditorCommon.Copy",
    .CategoryKey = "UI_EDIT",
    .Context = EShortcutContext::Global,
    .DefaultChord = ShortcutChord(Engine::EKeyCode::C, EModifierKey::Ctrl),
    .ExecuteHandler = []() {
        if (auto* scene = SceneManager::Get().GetCurrentScene())
            EntityClipboard::CopySelected(scene);
    },
    .CanExecute = []() {
        auto& h = Application::Get().GetEditHistory();
        if (!h.ShortcutsAllowed() || !PIEManager::Get().IsEditing()) return false;
        return !SelectionManager::Get().GetSelectedEntities().empty();
    }
        });

    sm.RegisterAction({
        .Id = "Edit.Paste",
        .DisplayNameKey = "EditorCommon.Paste",
        .CategoryKey = "UI_EDIT",
        .Context = EShortcutContext::Global,
        .DefaultChord = ShortcutChord(Engine::EKeyCode::V, EModifierKey::Ctrl),
        .ExecuteHandler = []() {
            if (auto* scene = SceneManager::Get().GetCurrentScene())
                EntityClipboard::Paste(scene, nullptr);
        },
        .CanExecute = []() {
            auto& h = Application::Get().GetEditHistory();
            return h.ShortcutsAllowed() && PIEManager::Get().IsEditing() && EntityClipboard::HasData();
        }
        });

    sm.RegisterAction({
        .Id = "Edit.PasteAsChild",
        .DisplayNameKey = "EditorCommon.PasteAsChild",
        .CategoryKey = "UI_EDIT",
        .Context = EShortcutContext::Global,
        .DefaultChord = ShortcutChord(Engine::EKeyCode::V, EModifierKey::Ctrl | EModifierKey::Shift),
        .ExecuteHandler = []() {
            if (auto* scene = SceneManager::Get().GetCurrentScene())
            {
                auto* parent = SelectionManager::Get().GetPrimarySelectedEntity();
                EntityClipboard::Paste(scene, parent);
            }
        },
        .CanExecute = []() {
            auto& h = Application::Get().GetEditHistory();
            return h.ShortcutsAllowed() && PIEManager::Get().IsEditing() && EntityClipboard::HasData();
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
        .CanExecute = []() { return PIEManager::Get().IsEditing(); }
        });

    sm.RegisterAction({
        .Id = "File.SaveScene",
        .DisplayNameKey = "UI_SAVE_SCENE",
        .CategoryKey = "UI_SCENE",
        .Context = EShortcutContext::Global,
        .DefaultChord = ShortcutChord(Engine::EKeyCode::S, EModifierKey::Ctrl),
        .ExecuteHandler = []() { ToolBar::SaveScene(); },
        .CanExecute = []() {
            const auto* document = Application::Get().GetSceneDocument();
            return PIEManager::Get().IsEditing() && document && document->CanSave(); }
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
}

END