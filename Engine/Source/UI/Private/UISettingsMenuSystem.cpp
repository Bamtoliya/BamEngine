#include "Engine_Includes.h"
#include "UISettingsMenuSystem.h"

#include "ComponentEnable.h"
#include "InputManager.h"
#include "RuntimeSettingsManager.h"
#include "RuntimeUIInput.h"
#include "UIComponents.h"
#include "WorldComponents.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <vector>

namespace Engine
{
    namespace UISettingsMenuDetail
    {
        constexpr std::array<std::pair<std::int32_t, std::int32_t>, 5> kSupportedResolutions = {{
            { 1280, 720 },
            { 1600, 900 },
            { 1920, 1080 },
            { 2560, 1440 },
            { 3840, 2160 }
        }};

        constexpr std::array<const char*, 4> kQualityPresetLabels = {{
            "Low",
            "Medium",
            "High",
            "Ultra"
        }};

        constexpr std::string_view kResolutionDropdownName = "ResolutionDropdown";
        constexpr std::string_view kQualityDropdownName = "QualityDropdown";
        constexpr std::string_view kFullscreenToggleName = "FullscreenToggle";
        constexpr std::string_view kVSyncToggleName = "VSyncToggle";
        constexpr std::string_view kMasterSliderName = "MasterVolumeSlider";
        constexpr std::string_view kBgmSliderName = "BGMVolumeSlider";
        constexpr std::string_view kSfxSliderName = "SFXVolumeSlider";
        constexpr std::string_view kUiSliderName = "UIVolumeSlider";
        constexpr std::string_view kVoiceSliderName = "VoiceVolumeSlider";
        constexpr std::string_view kApplyButtonName = "ApplyButton";
        constexpr std::string_view kResetButtonName = "ResetButton";
        constexpr std::string_view kCloseButtonName = "CloseButton";
        constexpr std::string_view kControlsButtonName = "ControlsButton";
        constexpr std::string_view kControlsPanelName = "ControlsPanel";
        constexpr std::string_view kControlsListContainerName = "ControlsListContainer";
        constexpr std::string_view kControlsResetButtonName = "ControlsResetButton";
        constexpr std::string_view kControlsBackButtonName = "ControlsBackButton";

        constexpr glm::vec4 kKeyBindNormalColor{ 0.24f, 0.27f, 0.33f, 1.0f };
        constexpr glm::vec4 kKeyBindAwaitingColor{ 0.72f, 0.45f, 0.16f, 1.0f };

        int FindResolutionIndex(std::int32_t width, std::int32_t height) noexcept
        {
            for (std::size_t i = 0; i < kSupportedResolutions.size(); ++i)
            {
                if (kSupportedResolutions[i].first == width && kSupportedResolutions[i].second == height)
                    return static_cast<int>(i);
            }
            return -1;
        }

        std::wstring ToWideName(std::string_view utf8)
        {
            return std::filesystem::u8path(std::string(utf8)).wstring();
        }

        entt::entity FindNamedDescendant(const entt::registry& world, entt::entity root, std::string_view name)
        {
            if (root == entt::null || !world.valid(root) || name.empty())
                return entt::null;

            const std::wstring targetName = ToWideName(name);
            std::vector<entt::entity> queue;
            queue.push_back(root);
            constexpr std::size_t maxVisited = 256;

            for (std::size_t head = 0; head < queue.size() && queue.size() < maxVisited; ++head)
            {
                const entt::entity current = queue[head];
                if (current != root)
                {
                    if (const auto* nameComp = world.try_get<NameComponent>(current);
                        nameComp && nameComp->name == targetName)
                    {
                        return current;
                    }
                }

                if (const auto* hierarchy = world.try_get<HierarchyComponent>(current))
                {
                    for (const entt::entity child : hierarchy->children)
                    {
                        if (world.valid(child))
                            queue.push_back(child);
                    }
                }
            }
            return entt::null;
        }

        entt::entity ResolveWidget(Scene& scene, entt::entity menuEntity,
            const SceneEntityReference& ref, std::string_view fallbackName)
        {
            if (ref.PersistentId != 0)
            {
                if (Entity* entity = scene.FindEntityById(ref.PersistentId))
                    return entity->GetEntityHandle();
            }
            return FindNamedDescendant(scene.GetRegistry(), menuEntity, fallbackName);
        }

        void SyncDropdown(entt::registry& world, entt::entity entity, int index)
        {
            if (entity == entt::null || !world.valid(entity))
                return;

            if (auto* dropdown = world.try_get<UIDropdownComponent>(entity))
            {
                if (index >= 0 && index < static_cast<int>(dropdown->options.size()))
                    dropdown->SelectIndex(index);
                else if (index == -1)
                    dropdown->SelectIndex(-1);

                if (auto* text = world.try_get<UITextComponent>(entity))
                    text->text = std::string(dropdown->GetSelectedLabel());
            }
        }

        void SyncToggle(entt::registry& world, entt::entity entity, bool checked)
        {
            if (entity == entt::null || !world.valid(entity))
                return;

            if (auto* toggle = world.try_get<UIToggleComponent>(entity))
                toggle->checked = checked;
        }

        void SyncSlider(entt::registry& world, entt::entity entity, float value)
        {
            if (entity == entt::null || !world.valid(entity))
                return;

            if (auto* slider = world.try_get<UISliderComponent>(entity))
                slider->value = UIWidgetRange::ClampSliderValue(*slider, value);
        }

