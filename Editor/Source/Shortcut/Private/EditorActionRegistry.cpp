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
#include "AnimationTimelinePanel.h"
#include "UnifiedSettingsPanel.h"
#include "PrefabEditorPanel.h"
#include "SpriteAnimationEditor.h"
#include "ContentBrowserPanel.h"
#include "PreviewViewportWidget.h"
#include "UIViewportPanel.h"

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
            const auto* animatorPanel = AnimatorGraphPanel::GetActivePanel();
            if (animatorPanel && animatorPanel->IsOpen() && animatorPanel->IsFocused())
                return false;

            const auto* timelinePanel = AnimationTimelinePanel::GetActivePanel();
            if (timelinePanel && timelinePanel->IsOpen() && timelinePanel->IsFocused())
                return false;

            auto& history = Application::Get().GetEditHistory();
            auto& selection = SelectionManager::Get();
            return history.ShortcutsAllowed() && PIEManager::Get().IsEditing() &&
                !selection.IsPrimarySelectionLocked() && !selection.GetSelectedEntities().empty();
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
            const auto* prefabPanel = dynamic_cast<PrefabEditorPanel*>(
                ImGuiManager::Get().GetImGuiPanel(L"Prefab Editor"));
            if (prefabPanel && prefabPanel->IsOpen() && prefabPanel->IsFocused())
                return false;

            const auto* spritePanel = dynamic_cast<SpriteAnimationEditor*>(
                ImGuiManager::Get().GetImGuiPanel(L"Sprite Animation Editor"));
            if (spritePanel && spritePanel->IsOpen() && spritePanel->IsFocused())
                return false;

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
    .Context = EShortcutContext::Global,
    .DefaultChord = ShortcutChord(Engine::EKeyCode::F),
    .ExecuteHandler = []() {
        if (auto* preview = PreviewViewportWidget::GetShortcutTarget())
        {
            preview->RefocusTarget();
            return;
        }

        auto* graph = GraphPanel::GetActiveGraphPanel();
        if (graph && graph->CanUseCanvas() && graph->IsFocused())
        {
            graph->ResetCanvasView();
            return;
        }

        auto* ui = dynamic_cast<UIViewportPanel*>(ImGuiManager::Get().GetImGuiPanel(L"UI View"));
        if (ui && ui->IsOpen() && ui->IsFocused())
        {
            ui->FocusCanvas();
            return;
        }

        auto* viewport = SceneViewportPanel::GetActiveSceneViewport();
        if (!viewport)
            viewport = dynamic_cast<SceneViewportPanel*>(ImGuiManager::Get().GetImGuiPanel(L"Scene View"));

        if (viewport)
            viewport->FocusSelectedEntity();
    },
    .CanExecute = []() {
        if (const auto* preview = PreviewViewportWidget::GetShortcutTarget())
            return preview->CanFocusTarget();

        const auto* graph = GraphPanel::GetActiveGraphPanel();
        if (graph && graph->CanUseCanvas() && graph->IsFocused())
            return true;

        const auto* ui = dynamic_cast<UIViewportPanel*>(ImGuiManager::Get().GetImGuiPanel(L"UI View"));
        if (ui && ui->IsOpen() && ui->IsFocused())
            return ui->CanFocusCanvas();

        auto& history = Application::Get().GetEditHistory();
        return history.ShortcutsAllowed() && !SelectionManager::Get().GetSelectedEntities().empty();
    }
        });

    sm.RegisterAction({
        .Id = "Hierarchy.Rename",
        .DisplayNameKey = "EditorCommon.Rename",
        .CategoryKey = "UI_HIERARCHY",
        .Context = EShortcutContext::Global, // 계층 및 뷰포트 어디서든 선택된 엔티티 F2 이름 변경 지원
        .DefaultChord = ShortcutChord(Engine::EKeyCode::F2),
        .ExecuteHandler = []() {
            if (auto* hierarchy = dynamic_cast<HierarchyPanel*>(ImGuiManager::Get().GetImGuiPanel(L"Hierarchy Panel")))
                hierarchy->TriggerSelectedEntityRename();
        },
        .CanExecute = []() {
                const auto* browser = dynamic_cast<ContentBrowserPanel*>(ImGuiManager::Get().GetImGuiPanel(L"Content Browser"));
                if (browser && browser->IsOpen() && browser->IsFocused())
                    return false;

                auto& sel = SelectionManager::Get();
                return PIEManager::Get().IsEditing() && !sel.IsPrimarySelectionLocked() && !sel.GetSelectedEntities().empty();
            }
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
        .Id = "Graph.AddNode",
        .DisplayNameKey = "GraphPanel.AddNode",
        .CategoryKey = "Shortcuts.CategoryGraph",
        .Context = EShortcutContext::GraphEditor,
        .DefaultChord = ShortcutChord(Engine::EKeyCode::Space),
        .ExecuteHandler = []() {
            if (auto* panel = GraphPanel::GetActiveGraphPanel()) panel->OpenAddNodeMenu();
        },
        .CanExecute = []() {
            const auto* panel = GraphPanel::GetActiveGraphPanel();
            return panel && panel->CanUseCanvas() && panel->IsFocused();
        }
    });
    sm.RegisterAction({
        .Id = "Graph.Validate",
        .DisplayNameKey = "GraphPanel.Validate",
        .CategoryKey = "Shortcuts.CategoryGraph",
        .Context = EShortcutContext::GraphEditor,
        .DefaultChord = ShortcutChord(Engine::EKeyCode::F7),
        .ExecuteHandler = []() {
            if (auto* panel = GraphPanel::GetActiveGraphPanel()) panel->ValidateGraph();
        },
        .CanExecute = []() {
            const auto* panel = GraphPanel::GetActiveGraphPanel();
            return panel && panel->CanUseCanvas() && panel->IsFocused();
        }
    });
    sm.RegisterAction({
        .Id = "Graph.AlignTop",
        .DisplayNameKey = "Action.Graph.AlignTop",
        .CategoryKey = "Shortcuts.CategoryGraph",
        .Context = EShortcutContext::GraphEditor,
        .DefaultChord = ShortcutChord(Engine::EKeyCode::W, EModifierKey::Shift),
        .ExecuteHandler = []() {
            if (auto* gp = GraphPanel::GetActiveGraphPanel()) gp->AlignNodes('W');
        },
        .CanExecute = []() {
            const auto* panel = GraphPanel::GetActiveGraphPanel();
            return panel && panel->CanUseCanvas();
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
        },
        .CanExecute = []() {
            const auto* panel = GraphPanel::GetActiveGraphPanel();
            return panel && panel->CanUseCanvas();
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
        },
        .CanExecute = []() {
            const auto* panel = GraphPanel::GetActiveGraphPanel();
            return panel && panel->CanUseCanvas();
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
        },
        .CanExecute = []() {
            const auto* panel = GraphPanel::GetActiveGraphPanel();
            return panel && panel->CanUseCanvas();
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
        },
        .CanExecute = []() {
            const auto* panel = GraphPanel::GetActiveGraphPanel();
            return panel && panel->CanUseCanvas();
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
        },
        .CanExecute = []() {
            const auto* panel = GraphPanel::GetActiveGraphPanel();
            return panel && panel->CanUseCanvas();
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
        },
        .CanExecute = []() {
            const auto* panel = GraphPanel::GetActiveGraphPanel();
            return panel && panel->CanUseCanvas();
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
        },
        .CanExecute = []() {
            const auto* panel = GraphPanel::GetActiveGraphPanel();
            return panel && panel->CanUseCanvas();
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
        },
        .CanExecute = []() {
            const auto* panel = GraphPanel::GetActiveGraphPanel();
            return panel && panel->CanUseCanvas();
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

    sm.RegisterAction({
    .Id = "Animator.DeleteSelection",
    .DisplayNameKey = "EditorCommon.Delete",
    .CategoryKey = "AnimatorGraph.Title",
    .Context = EShortcutContext::AnimatorGraph,
    .DefaultChord = ShortcutChord(Engine::EKeyCode::Delete),
    .ExecuteHandler = []() {
        if (auto* panel = AnimatorGraphPanel::GetActivePanel())
            panel->DeleteSelection();
    },
    .CanExecute = []() {
        const auto* panel = AnimatorGraphPanel::GetActivePanel();
        return panel && panel->CanDeleteSelection();
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

    sm.RegisterAction({
    .Id = "Viewport.ToggleRealtime",
    .DisplayNameKey = "Action.Viewport.ToggleRealtime",
    .CategoryKey = "Shortcuts.CategoryViewport",
    .Context = EShortcutContext::Viewport,
    .DefaultChord = ShortcutChord(Engine::EKeyCode::R, EModifierKey::Ctrl),
    .ExecuteHandler = []() {
        if (auto* vp = SceneViewportPanel::GetActiveSceneViewport())
            vp->ToggleRealtime();
    }
        });

    sm.RegisterAction({
            .Id = "Graph.Run",
            .DisplayNameKey = "Action.Graph.Run",
            .CategoryKey = "Shortcuts.CategoryGraph",
            .Context = EShortcutContext::GraphEditor,
            .DefaultChord = ShortcutChord(Engine::EKeyCode::F5, EModifierKey::Ctrl),
            .ExecuteHandler = []() {
                if (auto* panel = GraphPanel::GetActiveGraphPanel())
                    panel->RunExecution();
            },
            .CanExecute = []() {
                const auto* panel = GraphPanel::GetActiveGraphPanel();
                return panel && panel->CanRunExecution();
            }
        });

    sm.RegisterAction({
    .Id = "Graph.Pause",
    .DisplayNameKey = "Action.Graph.Pause",
    .CategoryKey = "Shortcuts.CategoryGraph",
    .Context = EShortcutContext::GraphEditor,
    .DefaultChord = ShortcutChord(Engine::EKeyCode::F6, EModifierKey::Ctrl),
    .ExecuteHandler = []() {
        if (auto* panel = GraphPanel::GetActiveGraphPanel())
            panel->PauseExecution();
    },
    .CanExecute = []() {
        const auto* panel = GraphPanel::GetActiveGraphPanel();
        return panel && panel->CanPauseExecution();
    }
        });

    sm.RegisterAction({
    .Id = "Graph.Step",
    .DisplayNameKey = "Action.Graph.Step",
    .CategoryKey = "Shortcuts.CategoryGraph",
    .Context = EShortcutContext::GraphEditor,
    .DefaultChord = ShortcutChord(Engine::EKeyCode::F10),
    .ExecuteHandler = []() {
        if (auto* panel = GraphPanel::GetActiveGraphPanel())
            panel->StepExecution();
    },
    .CanExecute = []() {
        const auto* panel = GraphPanel::GetActiveGraphPanel();
        return panel && panel->CanStepExecution();
    }
        });

    sm.RegisterAction({
    .Id = "Graph.Stop",
    .DisplayNameKey = "Action.Graph.Stop",
    .CategoryKey = "Shortcuts.CategoryGraph",
    .Context = EShortcutContext::GraphEditor,
    .DefaultChord = ShortcutChord(Engine::EKeyCode::F5, EModifierKey::Ctrl | EModifierKey::Shift),
    .ExecuteHandler = []() {
        if (auto* panel = GraphPanel::GetActiveGraphPanel())
            panel->StopExecution();
    },
    .CanExecute = []() {
        const auto* panel = GraphPanel::GetActiveGraphPanel();
        return panel && panel->CanStopExecution();
    }
        });

    // =========================================================================================
    // [Animation Timeline Actions] (타임라인 패널 활성 시 동작)
    // =========================================================================================
    sm.RegisterAction({
        .Id = "Timeline.PlayPause",
        .DisplayNameKey = "Action.Timeline.PlayPause",
        .CategoryKey = "Shortcuts.CategoryTimeline",
        .Context = EShortcutContext::AnimationTimeline,
        .DefaultChord = ShortcutChord(Engine::EKeyCode::Space),
        .ExecuteHandler = []() {
            if (auto* panel = AnimationTimelinePanel::GetActivePanel()) panel->TogglePlayPause();
        }
        });
    sm.RegisterAction({
        .Id = "Timeline.PrevFrame",
        .DisplayNameKey = "Action.Timeline.PrevFrame",
        .CategoryKey = "Shortcuts.CategoryTimeline",
        .Context = EShortcutContext::AnimationTimeline,
        .DefaultChord = ShortcutChord(Engine::EKeyCode::Left),
        .ExecuteHandler = []() {
            if (auto* panel = AnimationTimelinePanel::GetActivePanel()) panel->StepFrame(-1);
        },
        .AllowRepeat = true
        });
    sm.RegisterAction({
        .Id = "Timeline.NextFrame",
        .DisplayNameKey = "Action.Timeline.NextFrame",
        .CategoryKey = "Shortcuts.CategoryTimeline",
        .Context = EShortcutContext::AnimationTimeline,
        .DefaultChord = ShortcutChord(Engine::EKeyCode::Right),
        .ExecuteHandler = []() {
            if (auto* panel = AnimationTimelinePanel::GetActivePanel()) panel->StepFrame(1);
        },
        .AllowRepeat = true
        });
    sm.RegisterAction({
        .Id = "Timeline.FirstFrame",
        .DisplayNameKey = "Action.Timeline.FirstFrame",
        .CategoryKey = "Shortcuts.CategoryTimeline",
        .Context = EShortcutContext::AnimationTimeline,
        .DefaultChord = ShortcutChord(Engine::EKeyCode::Home),
        .ExecuteHandler = []() {
            if (auto* panel = AnimationTimelinePanel::GetActivePanel()) panel->GoToFirstFrame();
        }
        });
    sm.RegisterAction({
        .Id = "Timeline.LastFrame",
        .DisplayNameKey = "Action.Timeline.LastFrame",
        .CategoryKey = "Shortcuts.CategoryTimeline",
        .Context = EShortcutContext::AnimationTimeline,
        .DefaultChord = ShortcutChord(Engine::EKeyCode::End),
        .ExecuteHandler = []() {
            if (auto* panel = AnimationTimelinePanel::GetActivePanel()) panel->GoToLastFrame();
        }
        });
    sm.RegisterAction({
        .Id = "Timeline.ToggleLoop",
        .DisplayNameKey = "Action.Timeline.ToggleLoop",
        .CategoryKey = "Shortcuts.CategoryTimeline",
        .Context = EShortcutContext::AnimationTimeline,
        .DefaultChord = ShortcutChord(Engine::EKeyCode::L, EModifierKey::Ctrl),
        .ExecuteHandler = []() {
            if (auto* panel = AnimationTimelinePanel::GetActivePanel()) panel->ToggleLoop();
        }
        });
    sm.RegisterAction({
    .Id = "Timeline.ZoomReset",
    .DisplayNameKey = "Action.Timeline.ZoomReset",
    .CategoryKey = "Shortcuts.CategoryTimeline",
    .Context = EShortcutContext::AnimationTimeline,
    .DefaultChord = ShortcutChord(Engine::EKeyCode::F),
    .ExecuteHandler = []() {
        if (auto* panel = AnimationTimelinePanel::GetActivePanel())
            panel->ResetZoom();
    },
    .CanExecute = []() {
        const auto* panel = AnimationTimelinePanel::GetActivePanel();
        return panel && panel->IsOpen();
    }
        });
    sm.RegisterAction({
    .Id = "Timeline.DeleteSelected",
    .DisplayNameKey = "Action.Timeline.DeleteSelected",
    .CategoryKey = "Shortcuts.CategoryTimeline",
    .Context = EShortcutContext::AnimationTimeline,
    .DefaultChord = ShortcutChord(Engine::EKeyCode::Delete),
    .ExecuteHandler = []() {
        if (auto* panel = AnimationTimelinePanel::GetActivePanel())
            panel->DeleteSelected();
    },
    .CanExecute = []() {
        const auto* panel = AnimationTimelinePanel::GetActivePanel();
        return panel && panel->CanDeleteSelected();
    }
        });

    // ── [Edit / Editor Actions] ──
    sm.RegisterAction({
        .Id = "Editor.OpenSettings",
        .DisplayNameKey = "Action.Editor.OpenSettings",
        .CategoryKey = "UI_FILE",
        .Context = EShortcutContext::Global,
        .DefaultChord = ShortcutChord(Engine::EKeyCode::Comma, EModifierKey::Ctrl),
        .ExecuteHandler = []() {
            if (auto* panel = dynamic_cast<UnifiedSettingsPanel*>(ImGuiManager::Get().GetImGuiPanel(L"UnifiedSettings")))
                panel->OpenWithCategory(EUnifiedSettingsCategory::General);
        }
        });
    // ── [Animator Graph Actions] ──
    sm.RegisterAction({
        .Id = "Animator.SaveAs",
        .DisplayNameKey = "Action.Animator.SaveAs",
        .CategoryKey = "Shortcuts.CategoryGraph",
        .Context = EShortcutContext::AnimatorGraph,
        .DefaultChord = ShortcutChord(Engine::EKeyCode::S, EModifierKey::Ctrl | EModifierKey::Shift),
        .ExecuteHandler = []() {
            if (auto* panel = AnimatorGraphPanel::GetActivePanel())
                panel->SaveDocument(true);
        }
        });
    // ── [Visual Script Graph Actions] ──
    sm.RegisterAction({
        .Id = "Graph.Open",
        .DisplayNameKey = "Action.Graph.Open",
        .CategoryKey = "Shortcuts.CategoryGraph",
        .Context = EShortcutContext::GraphEditor,
        .DefaultChord = ShortcutChord(Engine::EKeyCode::O, EModifierKey::Ctrl),
        .ExecuteHandler = []() {
            if (auto* gp = GraphPanel::GetActiveGraphPanel())
                gp->OpenDocument();
        }
        });
    sm.RegisterAction({
        .Id = "Graph.SaveAs",
        .DisplayNameKey = "Action.Graph.SaveAs",
        .CategoryKey = "Shortcuts.CategoryGraph",
        .Context = EShortcutContext::GraphEditor,
        .DefaultChord = ShortcutChord(Engine::EKeyCode::S, EModifierKey::Ctrl | EModifierKey::Shift),
        .ExecuteHandler = []() {
            if (auto* gp = GraphPanel::GetActiveGraphPanel())
                gp->SaveDocument(true);
        }
        });

    // ── [Prefab Editor Actions] ──
    sm.RegisterAction({
            .Id = "Prefab.Save",
            .DisplayNameKey = "PrefabEditor.Save",
            .CategoryKey = "ResourceEditor.PrefabTitle",
            .Context = EShortcutContext::PrefabEditor,
            .DefaultChord = ShortcutChord(Engine::EKeyCode::S, EModifierKey::Ctrl),
            .ExecuteHandler = []() {
                auto* panel = dynamic_cast<PrefabEditorPanel*>(ImGuiManager::Get().GetImGuiPanel(L"Prefab Editor"));
                if (panel) panel->SaveDocument();
            },
            .CanExecute = []() {
                const auto* panel = dynamic_cast<PrefabEditorPanel*>(ImGuiManager::Get().GetImGuiPanel(L"Prefab Editor"));
                return panel && panel->CanSaveDocument();
            }
        });

    sm.RegisterAction({
    .Id = "SpriteAnimation.Save",
    .DisplayNameKey = "EditorCommon.Save",
    .CategoryKey = "ResourceEditor.SpriteAnimationTitle",
    .Context = EShortcutContext::SpriteAnimationEditor,
    .DefaultChord = ShortcutChord(Engine::EKeyCode::S, EModifierKey::Ctrl),
    .ExecuteHandler = []() {
        auto* panel = dynamic_cast<SpriteAnimationEditor*>(
            ImGuiManager::Get().GetImGuiPanel(L"Sprite Animation Editor"));
        if (panel) panel->SaveAnimation();
    },
    .CanExecute = []() {
        const auto* panel = dynamic_cast<SpriteAnimationEditor*>(
            ImGuiManager::Get().GetImGuiPanel(L"Sprite Animation Editor"));
        return panel && panel->CanSaveAnimation();
    }
        });

    sm.RegisterAction({
    .Id = "Edit.Rename",
    .DisplayNameKey = "EditorCommon.Rename",
    .CategoryKey = "UI_CONTENT_BROWSER",
    .Context = EShortcutContext::ContentBrowser,
    .DefaultChord = ShortcutChord(Engine::EKeyCode::F2),
    .ExecuteHandler = []() {
        auto* panel = dynamic_cast<ContentBrowserPanel*>(ImGuiManager::Get().GetImGuiPanel(L"Content Browser"));
        if (panel) panel->RenameTarget();
    },
    .CanExecute = []() {
        const auto* panel = dynamic_cast<ContentBrowserPanel*>(ImGuiManager::Get().GetImGuiPanel(L"Content Browser"));
        return panel && panel->CanRenameTarget();
    }
        });
}

END
