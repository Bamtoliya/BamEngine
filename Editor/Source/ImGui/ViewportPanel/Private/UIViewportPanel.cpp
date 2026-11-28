#include "UIViewportPanel.h"
#include "ComponentEnable.h"
#include "InputManager.h"
#include "ShortcutManager.h"
#include "Application.h"
#include "SceneManager.h"
#include "WorldComponents.h"
#include "SelectionManager.h"
#include "EntityFactory.h"
#include "EditorUI.h"
#include "ImGuiManager.h"
#include "RenderTargetManager.h"
#include "RenderPassManager.h"
#include "Renderer.h"
#include "PrefabModeManager.h"
#include "UIClipLayout.h"

namespace
{
    const glm::vec2 CanvasCorners[] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };

    Engine::Scene* GetPrefabEditScene()
    {
        if (!Editor::PrefabModeManager::IsCreated()) return nullptr;
        const auto& prefab = Editor::PrefabModeManager::Get();
        return prefab.IsInPrefabMode() ? prefab.GetPrefabScene() : nullptr;
    }

    bool IsPrefabPreviewCanvas(const Engine::Scene* scene, entt::entity canvas)
    {
        if (!scene || scene != GetPrefabEditScene()) return false;
        const auto* root = Editor::PrefabModeManager::Get().GetRootEntity();
        return root && root->GetEntityHandle() != canvas;
    }

    ImVec2 ImPoint(const glm::vec2& point) { return ImVec2(point.x, point.y); }
    glm::vec2 Point(const ImVec2& point) { return glm::vec2(point.x, point.y); }
    bool SameRect(const Engine::RectTransformComponent& left, const Engine::RectTransformComponent& right)
    {
        return left.anchorMin == right.anchorMin && left.anchorMax == right.anchorMax &&
            left.anchoredPosition == right.anchoredPosition && left.sizeDelta == right.sizeDelta &&
            left.pivot == right.pivot && left.scale == right.scale && left.rotation == right.rotation;
    }
    const char* UIPointerEventKey(Engine::EUIPointerEventType type)
    {
        switch (type)
        {
        case Engine::EUIPointerEventType::Enter: return "Viewport.PointerEnter";
        case Engine::EUIPointerEventType::Exit: return "Viewport.PointerExit";
        case Engine::EUIPointerEventType::Down: return "Viewport.PointerDown";
        case Engine::EUIPointerEventType::Up: return "Viewport.PointerUp";
        case Engine::EUIPointerEventType::Click: return "Viewport.PointerClick";
        case Engine::EUIPointerEventType::ValueChanged: return "Viewport.PointerValueChanged";
        default: return "Viewport.PointerCancel";
        }
    }
}

EResult Editor::UIViewportPanel::OnInitialize(void* arg)
{
    const EResult result = BaseViewportPanel::OnInitialize(arg);
    if (result != EResult::Success)
        return result;

    m_CanvasRTName = m_Name + L"_VirtualCanvas";
    RenderTargetDesc desc{};
    desc.name = m_CanvasRTName;
    desc.textureDesc.width = static_cast<uint32>(m_CanvasSize.x);
    desc.textureDesc.height = static_cast<uint32>(m_CanvasSize.y);
    desc.textureDesc.usage = ETextureUsage::RenderTarget | ETextureUsage::Sampler;
    desc.textureDesc.clearColor = vec4(0.14f, 0.15f, 0.17f, 1.0f);
    RenderTargetManager::Get().CreateRenderTarget(&desc);
    m_UIOverlayPassID = RenderPassManager::Get().RegisterRenderPass(m_Name + L"_UIOverlayPass", { m_CanvasRTName }, L"",
        ERenderPassLoadOperation::RPLO_Clear, ERenderPassStoreOperation::RPSO_Store,
        ERenderPassLoadOperation::RPLO_Clear, ERenderPassStoreOperation::RPSO_Store,
        vec4(0.0f, 0.0f, 0.0f, -1.0f), 200, ERenderSortType::None, ERenderPassType::UI,
        EBlendMode::Opaque | EBlendMode::Masked | EBlendMode::AlphaBlend | EBlendMode::NonPremultiplied);
    m_SelectedRTName = m_CanvasRTName;
    return EResult::Success;
}

void Editor::UIViewportPanel::Free()
{
    SetTarget(nullptr, entt::null);
    Renderer::Get().ClearRenderQueue(m_UIOverlayPassID);
    Renderer::Get().UnregisterViewportCamera(m_UIOverlayPassID);
    BaseViewportPanel::Free();
}

bool Editor::UIViewportPanel::HasTarget() const
{
    if (!m_TargetScene) return false;
    if (const auto* prefabScene = GetPrefabEditScene(); prefabScene && prefabScene != m_TargetScene)
        return false;
    const auto active = SceneManager::Get().GetActiveScenes();
    if (std::find(active.begin(), active.end(), m_TargetScene) == active.end()) return false;
    const auto& world = m_TargetScene->GetRegistry();
    const auto* id = world.valid(m_TargetCanvas) ? world.try_get<IDComponent>(m_TargetCanvas) : nullptr;
    return id && id->id == m_TargetCanvasId && world.all_of<CanvasComponent>(m_TargetCanvas);
}

