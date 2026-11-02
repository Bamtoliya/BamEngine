#include "EditorActionRegistry.h"
#include "ShortcutManager.h"
#include "Application.h"
#include "ToolBar.h"
#include "PIEManager.h"
#include "SceneDocument.h"
#include "EntityClipboard.h"
#include "SelectionManager.h"
#include "SceneManager.h"
#include "HierarchyPanel.h"
#include "SceneViewportPanel.h"
#include "GraphPanel.h"
#include "HotReloadManager.h"
#include "ModuleBuildManager.h"
#include "ModuleHotReloadManager.h"
#include "ShaderHotReloadManager.h"
#include "ImGuiManager.h"
#include <SDL3/SDL_events.h>
#include "AnimatorGraphPanel.h"

BEGIN(Editor)

void EditorActionRegistry::RegisterAll()
{
    auto& sm = ShortcutManager::Get();

    // =========================================================================================
    // [Edit Actions] (Global Context)
    // =========================================================================================
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

    sm.RegisterAction({
        .Id = "Edit.Duplicate",
        .DisplayNameKey = "Action.Edit.Duplicate",
        .CategoryKey = "UI_EDIT",
        .Context = EShortcutContext::Global,
        .DefaultChord = ShortcutChord(Engine::EKeyCode::D, EModifierKey::Ctrl),
        .ExecuteHandler = []() {
            if (auto* scene = SceneManager::Get().GetCurrentScene())
            {
                if (EntityClipboard::CopySelected(scene))
                    EntityClipboard::Paste(scene, nullptr);
            }
        },
        .CanExecute = []() {
            auto& h = Application::Get().GetEditHistory();
            if (!h.ShortcutsAllowed() || !PIEManager::Get().IsEditing()) return false;
            return !SelectionManager::Get().GetSelectedEntities().empty();
        }
    });

    sm.RegisterAction({
        .Id = "Edit.Delete",
        .DisplayNameKey = "Action.Edit.Delete",
        .CategoryKey = "UI_EDIT",
        .Context = EShortcutContext::Global,
        .DefaultChord = ShortcutChord(Engine::EKeyCode::Delete),
        .ExecuteHandler = []() {
            if (auto* scene = SceneManager::Get().GetCurrentScene())
                HierarchyPanel::DeleteSelectedEntities(scene);
        },
        .CanExecute = []() {
            auto& h = Application::Get().GetEditHistory();
            auto& sel = SelectionManager::Get();
            return h.ShortcutsAllowed() && PIEManager::Get().IsEditing() &&
                !sel.IsPrimarySelectionLocked() && !sel.GetSelectedEntities().empty();
        }
    });

    sm.RegisterAction({
        .Id = "Edit.Deselect",
        .DisplayNameKey = "Action.Edit.Deselect",
        .CategoryKey = "UI_EDIT",
        .Context = EShortcutContext::Global,
        .DefaultChord = ShortcutChord(Engine::EKeyCode::Escape),
        .ExecuteHandler = []() { SelectionManager::Get().ClearSelection(); },
        .CanExecute = []() { return !SelectionManager::Get().GetSelectedEntities().empty(); }
    });

    // =========================================================================================
    // [File / Scene Actions] (Global Context)
    // =========================================================================================
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
            return PIEManager::Get().IsEditing() && document && document->CanSave();
        }
    });

    sm.RegisterAction({
        .Id = "File.SaveSceneAs",
        .DisplayNameKey = "Action.File.SaveSceneAs",
        .CategoryKey = "UI_SCENE",
        .Context = EShortcutContext::Global,
        .DefaultChord = ShortcutChord(Engine::EKeyCode::S, EModifierKey::Ctrl | EModifierKey::Shift),
        .ExecuteHandler = []() { ToolBar::SaveScene(true); },
        .CanExecute = []() {
            const auto* document = Application::Get().GetSceneDocument();
            return PIEManager::Get().IsEditing() && document && document->CanSave();
        }
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

    sm.RegisterAction({
        .Id = "File.CloseScene",
        .DisplayNameKey = "Action.File.CloseScene",
        .CategoryKey = "UI_SCENE",
        .Context = EShortcutContext::Global,
        .DefaultChord = ShortcutChord(Engine::EKeyCode::W, EModifierKey::Ctrl),
        .ExecuteHandler = []() { ImGuiManager::Get().RequestCloseScene(); },
        .CanExecute = []() { return PIEManager::Get().IsEditing(); }
    });

    sm.RegisterAction({
        .Id = "File.ImportAsset",
        .DisplayNameKey = "Action.File.ImportAsset",
        .CategoryKey = "UI_FILE",
        .Context = EShortcutContext::Global,
        .DefaultChord = ShortcutChord(Engine::EKeyCode::I, EModifierKey::Ctrl),
        .ExecuteHandler = []() { ToolBar::ImportAsset(); }
    });

    sm.RegisterAction({
        .Id = "File.Exit",
        .DisplayNameKey = "Action.File.Exit",
        .CategoryKey = "UI_FILE",
        .Context = EShortcutContext::Global,
        .DefaultChord = ShortcutChord(Engine::EKeyCode::F4, EModifierKey::Alt),
        .ExecuteHandler = []() {
            SDL_Event quitEvent;
            quitEvent.type = SDL_EVENT_QUIT;
            SDL_PushEvent(&quitEvent);
        }
    });

    // =========================================================================================
    // [PIE & Build Actions] (Global Context)
    // =========================================================================================
    sm.RegisterAction({
        .Id = "PIE.Play",
        .DisplayNameKey = "Action.PIE.Play",
        .CategoryKey = "Shortcuts.CategoryPIE",
        .Context = EShortcutContext::Global,
        .DefaultChord = ShortcutChord(Engine::EKeyCode::F5),
        .ExecuteHandler = []() { PIEManager::Get().StartPlay(); },
        .CanExecute = []() { return PIEManager::Get().IsEditing(); }
    });

    sm.RegisterAction({
        .Id = "PIE.PauseResume",
        .DisplayNameKey = "Action.PIE.PauseResume",
        .CategoryKey = "Shortcuts.CategoryPIE",
        .Context = EShortcutContext::Global,
        .DefaultChord = ShortcutChord(Engine::EKeyCode::F6),
        .ExecuteHandler = []() {
            auto& pie = PIEManager::Get();
            if (pie.GetState() == EPlayState::Play) pie.Pause();
            else if (pie.GetState() == EPlayState::Pause) pie.Resume();
        },
        .CanExecute = []() { return PIEManager::Get().GetState() != EPlayState::Edit; }
    });

    sm.RegisterAction({
        .Id = "PIE.Stop",
        .DisplayNameKey = "Action.PIE.Stop",
        .CategoryKey = "Shortcuts.CategoryPIE",
        .Context = EShortcutContext::Global,
        .DefaultChord = ShortcutChord(Engine::EKeyCode::F5, EModifierKey::Shift),
        .ExecuteHandler = []() { PIEManager::Get().Stop(); },
        .CanExecute = []() { return PIEManager::Get().GetState() != EPlayState::Edit; }
    });

    sm.RegisterAction({
        .Id = "Build.CompileModule",
        .DisplayNameKey = "Action.Build.CompileModule",
        .CategoryKey = "Shortcuts.CategoryBuild",
        .Context = EShortcutContext::Global,
        .DefaultChord = ShortcutChord(Engine::EKeyCode::B, EModifierKey::Ctrl | EModifierKey::Shift),
        .ExecuteHandler = []() { HotReloadManager::Get().GetModuleBuilder().RequestBuild(); },
        .CanExecute = []() { return !HotReloadManager::Get().GetModuleBuilder().IsCompiling(); }
    });

    sm.RegisterAction({
        .Id = "Build.ReloadModule",
        .DisplayNameKey = "Action.Build.ReloadModule",
        .CategoryKey = "Shortcuts.CategoryBuild",
        .Context = EShortcutContext::Global,
        .DefaultChord = ShortcutChord(Engine::EKeyCode::R, EModifierKey::Ctrl | EModifierKey::Shift),
        .ExecuteHandler = []() { HotReloadManager::Get().GetModuleReloader().ReloadModule(); }
    });

    sm.RegisterAction({
        .Id = "Build.ReloadShaders",
        .DisplayNameKey = "Action.Build.ReloadShaders",
        .CategoryKey = "Shortcuts.CategoryBuild",
        .Context = EShortcutContext::Global,
        .DefaultChord = ShortcutChord(Engine::EKeyCode::R, EModifierKey::Ctrl | EModifierKey::Alt),
        .ExecuteHandler = []() { HotReloadManager::Get().GetShaderReloader().RequestRecompileAll(); }
    });

    // =========================================================================================
    // [Viewport Actions] (Viewport Context)
    // =========================================================================================
    sm.RegisterAction({
        .Id = "Viewport.GizmoSelect",
        .DisplayNameKey = "Action.Viewport.GizmoSelect",
        .CategoryKey = "Shortcuts.CategoryViewport",
        .Context = EShortcutContext::Viewport,
        .DefaultChord = ShortcutChord(Engine::EKeyCode::Q),
        .ExecuteHandler = []() {
            if (auto* vp = SceneViewportPanel::GetActiveSceneViewport())
                vp->SetGizmoOperation(static_cast<ImGuizmo::OPERATION>(0));
        }
    });

    sm.RegisterAction({
        .Id = "Viewport.GizmoTranslate",
        .DisplayNameKey = "Action.Viewport.GizmoTranslate",
        .CategoryKey = "Shortcuts.CategoryViewport",
        .Context = EShortcutContext::Viewport,
        .DefaultChord = ShortcutChord(Engine::EKeyCode::W),
        .ExecuteHandler = []() {
            if (auto* vp = SceneViewportPanel::GetActiveSceneViewport())
                vp->SetGizmoOperation(ImGuizmo::TRANSLATE);
        }
    });

    sm.RegisterAction({
        .Id = "Viewport.GizmoRotate",
        .DisplayNameKey = "Action.Viewport.GizmoRotate",
        .CategoryKey = "Shortcuts.CategoryViewport",
        .Context = EShortcutContext::Viewport,
        .DefaultChord = ShortcutChord(Engine::EKeyCode::E),
        .ExecuteHandler = []() {
            if (auto* vp = SceneViewportPanel::GetActiveSceneViewport())
                vp->SetGizmoOperation(ImGuizmo::ROTATE);
        }
    });

    sm.RegisterAction({
        .Id = "Viewport.GizmoScale",
        .DisplayNameKey = "Action.Viewport.GizmoScale",
        .CategoryKey = "Shortcuts.CategoryViewport",
        .Context = EShortcutContext::Viewport,
        .DefaultChord = ShortcutChord(Engine::EKeyCode::R),
        .ExecuteHandler = []() {
            if (auto* vp = SceneViewportPanel::GetActiveSceneViewport())
                vp->SetGizmoOperation(ImGuizmo::SCALE);
        }
    });

    sm.RegisterAction({
        .Id = "Viewport.ToggleCoord",
        .DisplayNameKey = "Action.Viewport.ToggleCoord",
        .CategoryKey = "Shortcuts.CategoryViewport",
        .Context = EShortcutContext::Viewport,
        .DefaultChord = ShortcutChord(Engine::EKeyCode::GraveAccent),
        .ExecuteHandler = []() {
            if (auto* vp = SceneViewportPanel::GetActiveSceneViewport())
                vp->ToggleGizmoMode();
        }
    });

    sm.RegisterAction({
        .Id = "Viewport.ToggleSnap",
        .DisplayNameKey = "Action.Viewport.ToggleSnap",
        .CategoryKey = "Shortcuts.CategoryViewport",
        .Context = EShortcutContext::Viewport,
        .DefaultChord = ShortcutChord(Engine::EKeyCode::S, EModifierKey::Shift),
        .ExecuteHandler = []() {
            if (auto* vp = SceneViewportPanel::GetActiveSceneViewport())
                vp->ToggleSnap();
        }
    });

    sm.RegisterAction({
        .Id = "Viewport.Focus",
        .DisplayNameKey = "Action.Viewport.Focus",
        .CategoryKey = "Shortcuts.CategoryViewport",
        .Context = EShortcutContext::Viewport,
        .DefaultChord = ShortcutChord(Engine::EKeyCode::F),
        .ExecuteHandler = []() {
            if (auto* vp = SceneViewportPanel::GetActiveSceneViewport())
                vp->FocusSelectedEntity();
        },
        .CanExecute = []() { return !SelectionManager::Get().GetSelectedEntities().empty(); }
    });

    sm.RegisterAction({
        .Id = "Viewport.ToggleLock",
        .DisplayNameKey = "Action.Viewport.ToggleLock",
        .CategoryKey = "Shortcuts.CategoryViewport",
        .Context = EShortcutContext::Viewport,
        .DefaultChord = ShortcutChord(Engine::EKeyCode::L, EModifierKey::Shift),
        .ExecuteHandler = []() { SelectionManager::Get().TogglePrimarySelectionLock(); }
    });

    sm.RegisterAction({
        .Id = "Viewport.ToggleGrid",
        .DisplayNameKey = "Action.Viewport.ToggleGrid",
        .CategoryKey = "Shortcuts.CategoryViewport",
        .Context = EShortcutContext::Viewport,
        .DefaultChord = ShortcutChord(Engine::EKeyCode::G),
        .ExecuteHandler = []() {
            if (auto* vp = SceneViewportPanel::GetActiveSceneViewport())
                vp->ToggleGrid();
        }
    });

    // =========================================================================================
    // [Graph Editor Actions] (GraphEditor Context)
    // =========================================================================================
    sm.RegisterAction({
        .Id = "Graph.AlignTop",
        .DisplayNameKey = "Action.Graph.AlignTop",
        .CategoryKey = "Shortcuts.CategoryGraph",
        .Context = EShortcutContext::GraphEditor,
        .DefaultChord = ShortcutChord(Engine::EKeyCode::W, EModifierKey::Shift),
        .ExecuteHandler = []() {
            if (auto* gp = GraphPanel::GetActiveGraphPanel()) gp->AlignNodes('W');
        }
    });

    sm.RegisterAction({
        .Id = "Graph.AlignBottom",
        .DisplayNameKey = "Action.Graph.AlignBottom",
        .CategoryKey = "Shortcuts.CategoryGraph",
        .Context = EShortcutContext::GraphEditor,
        .DefaultChord = ShortcutChord(Engine::EKeyCode::S, EModifierKey::Shift),
        .ExecuteHandler = []() {
            if (auto* gp = GraphPanel::GetActiveGraphPanel()) gp->AlignNodes('S');
        }
    });

    sm.RegisterAction({
        .Id = "Graph.AlignLeft",
        .DisplayNameKey = "Action.Graph.AlignLeft",
        .CategoryKey = "Shortcuts.CategoryGraph",
        .Context = EShortcutContext::GraphEditor,
        .DefaultChord = ShortcutChord(Engine::EKeyCode::A, EModifierKey::Shift),
        .ExecuteHandler = []() {
            if (auto* gp = GraphPanel::GetActiveGraphPanel()) gp->AlignNodes('A');
        }
    });

    sm.RegisterAction({
        .Id = "Graph.AlignRight",
        .DisplayNameKey = "Action.Graph.AlignRight",
        .CategoryKey = "Shortcuts.CategoryGraph",
        .Context = EShortcutContext::GraphEditor,
        .DefaultChord = ShortcutChord(Engine::EKeyCode::D, EModifierKey::Shift),
        .ExecuteHandler = []() {
            if (auto* gp = GraphPanel::GetActiveGraphPanel()) gp->AlignNodes('D');
        }
    });

    sm.RegisterAction({
        .Id = "Graph.DistributeHoriz",
        .DisplayNameKey = "Action.Graph.DistributeHoriz",
        .CategoryKey = "Shortcuts.CategoryGraph",
        .Context = EShortcutContext::GraphEditor,
        .DefaultChord = ShortcutChord(Engine::EKeyCode::H, EModifierKey::Shift),
        .ExecuteHandler = []() {
            if (auto* gp = GraphPanel::GetActiveGraphPanel()) gp->AlignNodes('H');
        }
    });

    sm.RegisterAction({
        .Id = "Graph.DistributeVert",
        .DisplayNameKey = "Action.Graph.DistributeVert",
        .CategoryKey = "Shortcuts.CategoryGraph",
        .Context = EShortcutContext::GraphEditor,
        .DefaultChord = ShortcutChord(Engine::EKeyCode::V, EModifierKey::Shift),
        .ExecuteHandler = []() {
            if (auto* gp = GraphPanel::GetActiveGraphPanel()) gp->AlignNodes('V');
        }
    });

    sm.RegisterAction({
        .Id = "Graph.Duplicate",
        .DisplayNameKey = "Action.Graph.Duplicate",
        .CategoryKey = "Shortcuts.CategoryGraph",
        .Context = EShortcutContext::GraphEditor,
        .DefaultChord = ShortcutChord(Engine::EKeyCode::D, EModifierKey::Ctrl),
        .ExecuteHandler = []() {
            if (auto* gp = GraphPanel::GetActiveGraphPanel()) gp->DuplicateSelectedNodes();
        }
    });

    sm.RegisterAction({
        .Id = "Graph.CreateComment",
        .DisplayNameKey = "Action.Graph.CreateComment",
        .CategoryKey = "Shortcuts.CategoryGraph",
        .Context = EShortcutContext::GraphEditor,
        .DefaultChord = ShortcutChord(Engine::EKeyCode::C),
        .ExecuteHandler = []() {
            if (auto* gp = GraphPanel::GetActiveGraphPanel()) gp->CreateCommentBox();
        }
    });

    sm.RegisterAction({
        .Id = "Graph.Delete",
        .DisplayNameKey = "Action.Graph.Delete",
        .CategoryKey = "Shortcuts.CategoryGraph",
        .Context = EShortcutContext::GraphEditor,
        .DefaultChord = ShortcutChord(Engine::EKeyCode::Delete),
        .ExecuteHandler = []() {
            if (auto* gp = GraphPanel::GetActiveGraphPanel()) gp->DeleteSelectedElements();
        }
    });

    sm.RegisterAction({
        .Id = "Graph.ToggleBreakpoint",
        .DisplayNameKey = "Action.Graph.ToggleBreakpoint",
        .CategoryKey = "Shortcuts.CategoryGraph",
        .Context = EShortcutContext::GraphEditor,
        .DefaultChord = ShortcutChord(Engine::EKeyCode::F9),
        .ExecuteHandler = []() {
            if (auto* gp = GraphPanel::GetActiveGraphPanel()) gp->ToggleBreakpointOnSelected();
        }
    });

    // [Animator Graph Actions] (포커스 시 우선 실행)
    sm.RegisterAction({
        .Id = "Animator.Save",
        .DisplayNameKey = "EditorCommon.Save",
        .CategoryKey = "Shortcuts.CategoryGraph",
        .Context = EShortcutContext::AnimatorGraph,
        .DefaultChord = ShortcutChord(Engine::EKeyCode::S, EModifierKey::Ctrl),
        .ExecuteHandler = []() {
            if (auto* panel = AnimatorGraphPanel::GetActivePanel())
                panel->SaveDocument(false);
        }
        });

    // [Graph Editor Actions] (포커스 시 우선 실행)
    sm.RegisterAction({
        .Id = "Graph.Save",
        .DisplayNameKey = "EditorCommon.Save",
        .CategoryKey = "Shortcuts.CategoryGraph",
        .Context = EShortcutContext::GraphEditor,
        .DefaultChord = ShortcutChord(Engine::EKeyCode::S, EModifierKey::Ctrl),
        .ExecuteHandler = []() {
            if (auto* gp = GraphPanel::GetActiveGraphPanel())
                gp->SaveDocument(false);
        }
        });

    sm.RegisterAction({
    .Id = "Animator.New",
    .DisplayNameKey = "EditorCommon.New",
    .CategoryKey = "Shortcuts.CategoryGraph",
    .Context = EShortcutContext::AnimatorGraph,
    .DefaultChord = ShortcutChord(Engine::EKeyCode::N, EModifierKey::Ctrl),
    .ExecuteHandler = []() {
        if (auto* panel = AnimatorGraphPanel::GetActivePanel())
            panel->RequestNew();
    }
        });

    sm.RegisterAction({
        .Id = "Animator.Open",
        .DisplayNameKey = "EditorCommon.Open",
        .CategoryKey = "Shortcuts.CategoryGraph",
        .Context = EShortcutContext::AnimatorGraph,
        .DefaultChord = ShortcutChord(Engine::EKeyCode::O, EModifierKey::Ctrl),
        .ExecuteHandler = []() {
            if (auto* panel = AnimatorGraphPanel::GetActivePanel())
                panel->RequestOpen();
        }
        });
}

END