        void ApplyWidgetsFromSettings(Scene& scene, entt::entity menuEntity,
            RuntimeSettingsMenuComponent& menu, const RuntimeSettings& settings)
        {
            auto& world = scene.GetRegistry();

            menu.pendingSettings = settings;
            menu.hasPendingChanges = false;
            menu.initialized = true;

            SyncDropdown(world, ResolveWidget(scene, menuEntity, menu.resolutionDropdown, kResolutionDropdownName),
                FindResolutionIndex(settings.ResolutionWidth, settings.ResolutionHeight));

            const int qualityIndex = std::clamp(settings.QualityPreset,
                RuntimeSettings::MinQualityPreset, RuntimeSettings::MaxQualityPreset);
            SyncDropdown(world, ResolveWidget(scene, menuEntity, menu.qualityDropdown, kQualityDropdownName), qualityIndex);

            SyncToggle(world, ResolveWidget(scene, menuEntity, menu.fullscreenToggle, kFullscreenToggleName), settings.Fullscreen);
            SyncToggle(world, ResolveWidget(scene, menuEntity, menu.vsyncToggle, kVSyncToggleName), settings.VSync);

            SyncSlider(world, ResolveWidget(scene, menuEntity, menu.masterVolumeSlider, kMasterSliderName), settings.MasterVolume);
            SyncSlider(world, ResolveWidget(scene, menuEntity, menu.bgmVolumeSlider, kBgmSliderName), settings.BGMVolume);
            SyncSlider(world, ResolveWidget(scene, menuEntity, menu.sfxVolumeSlider, kSfxSliderName), settings.SFXVolume);
            SyncSlider(world, ResolveWidget(scene, menuEntity, menu.uiVolumeSlider, kUiSliderName), settings.UIVolume);
            SyncSlider(world, ResolveWidget(scene, menuEntity, menu.voiceVolumeSlider, kVoiceSliderName), settings.VoiceVolume);
        }

        Entity& CreateUIEntity(Scene& scene, entt::entity parent, std::string_view name,
            const glm::vec2& position, const glm::vec2& size, std::int32_t zIndex)
        {
            Entity& entity = scene.CreateEntity();
            auto& world = scene.GetRegistry();
            const entt::entity handle = entity.GetEntityHandle();

            world.emplace<FlagComponent>(handle);
            world.emplace<NameComponent>(handle).name = ToWideName(name);
            world.emplace<HierarchyComponent>(handle).parent = parent;
            world.emplace<UIRenderOrderComponent>(handle).zIndex = zIndex;

            auto& rect = world.emplace<RectTransformComponent>(handle);
            rect.anchorMin = glm::vec2(0.0f);
            rect.anchorMax = glm::vec2(0.0f);
            rect.pivot = glm::vec2(0.0f);
            rect.anchoredPosition = position;
            rect.sizeDelta = size;

            if (parent != entt::null && world.valid(parent))
                world.get_or_emplace<HierarchyComponent>(parent).children.push_back(handle);

            return entity;
        }

        void CreateLabel(Scene& scene, entt::entity parent, std::string_view name,
            std::string_view text, std::string_view localeKey,
            const glm::vec2& position, const glm::vec2& size, int fontPixels = 22,
            EUITextAlignment alignment = EUITextAlignment::Left)
        {
            Entity& label = CreateUIEntity(scene, parent, name, position, size, 1);
            auto& textComp = scene.GetRegistry().emplace<UITextComponent>(label.GetEntityHandle());
            textComp.text = std::string(text);
            textComp.useLocalization = !localeKey.empty();
            textComp.localizationKey = std::string(localeKey);
            textComp.fontPixels = fontPixels;
            textComp.alignment = alignment;
            textComp.verticalAlignment = EUITextVerticalAlignment::Middle;
            textComp.color = glm::vec4(0.92f, 0.94f, 0.98f, 1.0f);
        }

        SceneEntityReference CreateButton(Scene& scene, entt::entity parent, std::string_view name,
            std::string_view label, std::string_view localeKey,
            const glm::vec2& position, const glm::vec2& size, const glm::vec4& normalColor)
        {
            Entity& entity = CreateUIEntity(scene, parent, name, position, size, 2);
            const entt::entity handle = entity.GetEntityHandle();
            auto& world = scene.GetRegistry();

            auto& image = world.emplace<UIImageComponent>(handle);
            image.color = glm::vec4(1.0f);
            image.raycastTarget = true;

            auto& button = world.emplace<UIButtonComponent>(handle);
            button.normalColor = normalColor;
            button.hoveredColor = glm::min(normalColor + glm::vec4(0.12f, 0.12f, 0.12f, 0.0f), glm::vec4(1.0f));
            button.pressedColor = glm::max(normalColor - glm::vec4(0.12f, 0.12f, 0.12f, 0.0f), glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));

            auto& textComp = world.emplace<UITextComponent>(handle);
            textComp.text = std::string(label);
            textComp.useLocalization = !localeKey.empty();
            textComp.localizationKey = std::string(localeKey);
            textComp.fontPixels = 20;
            textComp.alignment = EUITextAlignment::Center;
            textComp.verticalAlignment = EUITextVerticalAlignment::Middle;

            return SceneEntityReference{ world.get<IDComponent>(handle).id };
        }

        SceneEntityReference CreateToggle(Scene& scene, entt::entity parent, std::string_view name,
            const glm::vec2& position, const glm::vec2& size)
        {
            Entity& entity = CreateUIEntity(scene, parent, name, position, size, 2);
            const entt::entity handle = entity.GetEntityHandle();
            auto& world = scene.GetRegistry();

            auto& image = world.emplace<UIImageComponent>(handle);
            image.color = glm::vec4(0.18f, 0.20f, 0.24f, 1.0f);
            image.raycastTarget = true;

            auto& toggle = world.emplace<UIToggleComponent>(handle);
            toggle.checkColor = glm::vec4(0.26f, 0.65f, 0.96f, 1.0f);

            return SceneEntityReference{ world.get<IDComponent>(handle).id };
        }

        SceneEntityReference CreateSlider(Scene& scene, entt::entity parent, std::string_view name,
            const glm::vec2& position, const glm::vec2& size)
        {
            Entity& entity = CreateUIEntity(scene, parent, name, position, size, 2);
            const entt::entity handle = entity.GetEntityHandle();
            auto& world = scene.GetRegistry();

            auto& image = world.emplace<UIImageComponent>(handle);
            image.color = glm::vec4(0.16f, 0.17f, 0.21f, 1.0f);
            image.raycastTarget = true;

            auto& slider = world.emplace<UISliderComponent>(handle);
            slider.minimum = 0.0f;
            slider.maximum = 1.0f;
            slider.step = 0.05f;
            slider.value = 1.0f;

            return SceneEntityReference{ world.get<IDComponent>(handle).id };
        }