void Editor::UIViewportPanel::SetTarget(Scene* scene, entt::entity canvas)
{
    if (scene == m_TargetScene && canvas == m_TargetCanvas && HasTarget()) return;
    if (m_Interaction) m_Interaction->Cancel();
    m_Interaction.reset(); // Clears transient state while its retained scene is still alive.
    if (m_TargetScene && m_TargetScene != scene && !HasTarget())
    {
        auto& selection = SelectionManager::Get();
        for (const auto entity : m_TargetScene->GetRegistry().view<IDComponent>())
            if (selection.IsEntitySelected(m_TargetScene->FindEntity(entity)))
            {
                selection.ClearSelection(true);
                break;
            }
    }
    Safe_AddRef(scene);
    Safe_Release(m_TargetScene);
    m_TargetScene = scene;
    m_TargetCanvas = canvas;
    const auto* id = scene && scene->GetRegistry().valid(canvas) ?
        scene->GetRegistry().try_get<IDComponent>(canvas) : nullptr;
    m_TargetCanvasId = id ? id->id : 0;
    if (scene && id)
    {
        m_Interaction = std::make_unique<UIEventSystem>(*scene, canvas);
        m_Interaction->OnPointerEvent().AddLambda([this](const UIPointerEvent& event)
        {
            m_LastUIEvent = event.type;
            m_LastUIEventTarget = event.targetId;
            if (event.type == EUIPointerEventType::Click) ++m_ClickCount;
        });
    }
    m_LastUIEvent.reset();
    m_ClickCount = 0;
    m_Drag = {};
    m_PendingEdit.reset();
    m_Layouts.clear();
    m_PreviewReady = false;
    m_FitPending = true;
}

void Editor::UIViewportPanel::ResolveTarget()
{
    if (m_TargetScene && !HasTarget()) SetTarget(nullptr, entt::null);
    Entity* selected = SelectionManager::Get().GetPrimarySelectedEntity();
    if (selected && selected != m_LastSelection)
    {
        // Compare wrappers from live registries before dereferencing a possibly stale selection pointer.
        const auto* prefabScene = GetPrefabEditScene();
        for (Scene* scene : SceneManager::Get().GetActiveScenes())
        {
            if (prefabScene && scene != prefabScene) continue;
            for (const auto entity : scene->GetRegistry().view<IDComponent>())
            {
                if (scene->FindEntity(entity) != selected) continue;
                const auto canvas = UILayout::FindCanvas(scene->GetRegistry(), entity);
                if (canvas != entt::null) SetTarget(scene, canvas);
                break;
            }
        }
    }
    m_LastSelection = selected;
    if (HasTarget()) return;
    Scene* scene = SceneManager::Get().GetCurrentScene();
    if (!scene || !scene->IsActive()) return;
    entt::entity first = entt::null;
    uint64 firstId = UINT64_MAX;
    for (auto [entity, canvas, id] : scene->GetRegistry().view<CanvasComponent, IDComponent>().each())
        if (id.id < firstId) { first = entity; firstId = id.id; }
    if (first != entt::null) SetTarget(scene, first);
}

void Editor::UIViewportPanel::Update(f32 dt)
{
    ApplyPendingEdit();
    ResolveTarget();
    if (m_Interaction && (!m_Open || !m_InteractionPreview || !HasTarget())) m_Interaction->Cancel();
    if (!m_Open) { m_Drag = {}; m_Panning = false; return; }
    if (HasTarget())
    {
        const auto& canvas = m_TargetScene->GetRegistry().get<CanvasComponent>(m_TargetCanvas);
        if (UILayout::IsValid(canvas) && m_CanvasSize != canvas.referenceResolution)
        {
            m_CanvasSize = canvas.referenceResolution;
            m_FitPending = true;
            m_Drag = {};
            if (m_Interaction) m_Interaction->Cancel();
        }
    }
    auto* target = RenderTargetManager::Get().GetRenderTarget(m_CanvasRTName);
    if (!target) return;
    const auto width = static_cast<uint32>(m_CanvasSize.x);
    const auto height = static_cast<uint32>(m_CanvasSize.y);
    if (target->GetWidth() != width || target->GetHeight() != height)
    {
        if (target->Resize(width, height) != EResult::Success) { m_PreviewReady = false; return; }
    }
    auto* pass = RenderPassManager::Get().GetRenderPassByID(m_UIOverlayPassID);
    Renderer::Get().RegisterUICanvas(HasTarget() ? m_TargetScene : nullptr, m_TargetCanvas, pass);
    m_CanvasToDraw = m_CanvasRTName;
    m_ChannelFilter.SubmitChannelPreviewPass(m_CanvasRTName, m_CanvasToDraw);
    m_PreviewReady = true;
}

void Editor::UIViewportPanel::Draw()
{
    m_InteractionUpdated = false;
    BaseViewportPanel::Draw();
    if (!m_InteractionUpdated && m_Interaction) m_Interaction->Cancel();
}

float Editor::UIViewportPanel::GetCustomOptionsWidth() const
{
    return MeasureOptionLabels({ LOCAL_CSTR("Viewport.Canvas"), LOCAL_CSTR("Viewport.Order"),
        LOCAL_CSTR("Viewport.Snap"), LOCAL_CSTR("Viewport.View") });
}

