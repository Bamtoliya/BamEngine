#pragma once

#include "ComponentAnnotations.h"
#include "RuntimeSettings.h"
#include "SceneEntityReference.h"
#include <entt/entt.hpp>
#include <string>
#include <vector>

namespace Engine
{
    STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_UI_SETTINGS_MENU"), CATEGORY("CATEGORY_UI"))
    struct RuntimeSettingsMenuComponent
    {
        REFLECT_BODY()

        PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_UI_SETTINGS_AUTO_SYNC"))
        bool autoSyncOnEnable = true;

        PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_UI_SETTINGS_SAVE_ON_APPLY"))
        bool saveOnApply = true;

        PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_UI_SETTINGS_FILE_PATH"))
        std::string settingsFilePath = "Config/RuntimeSettings.json";

        PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_UI_SETTINGS_BINDINGS_FILE_PATH"))
        std::string bindingsFilePath = "Config/InputBindings.json";

        PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_UI_SETTINGS_RESOLUTION_DROPDOWN"))
        SceneEntityReference resolutionDropdown;

        PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_UI_SETTINGS_QUALITY_DROPDOWN"))
        SceneEntityReference qualityDropdown;

        PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_UI_SETTINGS_FULLSCREEN_TOGGLE"))
        SceneEntityReference fullscreenToggle;

        PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_UI_SETTINGS_VSYNC_TOGGLE"))
        SceneEntityReference vsyncToggle;

        PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_UI_SETTINGS_MASTER_SLIDER"))
        SceneEntityReference masterVolumeSlider;

        PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_UI_SETTINGS_BGM_SLIDER"))
        SceneEntityReference bgmVolumeSlider;

        PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_UI_SETTINGS_SFX_SLIDER"))
        SceneEntityReference sfxVolumeSlider;

        PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_UI_SETTINGS_UI_SLIDER"))
        SceneEntityReference uiVolumeSlider;

        PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_UI_SETTINGS_VOICE_SLIDER"))
        SceneEntityReference voiceVolumeSlider;

        PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_UI_SETTINGS_APPLY_BUTTON"))
        SceneEntityReference applyButton;

        PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_UI_SETTINGS_RESET_BUTTON"))
        SceneEntityReference resetButton;

        PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_UI_SETTINGS_CLOSE_BUTTON"))
        SceneEntityReference closeButton;

        PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_UI_SETTINGS_CONTROLS_BUTTON"))
        SceneEntityReference controlsButton;

        PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_UI_SETTINGS_CONTROLS_PANEL"))
        SceneEntityReference controlsPanel;

        PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_UI_SETTINGS_CONTROLS_LIST"))
        SceneEntityReference controlsListContainer;

        PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_UI_SETTINGS_CONTROLS_RESET_BUTTON"))
        SceneEntityReference controlsResetButton;

        PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_UI_SETTINGS_CONTROLS_BACK_BUTTON"))
        SceneEntityReference controlsBackButton;

        struct KeyBindingRowEntry
        {
            std::string mapId;
            std::string actionId;
            entt::entity labelEntity = entt::null;
            entt::entity buttonEntity = entt::null;
        };

        // 런타임 전용 상태 (직렬화 제외)
        RuntimeSettings pendingSettings{};
        std::vector<KeyBindingRowEntry> keyBindingRows;
        bool hasPendingChanges = false;
        bool initialized = false;
        bool wasVisible = false;
        bool controlsVisible = false;
    };
}