        SceneEntityReference CreateDropdown(Scene& scene, entt::entity parent, std::string_view name,
            const std::vector<std::string>& options, const glm::vec2& position, const glm::vec2& size, int zIndex)
        {
            Entity& entity = CreateUIEntity(scene, parent, name, position, size, zIndex);
            const entt::entity handle = entity.GetEntityHandle();
            auto& world = scene.GetRegistry();

            auto& image = world.emplace<UIImageComponent>(handle);
            image.color = glm::vec4(0.22f, 0.23f, 0.27f, 1.0f);
            image.raycastTarget = true;

            auto& dropdown = world.emplace<UIDropdownComponent>(handle);
            dropdown.options = options;
            dropdown.itemHeight = 32.0f;
            dropdown.maxVisibleItems = 5;

            auto& textComp = world.emplace<UITextComponent>(handle);
            textComp.useLocalization = false;
            textComp.fontPixels = 18;
            textComp.alignment = EUITextAlignment::Left;
            textComp.verticalAlignment = EUITextVerticalAlignment::Middle;

            return SceneEntityReference{ world.get<IDComponent>(handle).id };
        }

        void DestroyEntityAndDetach(Scene& scene, entt::entity parent, entt::entity child)
        {
            auto& world = scene.GetRegistry();
            if (child == entt::null || !world.valid(child))
                return;

            if (parent != entt::null && world.valid(parent))
            {
                if (auto* hierarchy = world.try_get<HierarchyComponent>(parent))
                {
                    std::erase(hierarchy->children, child);
                }
            }

            if (Entity* entity = scene.FindEntity(child))
                (void)scene.DestroyEntity(*entity);
        }

        std::string FormatBindingsLabel(const std::vector<std::string>& bindings)
        {
            std::string joined;
            for (std::size_t i = 0; i < bindings.size(); ++i)
            {
                if (i > 0)
                    joined += ", ";
                joined += bindings[i];
            }
            return joined;
        }

        void UpdateKeyBindingRowVisual(entt::registry& world, entt::entity buttonEntity,
            const std::vector<std::string>& bindings, bool awaiting)
        {
            if (buttonEntity == entt::null || !world.valid(buttonEntity))
                return;

            if (auto* button = world.try_get<UIButtonComponent>(buttonEntity))
            {
                const glm::vec4 baseColor = awaiting ? kKeyBindAwaitingColor : kKeyBindNormalColor;
                button->normalColor = baseColor;
                button->hoveredColor = glm::min(baseColor + glm::vec4(0.12f, 0.12f, 0.12f, 0.0f), glm::vec4(1.0f));
                button->pressedColor = glm::max(baseColor - glm::vec4(0.12f, 0.12f, 0.12f, 0.0f), glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
            }

            if (auto* textComp = world.try_get<UITextComponent>(buttonEntity))
            {
                if (awaiting)
                {
                    textComp->text = "Press any key...";
                    textComp->useLocalization = true;
                    textComp->localizationKey = "UI.Settings.PressAnyKey";
                }
                else if (bindings.empty())
                {
                    textComp->text = "Unbound";
                    textComp->useLocalization = true;
                    textComp->localizationKey = "UI.Settings.Unbound";
                }
                else
                {
                    textComp->text = FormatBindingsLabel(bindings);
                    textComp->useLocalization = false;
                    textComp->localizationKey.clear();
                }
            }
        }

        void RefreshKeyBindingsImpl(Scene& scene, entt::entity menuEntity,
            std::string_view awaitingMapId, std::string_view awaitingActionId)
        {
            if (!InputManager::IsCreated())
                return;

            auto& world = scene.GetRegistry();
            if (!world.valid(menuEntity))
                return;

            auto* menu = world.try_get<RuntimeSettingsMenuComponent>(menuEntity);
            if (!menu)
                return;

            const entt::entity listContainer = ResolveWidget(
                scene, menuEntity, menu->controlsListContainer, kControlsListContainerName);
            if (listContainer == entt::null || !world.valid(listContainer))
                return;

            const auto& input = InputManager::Get();
            const InputActionSettings& actionSettings = input.GetCurrentInputActionSettings();

            std::vector<std::pair<std::string, std::string>> desiredRows;
            for (const auto& map : actionSettings.Maps)
            {
                for (const auto& action : map.Actions)
                    desiredRows.emplace_back(map.Id, action.Id);
            }

            bool needsRebuild = (menu->keyBindingRows.size() != desiredRows.size());
            if (!needsRebuild)
            {
                for (std::size_t i = 0; i < desiredRows.size(); ++i)
                {
                    const auto& row = menu->keyBindingRows[i];
                    if (row.mapId != desiredRows[i].first || row.actionId != desiredRows[i].second ||
                        !world.valid(row.buttonEntity))
                    {
                        needsRebuild = true;
                        break;
                    }
                }
            }

            if (needsRebuild)
            {
                for (const auto& row : menu->keyBindingRows)
                {
                    DestroyEntityAndDetach(scene, listContainer, row.labelEntity);
                    DestroyEntityAndDetach(scene, listContainer, row.buttonEntity);
                }
                menu->keyBindingRows.clear();

                const bool multiMap = actionSettings.Maps.size() > 1;
                constexpr float rowHeight = 38.0f;
                constexpr float rowStep = 48.0f;
                for (std::size_t i = 0; i < desiredRows.size(); ++i)
                {
                    const auto& [mapId, actionId] = desiredRows[i];
                    const float rowY = 8.0f + static_cast<float>(i) * rowStep;
                    const std::string displayLabel = multiMap ? (mapId + " - " + actionId) : actionId;

                    Entity& labelEnt = CreateUIEntity(scene, listContainer,
                        "KeyBindLabel_" + mapId + "_" + actionId,
                        { 16.0f, rowY }, { 320.0f, rowHeight }, 1);
                    auto& labelText = world.emplace<UITextComponent>(labelEnt.GetEntityHandle());
                    labelText.text = displayLabel;
                    labelText.useLocalization = false;
                    labelText.fontPixels = 20;
                    labelText.alignment = EUITextAlignment::Left;
                    labelText.verticalAlignment = EUITextVerticalAlignment::Middle;
                    labelText.color = glm::vec4(0.92f, 0.94f, 0.98f, 1.0f);

                    const SceneEntityReference btnRef = CreateButton(scene, listContainer,
                        "KeyBindButton_" + mapId + "_" + actionId,
                        "", "", { 356.0f, rowY }, { 288.0f, rowHeight }, kKeyBindNormalColor);
                    Entity* btnEnt = scene.FindEntityById(btnRef.PersistentId);

                    menu->keyBindingRows.push_back({
                        mapId,
                        actionId,
                        labelEnt.GetEntityHandle(),
                        btnEnt ? btnEnt->GetEntityHandle() : entt::null
                    });
                }
            }

            for (const auto& row : menu->keyBindingRows)
            {
                const bool awaiting = (!awaitingMapId.empty() && !awaitingActionId.empty() &&
                    row.mapId == awaitingMapId && row.actionId == awaitingActionId);
                const std::vector<std::string> bindings = input.GetActionBindings(row.mapId, row.actionId);
                UpdateKeyBindingRowVisual(world, row.buttonEntity, bindings, awaiting);
            }
        }
    }