void Editor::UIViewportPanel::DrawCustomOptions()
{
    DrawCanvasMenu();
    DrawOrderMenu();
    if (ImGui::BeginMenu(LOCAL_LABEL("Viewport.Snap").c_str()))
    {
        if (ImGui::Checkbox(LOCAL_LABEL("Viewport.Grid").c_str(), &m_GridSnap) && m_GridSnap) m_PixelSnap = false;
        if (ImGui::Checkbox(LOCAL_LABEL("Viewport.Pixel").c_str(), &m_PixelSnap) && m_PixelSnap) m_GridSnap = false;
        ImGui::InputFloat(LOCAL_LABEL("Viewport.GridStep").c_str(), &m_GridStep);
        if (!std::isfinite(m_GridStep) || m_GridStep <= 0.0f) m_GridStep = 10.0f;
        m_GridStep = std::clamp(m_GridStep, 0.25f, 4096.0f);
        ImGui::TextDisabled("%s", LOCAL_CSTR("Viewport.BypassSnap"));
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu(LOCAL_LABEL("Viewport.View").c_str()))
    {
        if (ImGui::MenuItem(LOCAL_LABEL("Viewport.Interact").c_str(), nullptr, m_InteractionPreview))
            ToggleInteractionPreview();
        if (ImGui::MenuItem(LOCAL_LABEL("Viewport.Fit").c_str(), ShortcutManager::Get().GetShortcutHint("Viewport.Focus"),
            false, CanFocusCanvas()))
            FocusCanvas();
        if (ImGui::MenuItem(LOCAL_LABEL("Viewport.ActualSize").c_str(), nullptr, false, HasTarget()))
            ShowActualSize();
        ImGui::Separator();
        ImGui::Checkbox(LOCAL_LABEL("Viewport.Grid").c_str(), &m_ShowGrid);
        ImGui::Checkbox(LOCAL_LABEL("Viewport.SafeArea").c_str(), &m_ShowSafeArea);
        ImGui::Checkbox(LOCAL_LABEL("Viewport.Anchors").c_str(), &m_ShowAnchors);
        ImGui::EndMenu();
    }
}

void Editor::UIViewportPanel::DrawQuickAccessBar()
{
    EditorUI::ToolbarLayout toolbar;
    EditorUI::IconButtonOptions interact;
    interact.Active = m_InteractionPreview;
    if (toolbar.Icon("Interact", ICON_FA_HAND_POINTER, "Viewport.Interact", interact))
        ToggleInteractionPreview();

    EditorUI::IconButtonOptions fit;
    fit.Enabled = CanFocusCanvas();
    fit.ShortcutHint = ShortcutManager::Get().GetShortcutHint("Viewport.Focus");
    if (toolbar.Icon("Fit", ICON_FA_EXPAND, "Viewport.Fit", fit))
        FocusCanvas();

    EditorUI::IconButtonOptions actualSize;
    actualSize.Enabled = HasTarget();
    if (toolbar.Icon("ActualSize", ICON_FA_PERCENT, "Viewport.ActualSize", actualSize))
        ShowActualSize();

    ImGuiTextBuffer zoom;
    zoom.appendf("%.0f%%", m_View.zoom * 100.0f);
    toolbar.Text(zoom.c_str());
    ImGui::Separator();
}

void Editor::UIViewportPanel::ToggleInteractionPreview()
{
    m_InteractionPreview = !m_InteractionPreview;
    m_Drag = {};
    m_PendingEdit.reset();
    m_Panning = false;
    if (m_Interaction) m_Interaction->Cancel();
}

void Editor::UIViewportPanel::ShowActualSize()
{
    m_FitPending = false;
    m_View.zoom = 1.0f;
    m_View.center = m_CanvasSize * 0.5f;
    m_Drag = {};
    if (m_Interaction) m_Interaction->Cancel();
}

void Editor::UIViewportPanel::DrawOrderMenu()
{
    if (!ImGui::BeginMenu(LOCAL_LABEL("Viewport.Order").c_str())) return;
    entt::entity selected = entt::null;
    auto& history = Application::Get().GetEditHistory();
    if (HasTarget())
    {
        auto& world = m_TargetScene->GetRegistry();
        auto* primary = SelectionManager::Get().GetPrimarySelectedEntity();
        for (const auto entity : world.view<IDComponent>())
            if (m_TargetScene->FindEntity(entity) == primary &&
                UILayout::FindCanvas(world, entity) == m_TargetCanvas)
            {
                selected = entity;
                break;
            }
    }
    if (selected == entt::null)
    {
        ImGui::TextDisabled("%s", LOCAL_CSTR("Viewport.OrderSelectionHint"));
        ImGui::EndMenu();
        return;
    }
    auto& world = m_TargetScene->GetRegistry();
    const bool canEdit = history.Enabled() && !m_InteractionPreview &&
        !SelectionManager::Get().IsPrimarySelectionLocked() &&
        (selected != m_TargetCanvas || !IsPrefabPreviewCanvas(m_TargetScene, m_TargetCanvas));
    ImGui::BeginDisabled(!canEdit);
    int value = UILayout::GetZIndex(world, selected);
    if (ImGui::InputInt(LOCAL_LABEL("Viewport.ZIndex").c_str(), &value, 0, 0) && canEdit) SetZIndex(selected, value);
    const auto current = UILayout::GetZIndex(world, selected);
    constexpr auto minimum = (std::numeric_limits<std::int32_t>::min)();
    constexpr auto maximum = (std::numeric_limits<std::int32_t>::max)();
    if (ImGui::MenuItem(LOCAL_LABEL("Viewport.IncreaseZ").c_str(), nullptr, false,
        current < maximum)) SetZIndex(selected, current + 1);
    if (ImGui::MenuItem(LOCAL_LABEL("Viewport.DecreaseZ").c_str(), nullptr, false,
        current > minimum)) SetZIndex(selected, current - 1);
    auto lowest = current;
    auto highest = current;
    std::size_t peers = 0;
    const auto includePeer = [&](entt::entity entity)
    {
        if (entity == selected || !world.valid(entity)) return;
        const auto order = UILayout::GetZIndex(world, entity);
        lowest = std::min(lowest, order);
        highest = std::max(highest, order);
        ++peers;
    };
    if (selected == m_TargetCanvas)
    {
        for (const auto entity : world.view<CanvasComponent, IDComponent>()) includePeer(entity);
    }
    else if (const auto* link = world.try_get<HierarchyComponent>(selected))
    {
        if (world.valid(link->parent))
            if (const auto* parent = world.try_get<HierarchyComponent>(link->parent))
                for (const auto entity : parent->children) includePeer(entity);
    }
    ImGui::Separator();
    if (ImGui::MenuItem(LOCAL_LABEL("Viewport.BringToFront").c_str(), nullptr, false, peers > 0 && highest < maximum))
        SetZIndex(selected, highest + 1);
    if (ImGui::MenuItem(LOCAL_LABEL("Viewport.SendToBack").c_str(), nullptr, false, peers > 0 && lowest > minimum))
        SetZIndex(selected, lowest - 1);
    ImGui::EndDisabled();
    const char* hintKey = selected == m_TargetCanvas ? "Viewport.CanvasOrderHint" : "Viewport.ElementOrderHint";
    ImGui::TextDisabled("%s", LOCAL_CSTR(hintKey));
    ImGui::EndMenu();
}

void Editor::UIViewportPanel::SetZIndex(entt::entity entity, std::int32_t zIndex)
{
    if (!HasTarget() || m_InteractionPreview || SelectionManager::Get().IsPrimarySelectionLocked()) return;
    if (entity == m_TargetCanvas && IsPrefabPreviewCanvas(m_TargetScene, m_TargetCanvas)) return;
    auto& history = Application::Get().GetEditHistory();
    auto& world = m_TargetScene->GetRegistry();
    if (!history.Enabled() || !world.valid(entity) || UILayout::FindCanvas(world, entity) != m_TargetCanvas ||
        UILayout::GetZIndex(world, entity) == zIndex) return;
    const std::array targets{ entity };
    auto before = SceneEditSnapshot::Capture(*m_TargetScene, targets);
    if (!before) return;
    world.get_or_emplace<UIRenderOrderComponent>(entity).zIndex = zIndex;
    history.Record(std::move(before), SceneEditSnapshot::Capture(*m_TargetScene, targets), "History.Command.UIRenderOrder", true);
    m_Drag = {};
    m_PendingEdit.reset();
    if (m_Interaction) m_Interaction->Cancel();
}

void Editor::UIViewportPanel::DrawCanvasMenu()
{
    if (!ImGui::BeginMenu(LOCAL_LABEL("Viewport.Canvas").c_str())) return;
    auto* current = SceneManager::Get().GetCurrentScene();
    auto* prefabScene = GetPrefabEditScene();
    if (ImGui::MenuItem(LOCAL_LABEL("Viewport.NewCanvas").c_str(), nullptr, false, current && !prefabScene))
    {
        auto& canvas = EntityFactory::CreateCanvasEntity(current);
        SetTarget(current, canvas.GetEntityHandle());
        SelectionManager::Get().SetSelectedEntity(&canvas);
    }
    Entity* parent = nullptr;
    if (HasTarget())
    {
        parent = IsPrefabPreviewCanvas(m_TargetScene, m_TargetCanvas)
            ? PrefabModeManager::Get().GetRootEntity() : m_TargetScene->FindEntity(m_TargetCanvas);
        auto* selected = SelectionManager::Get().GetPrimarySelectedEntity();
        if (selected && selected->GetScene() == m_TargetScene && FindLayout(selected->GetEntityHandle()))
            parent = selected;
        if (parent && TryGetEnabledComponent<UIScrollComponent>(m_TargetScene->GetRegistry(), parent->GetEntityHandle()))
        {
            const auto content = UIClipLayout::FindScrollContent(m_TargetScene->GetRegistry(), parent->GetEntityHandle());
            if (content != entt::null) parent = m_TargetScene->FindEntity(content);
        }
    }

    if (ImGui::MenuItem(LOCAL_LABEL("Viewport.AddImage").c_str(), nullptr, false, parent != nullptr) && parent)
        SelectionManager::Get().SetSelectedEntity(&EntityFactory::CreateUIImageEntity(m_TargetScene, *parent));
    if (ImGui::MenuItem(LOCAL_LABEL("Viewport.AddButton").c_str(), nullptr, false, parent != nullptr) && parent)
        SelectionManager::Get().SetSelectedEntity(&EntityFactory::CreateUIButtonEntity(m_TargetScene, *parent));
    if (ImGui::MenuItem(LOCAL_LABEL("Viewport.AddText").c_str(), nullptr, false, parent != nullptr) && parent)
    {
        const auto label = LOCAL("Viewport.NewTextLabel");
        SelectionManager::Get().SetSelectedEntity(&EntityFactory::CreateUITextEntity(m_TargetScene, *parent, Engine::Unicode::Utf8ToWide(label), label));
    }
    if (ImGui::MenuItem(LOCAL_LABEL("Viewport.AddTextInput").c_str(), nullptr, false, parent != nullptr) && parent)
    {
        const auto label = LOCAL("Viewport.AddTextInput");
        SelectionManager::Get().SetSelectedEntity(&EntityFactory::CreateUITextInputEntity(m_TargetScene, *parent, Engine::Unicode::Utf8ToWide(label)));
    }
    if (ImGui::MenuItem(LOCAL_LABEL("Viewport.AddDropdown").c_str(), nullptr, false, parent != nullptr) && parent)
    {
        const auto label = LOCAL("Viewport.AddDropdown");
        SelectionManager::Get().SetSelectedEntity(&EntityFactory::CreateUIDropdownEntity(m_TargetScene, *parent, Engine::Unicode::Utf8ToWide(label)));
    }
    if (ImGui::BeginMenu(LOCAL_LABEL("Viewport.UIPresets").c_str(), parent != nullptr))
    {
        struct PresetMenuEntry { UIElementPreset preset; const char* labelKey; };
        static constexpr PresetMenuEntry presets[] = {
            { UIElementPreset::Panel, "Viewport.AddPanel" },
            { UIElementPreset::Row, "Viewport.AddRow" },
            { UIElementPreset::Column, "Viewport.AddColumn" },
            { UIElementPreset::Grid, "Viewport.AddGrid" },
            { UIElementPreset::ProgressBar, "Viewport.AddProgressBar" },
            { UIElementPreset::Toggle, "Viewport.AddToggle" },
            { UIElementPreset::Slider, "Viewport.AddSlider" },
            { UIElementPreset::ClipPanel, "Viewport.AddClipPanel" },
            { UIElementPreset::EllipseMask, "Viewport.AddEllipseMask" },
            { UIElementPreset::ImageMask, "Viewport.AddImageMask" },
            { UIElementPreset::ScrollView, "Viewport.AddScrollView" },
            { UIElementPreset::TextInput, "Viewport.AddTextInput" },
            { UIElementPreset::Dropdown, "Viewport.AddDropdown" }
        };
        for (const auto& entry : presets)
        {
            if (ImGui::MenuItem(LOCAL_LABEL(entry.labelKey).c_str()) && parent)
            {
                auto& element = EntityFactory::CreateUIPreset(m_TargetScene, *parent, entry.preset,
                    Engine::Unicode::Utf8ToWide(LOCAL(entry.labelKey)));
                SelectionManager::Get().SetSelectedEntity(&element);
            }
        }
        ImGui::EndMenu();
    }

    ImGui::Separator();
    for (Scene* scene : SceneManager::Get().GetActiveScenes())
    {
        if (prefabScene && scene != prefabScene) continue;
        for (auto [entity, canvas, id] : scene->GetRegistry().view<CanvasComponent, IDComponent>().each())
        {
            const auto* name = scene->GetRegistry().try_get<NameComponent>(entity);
            const auto label =
                Engine::Unicode::WideToUtf8(scene->GetName()) + "/" + (name ? Engine::Unicode::WideToUtf8(name->name) : LOCAL("Viewport.Canvas")) +
                "##" + std::to_string(reinterpret_cast<uintptr_t>(scene)) + ":" + std::to_string(id.id);
            if (ImGui::MenuItem(label.c_str(), nullptr, scene == m_TargetScene && entity == m_TargetCanvas))
                SetTarget(scene, entity);
        }
    }
    if (HasTarget() && !IsPrefabPreviewCanvas(m_TargetScene, m_TargetCanvas))
    {
        auto& history = Application::Get().GetEditHistory();
        const std::array targets{ m_TargetCanvas };
        auto before = history.Enabled() ? SceneEditSnapshot::Capture(*m_TargetScene, targets) : SceneEditSnapshot{};
        bool changed = false;
        auto& canvas = m_TargetScene->GetRegistry().get<CanvasComponent>(m_TargetCanvas);
        const auto size = UILayout::IsValid(canvas) ? canvas.referenceResolution : m_CanvasSize;
        int resolution[] = { int(size.x), int(size.y) };
        if (ImGui::InputInt2(LOCAL_LABEL("Viewport.Resolution").c_str(), resolution))
        {
            CanvasComponent candidate = canvas;
            candidate.referenceResolution = vec2(resolution[0], resolution[1]);
            if (before && UILayout::IsValid(candidate) && candidate.referenceResolution != canvas.referenceResolution)
            {
                canvas = candidate;
                m_PreviewReady = false;
                changed = true;
            }
        }
        for (const auto& preset : g_ResolutionPresets)
            if (ImGui::MenuItem(LOCAL_LABEL(preset.NameKey).c_str()))
            {
                if (before && canvas.referenceResolution != vec2(preset.Width, preset.Height))
                {
                    canvas.referenceResolution = vec2(preset.Width, preset.Height);
                    m_PreviewReady = false;
                    changed = true;
                }
            }
        if (changed)
            history.Record(std::move(before), SceneEditSnapshot::Capture(*m_TargetScene, targets),
                "History.Command.CanvasResolution", true);
    }
    ImGui::EndMenu();
}