    Entity* CreateInGameSettingsMenu(Scene& scene, const InGameSettingsMenuDesc& desc)
    {
        using namespace UISettingsMenuDetail;

        Entity& canvasEntity = scene.CreateEntity();
        auto& world = scene.GetRegistry();
        const entt::entity canvasHandle = canvasEntity.GetEntityHandle();

        auto& flag = world.emplace<FlagComponent>(canvasHandle);
        if (!desc.startVisible)
            flag.flags = EEntityFlag::None;

        world.emplace<NameComponent>(canvasHandle).name = ToWideName(desc.name);
        world.emplace<HierarchyComponent>(canvasHandle);
        world.emplace<CanvasComponent>(canvasHandle).referenceResolution = desc.referenceResolution;
        world.emplace<UIRenderOrderComponent>(canvasHandle).zIndex = desc.zIndex;

        auto& menu = world.emplace<RuntimeSettingsMenuComponent>(canvasHandle);
        menu.saveOnApply = desc.saveOnApply;
        menu.settingsFilePath = desc.settingsFilePath;
        menu.bindingsFilePath = desc.bindingsFilePath;

        // 화면 전체 반투명 배경 (뒤쪽 입력 차단)
        Entity& backdrop = CreateUIEntity(scene, canvasHandle, "Backdrop", { 0.0f, 0.0f }, desc.referenceResolution, 0);
        {
            auto& bgImage = world.emplace<UIImageComponent>(backdrop.GetEntityHandle());
            bgImage.color = glm::vec4(0.04f, 0.05f, 0.07f, 0.65f);
            bgImage.raycastTarget = true;
        }

        // 중앙 설정 패널
        constexpr glm::vec2 panelSize{ 760.0f, 660.0f };
        const glm::vec2 panelPos = (desc.referenceResolution - panelSize) * 0.5f;
        Entity& panel = CreateUIEntity(scene, canvasHandle, "SettingsPanel", panelPos, panelSize, 1);
        const entt::entity panelHandle = panel.GetEntityHandle();
        {
            auto& panelImage = world.emplace<UIImageComponent>(panelHandle);
            panelImage.color = glm::vec4(0.11f, 0.12f, 0.15f, 0.96f);
            panelImage.raycastTarget = true;
        }

        CreateLabel(scene, panelHandle, "TitleLabel", "Settings", "UI.Settings.Title",
            { 32.0f, 20.0f }, { 696.0f, 44.0f }, 30, EUITextAlignment::Center);

        constexpr float labelX = 40.0f;
        constexpr float labelW = 240.0f;
        constexpr float controlX = 300.0f;
        constexpr float controlW = 420.0f;
        constexpr float rowH = 38.0f;
        constexpr float rowStep = 50.0f;
        float y = 84.0f;

        // Resolution
        CreateLabel(scene, panelHandle, "ResolutionLabel", "Resolution", "UI.Settings.Resolution",
            { labelX, y }, { labelW, rowH });
        std::vector<std::string> resolutionOptions;
        resolutionOptions.reserve(kSupportedResolutions.size());
        for (const auto& [w, h] : kSupportedResolutions)
            resolutionOptions.push_back(std::to_string(w) + "x" + std::to_string(h));
        menu.resolutionDropdown = CreateDropdown(scene, panelHandle, kResolutionDropdownName,
            resolutionOptions, { controlX, y }, { controlW, rowH }, 10);
        y += rowStep;

        // Quality
        CreateLabel(scene, panelHandle, "QualityLabel", "Quality", "UI.Settings.Quality",
            { labelX, y }, { labelW, rowH });
        std::vector<std::string> qualityOptions(kQualityPresetLabels.begin(), kQualityPresetLabels.end());
        menu.qualityDropdown = CreateDropdown(scene, panelHandle, kQualityDropdownName,
            qualityOptions, { controlX, y }, { controlW, rowH }, 9);
        y += rowStep;

        // Fullscreen
        CreateLabel(scene, panelHandle, "FullscreenLabel", "Fullscreen", "UI.Settings.Fullscreen",
            { labelX, y }, { labelW, rowH });
        menu.fullscreenToggle = CreateToggle(scene, panelHandle, kFullscreenToggleName,
            { controlX, y + 3.0f }, { 32.0f, 32.0f });
        y += rowStep;

        // VSync
        CreateLabel(scene, panelHandle, "VSyncLabel", "VSync", "UI.Settings.VSync",
            { labelX, y }, { labelW, rowH });
        menu.vsyncToggle = CreateToggle(scene, panelHandle, kVSyncToggleName,
            { controlX, y + 3.0f }, { 32.0f, 32.0f });
        y += rowStep;

        // Audio Volumes
        struct SliderRow
        {
            std::string_view labelName;
            std::string_view labelText;
            std::string_view localeKey;
            std::string_view sliderName;
            SceneEntityReference* targetRef;
        };
        const std::array<SliderRow, 5> sliderRows = {{
            { "MasterVolumeLabel", "Master Volume", "UI.Settings.MasterVolume", kMasterSliderName, &menu.masterVolumeSlider },
            { "BGMVolumeLabel", "BGM Volume", "UI.Settings.BGMVolume", kBgmSliderName, &menu.bgmVolumeSlider },
            { "SFXVolumeLabel", "SFX Volume", "UI.Settings.SFXVolume", kSfxSliderName, &menu.sfxVolumeSlider },
            { "UIVolumeLabel", "UI Volume", "UI.Settings.UIVolume", kUiSliderName, &menu.uiVolumeSlider },
            { "VoiceVolumeLabel", "Voice Volume", "UI.Settings.VoiceVolume", kVoiceSliderName, &menu.voiceVolumeSlider }
        }};

        for (const auto& row : sliderRows)
        {
            CreateLabel(scene, panelHandle, row.labelName, row.labelText, row.localeKey,
                { labelX, y }, { labelW, rowH });
            *row.targetRef = CreateSlider(scene, panelHandle, row.sliderName,
                { controlX, y + 7.0f }, { controlW, 24.0f });
            y += rowStep;
        }

        // Bottom Buttons: [Reset] [Controls]  [Apply] [Close]
        constexpr float btnY = 588.0f;
        constexpr float btnW = 140.0f;
        constexpr float btnH = 44.0f;
        menu.resetButton = CreateButton(scene, panelHandle, kResetButtonName,
            "Defaults", "UI.Settings.Defaults", { 40.0f, btnY }, { btnW, btnH }, { 0.30f, 0.32f, 0.38f, 1.0f });
        menu.controlsButton = CreateButton(scene, panelHandle, kControlsButtonName,
            "Controls", "UI.Settings.Controls", { 200.0f, btnY }, { btnW, btnH }, { 0.28f, 0.34f, 0.42f, 1.0f });
        menu.applyButton = CreateButton(scene, panelHandle, kApplyButtonName,
            "Apply", "UI.Settings.Apply", { 424.0f, btnY }, { btnW, btnH }, { 0.22f, 0.52f, 0.88f, 1.0f });
        menu.closeButton = CreateButton(scene, panelHandle, kCloseButtonName,
            "Close", "UI.Settings.Close", { 580.0f, btnY }, { btnW, btnH }, { 0.38f, 0.26f, 0.26f, 1.0f });

        // 조작 키 리매핑 서브 패널 (초기 숨김, 기존 위젯 좌표/동작 비간섭)
        Entity& controlsPanel = CreateUIEntity(scene, canvasHandle, kControlsPanelName, panelPos, panelSize, 20);
        const entt::entity controlsPanelHandle = controlsPanel.GetEntityHandle();
        world.get<FlagComponent>(controlsPanelHandle).flags = EEntityFlag::None;
        menu.controlsPanel = SceneEntityReference{ world.get<IDComponent>(controlsPanelHandle).id };
        {
            auto& cpImage = world.emplace<UIImageComponent>(controlsPanelHandle);
            cpImage.color = glm::vec4(0.11f, 0.12f, 0.15f, 0.98f);
            cpImage.raycastTarget = true;
        }

        CreateLabel(scene, controlsPanelHandle, "ControlsTitleLabel", "Key Bindings", "UI.Settings.ControlsTitle",
            { 32.0f, 20.0f }, { 696.0f, 44.0f }, 30, EUITextAlignment::Center);

        Entity& controlsList = CreateUIEntity(scene, controlsPanelHandle, kControlsListContainerName,
            { 40.0f, 84.0f }, { 680.0f, 480.0f }, 1);
        const entt::entity controlsListHandle = controlsList.GetEntityHandle();
        menu.controlsListContainer = SceneEntityReference{ world.get<IDComponent>(controlsListHandle).id };
        {
            auto& listBg = world.emplace<UIImageComponent>(controlsListHandle);
            listBg.color = glm::vec4(0.08f, 0.09f, 0.12f, 0.65f);
            listBg.raycastTarget = true;
            world.emplace<UIClipComponent>(controlsListHandle);
            auto& scroll = world.emplace<UIScrollComponent>(controlsListHandle);
            scroll.wheelStep = 40.0f;
            scroll.showScrollbar = true;
        }

        menu.controlsResetButton = CreateButton(scene, controlsPanelHandle, kControlsResetButtonName,
            "Reset Controls", "UI.Settings.ResetControls", { 40.0f, btnY }, { 180.0f, btnH }, { 0.30f, 0.32f, 0.38f, 1.0f });
        menu.controlsBackButton = CreateButton(scene, controlsPanelHandle, kControlsBackButtonName,
            "Back", "UI.Settings.Back", { 580.0f, btnY }, { btnW, btnH }, { 0.38f, 0.26f, 0.26f, 1.0f });

        UISettingsMenuSystem::SyncFromManager(scene, canvasHandle);
        return &canvasEntity;
    }