void Editor::UIViewportPanel::DrawCustomViewport()
{
    Application::Get().GetEditHistory().AllowShortcuts(ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows));
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 available = ImGui::GetContentRegionAvail();
    if (available.x < 1.0f || available.y < 1.0f) return;
    m_ImageScreenPos = origin;
    m_ImageSize = available;
    m_View.viewportOrigin = Point(origin);
    if (m_View.viewportSize != Point(available))
    {
        m_Drag = {};
        if (m_Interaction) m_Interaction->Cancel();
    }
    m_View.viewportSize = Point(available);
    if (m_FitPending)
    {
        m_View.Fit(m_CanvasSize);
        m_FitPending = false;
        if (m_Interaction) m_Interaction->Cancel();
    }
    ImGui::InvisibleButton("##UIWorkspace", available,
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);
    m_Hovered = ImGui::IsItemHovered();
    m_Focused = ImGui::IsWindowFocused();
    m_Layouts.clear();
    m_LayoutValid = HasTarget() && m_PreviewReady &&
        UILayout::Build(m_TargetScene->GetRegistry(), m_TargetCanvas, m_Layouts);
    HandleInput(m_Hovered);
    auto* drawList = ImGui::GetWindowDrawList();
    const ImVec2 end(origin.x + available.x, origin.y + available.y);
    drawList->PushClipRect(origin, end, true);
    drawList->AddRectFilled(origin, end, IM_COL32(27, 29, 33, 255));
    const ImVec2 topLeft = ImPoint(m_View.ToScreen(vec2(0.0f)));
    const ImVec2 bottomRight = ImPoint(m_View.ToScreen(m_CanvasSize));
    auto* target = RenderTargetManager::Get().GetRenderTarget(m_CanvasToDraw);
    if (target && target->GetTexture() && m_PreviewReady)
        drawList->AddImage(ImGuiManager::Get().GetImGuiTextureID(target->GetTexture()), topLeft, bottomRight);
    if (m_ShowGrid) DrawGrid(drawList);
    drawList->AddRect(topLeft, bottomRight, IM_COL32(170, 185, 205, 255), 0.0f, 0, 1.5f);
    const string label = LOCAL_FMT("Viewport.CanvasSize", m_CanvasSize.x, m_CanvasSize.y, m_View.zoom * 100.0f);
    drawList->AddText(ImVec2(topLeft.x + 4.0f, topLeft.y - ImGui::GetFontSize() - 5.0f), IM_COL32_WHITE, label.c_str());
    if (m_ShowSafeArea)
        drawList->AddRect(ImPoint(m_View.ToScreen(m_CanvasSize * 0.05f)),
            ImPoint(m_View.ToScreen(m_CanvasSize * 0.95f)), IM_COL32(100, 200, 170, 100));
    if (!m_InteractionPreview) DrawSelection(drawList);
    else
    {
        const string eventText = m_LastUIEvent ?
            LOCAL_FMT("Viewport.PointerEvent", LOCAL(UIPointerEventKey(*m_LastUIEvent)), m_LastUIEventTarget) : "";
        const string status = LOCAL_FMT("Viewport.InteractionStatus", m_ClickCount, eventText);
        drawList->AddText(ImVec2(origin.x + 12.0f, end.y - 26.0f), IM_COL32(100, 230, 170, 255), status.c_str());
    }
    if (!HasTarget() || !m_LayoutValid)
        drawList->AddText(ImVec2(origin.x + 12.0f, origin.y + 12.0f), IM_COL32(240, 190, 100, 255),
            LOCAL_CSTR(HasTarget() ? "Viewport.CanvasUnavailable" :
                GetPrefabEditScene() ? "Viewport.PrefabCanvasHint" : "Viewport.CreateCanvasHint"));
    if (!m_InteractionPreview && SelectionManager::Get().IsPrimarySelectionLocked())
        drawList->AddText(ImVec2(origin.x + 12.0f, end.y - 26.0f), IM_COL32(100, 240, 140, 255),
            LOCAL_CSTR("Viewport.SelectionLocked"));
    drawList->PopClipRect();
}

void Editor::UIViewportPanel::DrawGrid(ImDrawList* drawList)
{
    float step = m_GridStep;
    while (step * m_View.zoom < 12.0f) step *= 2.0f;
    const auto minimum = m_View.ToCanvas(m_View.viewportOrigin);
    const auto maximum = m_View.ToCanvas(m_View.viewportOrigin + m_View.viewportSize);
    const auto first = glm::floor(minimum / step);
    for (int axis = 0; axis < 2; ++axis)
    {
        for (int line = 0; line < 2048; ++line)
        {
            const float index = first[axis] + line;
            const float coordinate = index * step;
            if (coordinate > maximum[axis]) break;
            auto begin = minimum;
            auto end = maximum;
            begin[axis] = end[axis] = coordinate;
            const bool major = std::fmod(index, 5.0f) == 0.0f;
            drawList->AddLine(ImPoint(m_View.ToScreen(begin)), ImPoint(m_View.ToScreen(end)),
                major ? IM_COL32(125, 140, 160, 75) : IM_COL32(110, 125, 145, 35));
        }
    }
}

const Engine::UIElementLayout* Editor::UIViewportPanel::FindLayout(entt::entity entity) const
{
    const auto found = std::find_if(m_Layouts.begin(), m_Layouts.end(),
        [entity](const auto& layout) { return layout.entity == entity; });
    return found == m_Layouts.end() ? nullptr : &*found;
}

void Editor::UIViewportPanel::DrawSelection(ImDrawList* drawList)
{
    if (!m_LayoutValid || !HasTarget()) return;
    auto& selection = SelectionManager::Get();
    const auto& selected = selection.GetSelectedEntities();
    for (const auto& layout : m_Layouts)
    {
        if (!layout.visible) continue;
        auto* wrapper = m_TargetScene->FindEntity(layout.entity);
        if (std::find(selected.begin(), selected.end(), wrapper) == selected.end()) continue;
        ImVec2 screen[4];
        for (int corner = 0; corner < 4; ++corner)
            screen[corner] = ImPoint(m_View.ToScreen(vec2(layout.renderMatrix * vec4(CanvasCorners[corner], 0.0f, 1.0f))));
        drawList->AddQuad(screen[0], screen[1], screen[2], screen[3], IM_COL32(70, 180, 255, 255), 2.0f);
        const auto pivot = m_View.ToScreen(vec2(layout.worldTransform[3]));
        drawList->AddCircle(ImPoint(pivot), 4.0f, IM_COL32(255, 210, 80, 255), 12, 2.0f);
        if (wrapper != selection.GetPrimarySelectedEntity() || layout.managedByGroup) continue;
        if (!selection.IsPrimarySelectionLocked())
            for (const auto& point : screen)
                drawList->AddRectFilled(ImVec2(point.x - 5.0f, point.y - 5.0f),
                    ImVec2(point.x + 5.0f, point.y + 5.0f), IM_COL32(80, 190, 255, 255));
        if (m_ShowAnchors)
        {
            const auto& rect = m_TargetScene->GetRegistry().get<RectTransformComponent>(layout.entity);
            for (const auto anchor : { rect.anchorMin, rect.anchorMax })
            {
                const auto local = layout.parentSize * (anchor - layout.parentPivot);
                const auto point = m_View.ToScreen(vec2(layout.parentTransform * vec4(local, 0.0f, 1.0f)));
                drawList->AddLine(ImVec2(point.x - 5.0f, point.y), ImVec2(point.x + 5.0f, point.y),
                    IM_COL32(100, 230, 170, 230), 1.5f);
                drawList->AddLine(ImVec2(point.x, point.y - 5.0f), ImVec2(point.x, point.y + 5.0f),
                    IM_COL32(100, 230, 170, 230), 1.5f);
            }
        }
    }
}