    UISettingsMenuSystem* UISettingsMenuSystem::Create(void*)
    {
        return new UISettingsMenuSystem();
    }

    void UISettingsMenuSystem::OnInitialize()
    {
        if (RuntimeSettingsManager::IsCreated())
        {
            m_SettingsListenerHandle = RuntimeSettingsManager::Get().AddChangeListener(
                [this](const RuntimeSettings&, ERuntimeSettingField)
                {
                    m_ExternalSettingsDirty = true;
                });
        }
    }

    void UISettingsMenuSystem::OnFree()
    {
        if (m_SettingsListenerHandle != 0 && RuntimeSettingsManager::IsCreated())
            RuntimeSettingsManager::Get().RemoveChangeListener(m_SettingsListenerHandle);
        m_SettingsListenerHandle = {};
        m_ExternalSettingsDirty = false;
        CancelRebind(nullptr);
    }

    void UISettingsMenuSystem::OnUpdate(entt::registry&, const vector<Scene*>& activeScenes, f32)
    {
        const bool externalDirty = m_ExternalSettingsDirty;
        m_ExternalSettingsDirty = false;

        for (Scene* scene : activeScenes)
        {
            if (!scene || !scene->IsActive() || scene->IsPaused())
                continue;

            auto& world = scene->GetRegistry();
            for (const entt::entity menuEntity : world.view<RuntimeSettingsMenuComponent>())
            {
                if (!IsComponentEnabled<RuntimeSettingsMenuComponent>(world, menuEntity))
                    continue;

                auto& menu = world.get<RuntimeSettingsMenuComponent>(menuEntity);
                const auto* flag = world.try_get<FlagComponent>(menuEntity);
                const bool visible = !flag || flag->IsVisibleInHierarchy();

                const bool becameVisible = visible && !menu.wasVisible;
                menu.wasVisible = visible;

                if (!menu.initialized || (menu.autoSyncOnEnable && becameVisible) ||
                    (externalDirty && !menu.hasPendingChanges))
                {
                    SyncFromManager(*scene, menuEntity);
                }
            }

            if (m_AwaitingRebindMenuEntity != entt::null &&
                !m_AwaitingRebindMapId.empty() && !m_AwaitingRebindActionId.empty())
            {
                PollRebindInput(*scene);
            }

            for (const UIPointerEvent& event : GetRuntimeUIPointerEvents(*scene))
                HandleInstancePointerEvent(*scene, event);
        }
    }