bool Editor::UIViewportPanel::CanFocusCanvas() const
{
    return m_Open && HasTarget() && !m_Panning && m_Drag.mode == DragMode::None;
}

void Editor::UIViewportPanel::FocusCanvas()
{
    if (!CanFocusCanvas()) return;
    if (m_Interaction) m_Interaction->Cancel();
    m_FitPending = true;
}

void Editor::UIViewportPanel::HandleInput(bool hovered)
{
    const auto mouse = InputManager::Get().GetScreenMousePosition();
    if (hovered && InputManager::Get().IsMouseButtonDown(EMouseButton::Middle) && m_Drag.mode == DragMode::None) m_Panning = true;
    if (!InputManager::Get().IsMouseButtonPressed(EMouseButton::Middle) || !m_Focused) m_Panning = false;
    if (m_Panning)
    {
        if (m_Interaction) m_Interaction->Cancel();
        m_View.Pan(InputManager::Get().GetMouseDelta());
    }
    const float wheel = InputManager::Get().GetMouseScrollDeltaY();
    if (hovered && !m_Panning && m_Drag.mode == DragMode::None && wheel != 0.0f &&
        (!m_InteractionPreview || !m_Interaction || !m_Interaction->HasPointerCapture()))
    {
        if (m_Interaction) m_Interaction->Cancel();
        const bool scrolled = m_InteractionPreview && m_LayoutValid && HasTarget() &&
            UIClipLayout::ScrollAt(m_TargetScene->GetRegistry(), m_Layouts, m_View.ToCanvas(mouse), wheel);
        if (!scrolled) m_View.ZoomAt(mouse, std::pow(1.15f, wheel));
    }
    if (m_InteractionPreview) { m_Drag = {}; HandleInteraction(hovered); return; }
    if (!m_LayoutValid || !HasTarget()) { m_Drag = {}; return; }
    auto& selection = SelectionManager::Get();
    if (selection.IsPrimarySelectionLocked()) { m_Drag = {}; return; }
    auto& world = m_TargetScene->GetRegistry();
    const auto canvasPoint = m_View.ToCanvas(mouse);
    if (m_Drag.mode == DragMode::None && hovered && !m_Panning && InputManager::Get().IsMouseButtonDown(EMouseButton::Left))
    {
        const UIElementLayout* hit = nullptr;
        std::optional<vec2> resizeCorner;
        for (const auto& layout : m_Layouts)
        {
            if (!layout.visible || m_TargetScene->FindEntity(layout.entity) != selection.GetPrimarySelectedEntity()) continue;
            if (layout.managedByGroup) continue;
            float nearestDistanceSquared = 8.0f * 8.0f;
            for (const auto& corner : CanvasCorners)
            {
                const auto handle = m_View.ToScreen(vec2(layout.renderMatrix * vec4(corner, 0.0f, 1.0f)));
                const auto offset = handle - mouse;
                const float distanceSquared = glm::dot(offset, offset);
                if (distanceSquared > nearestDistanceSquared || (resizeCorner && distanceSquared == nearestDistanceSquared))
                    continue;
                nearestDistanceSquared = distanceSquared;
                resizeCorner = corner;
                hit = &layout;
            }
            break;
        }
        if (!hit && canvasPoint.x >= 0.0f && canvasPoint.y >= 0.0f &&
            canvasPoint.x < m_CanvasSize.x && canvasPoint.y < m_CanvasSize.y)
        {
            for (auto it = m_Layouts.rbegin(); it != m_Layouts.rend(); ++it)
            {
                const auto* image = TryGetEnabledComponent<UIImageComponent>(world, it->entity);
                const auto* text = TryGetEnabledComponent<UITextComponent>(world, it->entity);
                const bool hasVisibleContent = (image && image->color.a > 0.0f) || (text && text->color.a > 0.0f);
                if (!hasVisibleContent || !UILayout::Contains(*it, canvasPoint)) continue;
                hit = &*it;
                break;
            }
        }
        auto* wrapper = hit ? m_TargetScene->FindEntity(hit->entity) : nullptr;
        if (InputManager::Get().IsCtrlPressed())
        {
            if (wrapper) selection.ToggleEntitySelection(wrapper);
            return;
        }
        if (!wrapper) { selection.ClearSelection(); return; }
        selection.SetSelectedEntity(wrapper);
        if (hit->managedByGroup) return;
        const auto* id = world.try_get<IDComponent>(hit->entity);
        if (!id) return;
        m_Drag.mode = resizeCorner ? DragMode::Resize : DragMode::Move;
        if (resizeCorner) m_Drag.resizeCorner = *resizeCorner;
        m_Drag.entity = hit->entity;
        m_Drag.id = id->id;
        m_Drag.mouseStart = canvasPoint;
        m_Drag.start = world.get<RectTransformComponent>(hit->entity);
        m_Drag.layout = *hit;
    }
    if (m_Drag.mode == DragMode::None) return;
    const auto* id = world.valid(m_Drag.entity) ? world.try_get<IDComponent>(m_Drag.entity) : nullptr;
    const auto* layout = FindLayout(m_Drag.entity);
    if (!id || id->id != m_Drag.id || !layout || !layout->visible || !m_Focused ||
        layout->parentTransform != m_Drag.layout.parentTransform ||
        layout->parentSize != m_Drag.layout.parentSize || layout->parentPivot != m_Drag.layout.parentPivot)
    {
        m_Drag = {};
        return;
    }
    if (layout->managedByGroup)
    {
        m_Drag = {};
        return;
    }
    if (InputManager::Get().IsKeyButtonDown(EKeyCode::Escape))
    {
        m_PendingEdit = PendingEdit{ m_Drag.entity, m_Drag.id,
            world.get<RectTransformComponent>(m_Drag.entity), m_Drag.start };
        m_Drag = {};
        return;
    }
    if (!InputManager::Get().IsMouseButtonPressed(EMouseButton::Left)) { m_Drag = {}; return; }
    if (!InputManager::Get().IsMouseButtonDragging(EMouseButton::Left, 3.0f)) return;
    const float step = InputManager::Get().IsAltPressed() ? 0.0f : m_PixelSnap ? 1.0f : m_GridSnap ? m_GridStep : 0.0f;
    auto candidate = m_Drag.start;
    const auto delta = canvasPoint - m_Drag.mouseStart;
    const float parentDeterminant = glm::determinant(m_Drag.layout.parentTransform);
    if (!std::isfinite(parentDeterminant) || parentDeterminant == 0.0f) return;
    if (m_Drag.mode == DragMode::Move)
    {
        const vec2 startPivot(m_Drag.layout.worldTransform[3]);
        const auto desiredPivot = CanvasView2D::Snap(startPivot + delta, step);
        const auto parentDelta = vec2(glm::inverse(m_Drag.layout.parentTransform) *
            vec4(desiredPivot - startPivot, 0.0f, 0.0f));
        candidate.anchoredPosition += parentDelta;
    }
    else
    {
        const float determinant = glm::determinant(m_Drag.layout.worldTransform);
        if (!std::isfinite(determinant) || determinant == 0.0f) return;
        const auto localDelta = vec2(glm::inverse(m_Drag.layout.worldTransform) * vec4(delta, 0.0f, 0.0f));
        const auto resize = CanvasView2D::ResizeFromCorner(
            m_Drag.layout.size, candidate.pivot, m_Drag.resizeCorner, localDelta, step);
        candidate.sizeDelta += resize.sizeChange;
        const auto rotationScale = glm::rotate(mat4(1.0f), glm::radians(candidate.rotation), vec3(0.0f, 0.0f, 1.0f)) *
            glm::scale(mat4(1.0f), vec3(candidate.scale, 1.0f));
        candidate.anchoredPosition += vec2(rotationScale * vec4(resize.localPivotOffset, 0.0f, 0.0f));
    }
    if (UILayout::IsValid(candidate))
        m_PendingEdit = PendingEdit{ m_Drag.entity, m_Drag.id,
            world.get<RectTransformComponent>(m_Drag.entity), candidate };
}

void Editor::UIViewportPanel::HandleInteraction(bool hovered)
{
    m_InteractionUpdated = true;
    if (!m_Interaction) return;
    if (!m_LayoutValid || !HasTarget() || m_Panning || InputManager::Get().IsKeyButtonDown(EKeyCode::Escape))
    {
        m_Interaction->Cancel();
        return;
    }
    UIPointerInput input;
    input.position = m_View.ToCanvas(InputManager::Get().GetScreenMousePosition());
    input.insideViewport = hovered;
    input.focused = m_Focused && !ImGui::GetIO().AppFocusLost;
    input.held = InputManager::Get().IsMouseButtonPressed(EMouseButton::Left);
    input.down = InputManager::Get().IsMouseButtonDown(EMouseButton::Left);
    input.up = InputManager::Get().IsMouseButtonUp(EMouseButton::Left);
    m_Interaction->Update(input);
}

void Editor::UIViewportPanel::ApplyPendingEdit()
{
    auto& history = Application::Get().GetEditHistory();
    if (m_HistoryRevision != history.Revision())
    {
        m_HistoryRevision = history.Revision();
        m_PendingEdit.reset();
        m_Drag = {};
        m_InteractionUpdated = false;
        if (m_Interaction) m_Interaction->Cancel();
    }
    if (!m_PendingEdit) return;
    const auto edit = *m_PendingEdit;
    m_PendingEdit.reset();
    if (!HasTarget() || SelectionManager::Get().IsPrimarySelectionLocked()) { m_Drag = {}; return; }
    auto& world = m_TargetScene->GetRegistry();
    const auto* id = world.valid(edit.entity) ? world.try_get<IDComponent>(edit.entity) : nullptr;
    auto* current = id ? world.try_get<RectTransformComponent>(edit.entity) : nullptr;
    if (!id || id->id != edit.id || !current || !SameRect(*current, edit.before) ||
        UILayout::FindCanvas(world, edit.entity) != m_TargetCanvas) { m_Drag = {}; return; }
    if (!history.Enabled() || SameRect(edit.before, edit.after)) return;
    const std::array targets{ edit.entity };
    auto before = SceneEditSnapshot::Capture(*m_TargetScene, targets);
    if (!before) { m_Drag = {}; return; }
    *current = edit.after;
    std::vector<UIElementLayout> layouts;
    if (!UILayout::Build(world, m_TargetCanvas, layouts)) { *current = edit.before; m_Drag = {}; }
    else history.Record(std::move(before), SceneEditSnapshot::Capture(*m_TargetScene, targets), "History.Command.UITransform", true);
}