    void UISettingsMenuSystem::SyncFromManager(Scene& scene, entt::entity menuEntity)
    {
        auto& world = scene.GetRegistry();
        if (!world.valid(menuEntity))
            return;

        auto* menu = world.try_get<RuntimeSettingsMenuComponent>(menuEntity);
        if (!menu)
            return;

        const RuntimeSettings applied = RuntimeSettingsManager::IsCreated()
            ? RuntimeSettingsManager::Get().GetAppliedSettings()
            : RuntimeSettingsManager::GetDefaultSettings();

        UISettingsMenuDetail::ApplyWidgetsFromSettings(scene, menuEntity, *menu, applied);
        RefreshKeyBindings(scene, menuEntity);
    }

    void UISettingsMenuSystem::SyncAllFromManager(Scene& scene)
    {
        auto& world = scene.GetRegistry();
        for (const entt::entity menuEntity : world.view<RuntimeSettingsMenuComponent>())
            SyncFromManager(scene, menuEntity);
    }

    bool UISettingsMenuSystem::HandlePointerEvent(Scene& scene, const UIPointerEvent& event)
    {
        using namespace UISettingsMenuDetail;

        if (event.type != EUIPointerEventType::ValueChanged && event.type != EUIPointerEventType::Click)
            return false;

        auto& world = scene.GetRegistry();
        if (!world.valid(event.target))
            return false;

        for (const entt::entity menuEntity : world.view<RuntimeSettingsMenuComponent>())
        {
            if (!IsComponentEnabled<RuntimeSettingsMenuComponent>(world, menuEntity))
                continue;

            auto& menu = world.get<RuntimeSettingsMenuComponent>(menuEntity);

            if (event.type == EUIPointerEventType::ValueChanged)
            {
                if (event.target == ResolveWidget(scene, menuEntity, menu.resolutionDropdown, kResolutionDropdownName))
                {
                    const int idx = static_cast<int>(event.value);
                    if (idx >= 0 && idx < static_cast<int>(kSupportedResolutions.size()))
                    {
                        const auto [w, h] = kSupportedResolutions[static_cast<std::size_t>(idx)];
                        menu.pendingSettings.ResolutionWidth = w;
                        menu.pendingSettings.ResolutionHeight = h;
                        menu.hasPendingChanges = true;
                    }
                    return true;
                }

                if (event.target == ResolveWidget(scene, menuEntity, menu.qualityDropdown, kQualityDropdownName))
                {
                    const int idx = static_cast<int>(event.value);
                    if (RuntimeSettings::IsValidQualityPreset(idx))
                    {
                        menu.pendingSettings.QualityPreset = idx;
                        menu.hasPendingChanges = true;
                    }
                    return true;
                }

                if (event.target == ResolveWidget(scene, menuEntity, menu.fullscreenToggle, kFullscreenToggleName))
                {
                    menu.pendingSettings.Fullscreen = (event.value != 0.0f);
                    menu.hasPendingChanges = true;
                    return true;
                }

                if (event.target == ResolveWidget(scene, menuEntity, menu.vsyncToggle, kVSyncToggleName))
                {
                    menu.pendingSettings.VSync = (event.value != 0.0f);
                    menu.hasPendingChanges = true;
                    return true;
                }

                if (event.target == ResolveWidget(scene, menuEntity, menu.masterVolumeSlider, kMasterSliderName))
                {
                    menu.pendingSettings.MasterVolume = std::clamp(event.value, 0.0f, 1.0f);
                    menu.hasPendingChanges = true;
                    return true;
                }

                if (event.target == ResolveWidget(scene, menuEntity, menu.bgmVolumeSlider, kBgmSliderName))
                {
                    menu.pendingSettings.BGMVolume = std::clamp(event.value, 0.0f, 1.0f);
                    menu.hasPendingChanges = true;
                    return true;
                }

                if (event.target == ResolveWidget(scene, menuEntity, menu.sfxVolumeSlider, kSfxSliderName))
                {
                    menu.pendingSettings.SFXVolume = std::clamp(event.value, 0.0f, 1.0f);
                    menu.hasPendingChanges = true;
                    return true;
                }

                if (event.target == ResolveWidget(scene, menuEntity, menu.uiVolumeSlider, kUiSliderName))
                {
                    menu.pendingSettings.UIVolume = std::clamp(event.value, 0.0f, 1.0f);
                    menu.hasPendingChanges = true;
                    return true;
                }

                if (event.target == ResolveWidget(scene, menuEntity, menu.voiceVolumeSlider, kVoiceSliderName))
                {
                    menu.pendingSettings.VoiceVolume = std::clamp(event.value, 0.0f, 1.0f);
                    menu.hasPendingChanges = true;
                    return true;
                }
            }
            else if (event.type == EUIPointerEventType::Click)
            {
                if (event.target == ResolveWidget(scene, menuEntity, menu.applyButton, kApplyButtonName))
                {
                    if (RuntimeSettingsManager::IsCreated())
                    {
                        auto& mgr = RuntimeSettingsManager::Get();
                        const RuntimeSettingsApplyResult res = mgr.ApplySettings(menu.pendingSettings);
                        if (res.Succeeded && menu.saveOnApply && !menu.settingsFilePath.empty())
                            (void)mgr.SaveToFile(std::filesystem::u8path(menu.settingsFilePath));
                    }
                    SyncFromManager(scene, menuEntity);
                    return true;
                }

                if (event.target == ResolveWidget(scene, menuEntity, menu.resetButton, kResetButtonName))
                {
                    if (RuntimeSettingsManager::IsCreated())
                    {
                        auto& mgr = RuntimeSettingsManager::Get();
                        const RuntimeSettingsApplyResult res = mgr.ResetToDefaults();
                        if (res.Succeeded && menu.saveOnApply && !menu.settingsFilePath.empty())
                            (void)mgr.SaveToFile(std::filesystem::u8path(menu.settingsFilePath));
                    }
                    SyncFromManager(scene, menuEntity);
                    return true;
                }

                if (event.target == ResolveWidget(scene, menuEntity, menu.closeButton, kCloseButtonName))
                {
                    CloseMenu(scene, menuEntity);
                    return true;
                }

                if (event.target == ResolveWidget(scene, menuEntity, menu.controlsButton, kControlsButtonName))
                {
                    OpenControlsPanel(scene, menuEntity);
                    return true;
                }

                if (event.target == ResolveWidget(scene, menuEntity, menu.controlsBackButton, kControlsBackButtonName))
                {
                    CloseControlsPanel(scene, menuEntity);
                    return true;
                }

                if (event.target == ResolveWidget(scene, menuEntity, menu.controlsResetButton, kControlsResetButtonName))
                {
                    if (InputManager::IsCreated())
                    {
                        auto& input = InputManager::Get();
                        input.ResetAllBindingsToDefault();
                        if (!menu.bindingsFilePath.empty())
                            (void)input.SaveBindingsToFile(std::filesystem::u8path(menu.bindingsFilePath));
                    }
                    RefreshKeyBindings(scene, menuEntity);
                    return true;
                }
            }
        }
        return false;
    }

    bool UISettingsMenuSystem::HandleInstancePointerEvent(Scene& scene, const UIPointerEvent& event)
    {
        if (event.type == EUIPointerEventType::Click)
        {
            auto& world = scene.GetRegistry();
            if (world.valid(event.target))
            {
                for (const entt::entity menuEntity : world.view<RuntimeSettingsMenuComponent>())
                {
                    if (!IsComponentEnabled<RuntimeSettingsMenuComponent>(world, menuEntity))
                        continue;

                    auto& menu = world.get<RuntimeSettingsMenuComponent>(menuEntity);
                    if (event.target == UISettingsMenuDetail::ResolveWidget(
                            scene, menuEntity, menu.controlsBackButton, UISettingsMenuDetail::kControlsBackButtonName))
                    {
                        CancelRebind(&scene);
                        CloseControlsPanel(scene, menuEntity);
                        return true;
                    }

                    if (event.target == UISettingsMenuDetail::ResolveWidget(
                            scene, menuEntity, menu.controlsResetButton, UISettingsMenuDetail::kControlsResetButtonName))
                    {
                        CancelRebind(&scene);
                        if (InputManager::IsCreated())
                        {
                            auto& input = InputManager::Get();
                            input.ResetAllBindingsToDefault();
                            if (!menu.bindingsFilePath.empty())
                                (void)input.SaveBindingsToFile(std::filesystem::u8path(menu.bindingsFilePath));
                        }
                        RefreshKeyBindings(scene, menuEntity);
                        return true;
                    }

                    if (event.target == UISettingsMenuDetail::ResolveWidget(
                            scene, menuEntity, menu.closeButton, UISettingsMenuDetail::kCloseButtonName))
                    {
                        CancelRebind(&scene);
                        CloseMenu(scene, menuEntity);
                        return true;
                    }

                    if (menu.controlsVisible)
                    {
                        for (const auto& row : menu.keyBindingRows)
                        {
                            if (event.target == row.buttonEntity)
                            {
                                BeginRebind(scene, menuEntity, row.mapId, row.actionId);
                                return true;
                            }
                        }
                    }
                }
            }
        }

        return HandlePointerEvent(scene, event);
    }

    void UISettingsMenuSystem::OpenMenu(Scene& scene, entt::entity menuEntity)
    {
        auto& world = scene.GetRegistry();
        if (!world.valid(menuEntity))
            return;

        if (auto* flag = world.try_get<FlagComponent>(menuEntity))
            flag->flags = EEntityFlag::Default;

        if (auto* menu = world.try_get<RuntimeSettingsMenuComponent>(menuEntity))
            menu->wasVisible = true;

        CloseControlsPanel(scene, menuEntity);
        SyncFromManager(scene, menuEntity);
    }

    void UISettingsMenuSystem::CloseMenu(Scene& scene, entt::entity menuEntity)
    {
        auto& world = scene.GetRegistry();
        if (!world.valid(menuEntity))
            return;

        CloseControlsPanel(scene, menuEntity);
        SyncFromManager(scene, menuEntity);

        if (auto* flag = world.try_get<FlagComponent>(menuEntity))
            flag->flags = EEntityFlag::None;

        if (auto* menu = world.try_get<RuntimeSettingsMenuComponent>(menuEntity))
            menu->wasVisible = false;
    }

    void UISettingsMenuSystem::OpenControlsPanel(Scene& scene, entt::entity menuEntity)
    {
        auto& world = scene.GetRegistry();
        if (!world.valid(menuEntity))
            return;

        auto* menu = world.try_get<RuntimeSettingsMenuComponent>(menuEntity);
        if (!menu)
            return;

        const entt::entity controlsPanel = UISettingsMenuDetail::ResolveWidget(
            scene, menuEntity, menu->controlsPanel, UISettingsMenuDetail::kControlsPanelName);
        if (controlsPanel != entt::null && world.valid(controlsPanel))
        {
            if (auto* flag = world.try_get<FlagComponent>(controlsPanel))
                flag->flags = EEntityFlag::Default;
        }
        menu->controlsVisible = true;
        RefreshKeyBindings(scene, menuEntity);
    }

    void UISettingsMenuSystem::CloseControlsPanel(Scene& scene, entt::entity menuEntity)
    {
        auto& world = scene.GetRegistry();
        if (!world.valid(menuEntity))
            return;

        auto* menu = world.try_get<RuntimeSettingsMenuComponent>(menuEntity);
        if (!menu)
            return;

        const entt::entity controlsPanel = UISettingsMenuDetail::ResolveWidget(
            scene, menuEntity, menu->controlsPanel, UISettingsMenuDetail::kControlsPanelName);
        if (controlsPanel != entt::null && world.valid(controlsPanel))
        {
            if (auto* flag = world.try_get<FlagComponent>(controlsPanel))
                flag->flags = EEntityFlag::None;
        }
        menu->controlsVisible = false;
    }

    void UISettingsMenuSystem::RefreshKeyBindings(Scene& scene, entt::entity menuEntity)
    {
        UISettingsMenuDetail::RefreshKeyBindingsImpl(scene, menuEntity, {}, {});
    }

    void UISettingsMenuSystem::BeginRebind(Scene& scene, entt::entity menuEntity,
        std::string_view mapId, std::string_view actionId)
    {
        m_AwaitingRebindMenuEntity = menuEntity;
        m_AwaitingRebindMapId = std::string(mapId);
        m_AwaitingRebindActionId = std::string(actionId);
        UISettingsMenuDetail::RefreshKeyBindingsImpl(scene, menuEntity, m_AwaitingRebindMapId, m_AwaitingRebindActionId);
    }

    void UISettingsMenuSystem::CancelRebind(Scene* scene)
    {
        const entt::entity prevMenu = m_AwaitingRebindMenuEntity;
        m_AwaitingRebindMenuEntity = entt::null;
        m_AwaitingRebindMapId.clear();
        m_AwaitingRebindActionId.clear();

        if (scene && prevMenu != entt::null && scene->GetRegistry().valid(prevMenu))
            UISettingsMenuDetail::RefreshKeyBindingsImpl(*scene, prevMenu, {}, {});
    }

    void UISettingsMenuSystem::PollRebindInput(Scene& scene)
    {
        auto& world = scene.GetRegistry();
        if (!InputManager::IsCreated() ||
            m_AwaitingRebindMenuEntity == entt::null || !world.valid(m_AwaitingRebindMenuEntity))
        {
            CancelRebind(&scene);
            return;
        }

        auto* menu = world.try_get<RuntimeSettingsMenuComponent>(m_AwaitingRebindMenuEntity);
        if (!menu || !menu->controlsVisible)
        {
            CancelRebind(&scene);
            return;
        }

        auto& input = InputManager::Get();

        // Escape 키로 리매핑 대기 취소
        if (input.IsKeyButtonDown(EKeyCode::Escape))
        {
            CancelRebind(&scene);
            return;
        }

        std::string capturedBinding;

        // 키보드 키 감지
        for (uint32 code = static_cast<uint32>(EKeyCode::A); code < static_cast<uint32>(EKeyCode::Count); ++code)
        {
            const auto key = static_cast<EKeyCode>(code);
            if (key == EKeyCode::Escape)
                continue;

            if (input.IsKeyButtonDown(key))
            {
                if (const char* keyName = EngineKeyCodeToString(key); keyName && keyName[0] != '\0')
                {
                    capturedBinding = keyName;
                    break;
                }
            }
        }

        // 게임패드 버튼 감지
        if (capturedBinding.empty())
        {
            for (uint32 pad = 0; pad < InputManager::MaxGamepads; ++pad)
            {
                if (!input.IsGamepadConnected(pad))
                    continue;

                for (uint32 b = 0; b < static_cast<uint32>(EGamepadButton::Count); ++b)
                {
                    const auto button = static_cast<EGamepadButton>(b);
                    if (input.IsGamepadButtonDown(button, pad))
                    {
                        if (const char* btnName = EngineGamepadButtonToString(button); btnName && btnName[0] != '\0')
                        {
                            capturedBinding = std::string("Gamepad:") + btnName;
                            break;
                        }
                    }
                }
                if (!capturedBinding.empty())
                    break;
            }
        }

        if (capturedBinding.empty())
            return;

        if (input.RebindAction(m_AwaitingRebindMapId, m_AwaitingRebindActionId, capturedBinding))
        {
            if (!menu->bindingsFilePath.empty())
                (void)input.SaveBindingsToFile(std::filesystem::u8path(menu->bindingsFilePath));
        }

        CancelRebind(&scene);
    }

    std::span<const std::pair<std::int32_t, std::int32_t>> UISettingsMenuSystem::GetSupportedResolutions() noexcept
    {
        return UISettingsMenuDetail::kSupportedResolutions;
    }
}
