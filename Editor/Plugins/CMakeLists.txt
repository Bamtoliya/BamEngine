if(WIN32)
    function(bam_add_editor_plugin target source_dir catalog)
        set(plugin_sources ${ARGN})
        set(auto_load FALSE)
        if(plugin_sources)
            list(GET plugin_sources 0 first_plugin_source)
            if(first_plugin_source STREQUAL "AUTOLOAD")
                set(auto_load TRUE)
                list(REMOVE_AT plugin_sources 0)
            endif()
        endif()

        add_library(${target} MODULE EXCLUDE_FROM_ALL ${plugin_sources})
        set_target_properties(${target} PROPERTIES PREFIX "")

        if(MSVC)
            target_link_options(${target} PRIVATE "$<$<CONFIG:Debug>:/PDBALTPATH:%_PDB%>")
        endif()

        add_dependencies(Editor ${target})
        file(RELATIVE_PATH watch_path "${CMAKE_SOURCE_DIR}" "${source_dir}")
        set(catalog_entry "${target}|${watch_path}")
        if(auto_load)
            string(APPEND catalog_entry "|auto")
        endif()
        set_property(DIRECTORY APPEND PROPERTY "${catalog}_ENTRIES" "${catalog_entry}")
    endfunction()

    function(bam_add_editor_panel_plugin target source_dir)
        bam_add_editor_plugin(${target} "${source_dir}" Plugins AUTOLOAD ${ARGN})
        target_include_directories(${target} PRIVATE
            "${EDITOR_SOURCE_DIR}"
            "${EDITOR_SOURCE_DIR}/HotReload/Public"
            "${EDITOR_SOURCE_DIR}/ImGui/Public"
            "${source_dir}/Public"
            "${CMAKE_SOURCE_DIR}/ThirdParty"
        )
        target_link_libraries(${target} PRIVATE BamEngine::Engine imgui::imgui)

        if(MSVC)
            target_compile_options(${target} PRIVATE /Zc:preprocessor /utf-8 /bigobj)
        endif()
    endfunction()

    function(bam_add_resource_panel_plugin target source_dir)
        bam_add_editor_plugin(${target} "${source_dir}" EditorResourcePlugins
            ${ARGN}
            "${EDITOR_SOURCE_DIR}/ImGui/Private/EditorUI.cpp"
        )
        target_include_directories(${target} PRIVATE
            "${EDITOR_SOURCE_DIR}"
            "${EDITOR_SOURCE_DIR}/HotReload/Public"
            "${EDITOR_SOURCE_DIR}/ImGui/Public"
            "${CMAKE_SOURCE_DIR}/ThirdParty"
        )
        target_link_libraries(${target} PRIVATE BamEngine::Engine imgui::imgui)

        if(MSVC)
            target_compile_options(${target} PRIVATE /Zc:preprocessor /utf-8 /bigobj)
        endif()
    endfunction()

    bam_add_editor_plugin(EditorPanelLifecycleProbe "${CMAKE_CURRENT_SOURCE_DIR}/PanelLifecycleProbe" Plugins
        PanelLifecycleProbe/PanelLifecycleProbe.cpp
    )
    target_include_directories(EditorPanelLifecycleProbe PRIVATE
        "${EDITOR_SOURCE_DIR}/HotReload/Public"
        "${CMAKE_SOURCE_DIR}/Engine/Source/Core/Module/Public"
    )

    bam_add_resource_panel_plugin(EditorSpriteAnimationTools "${CMAKE_CURRENT_SOURCE_DIR}/SpriteAnimationTools"
        SpriteAnimationTools/Private/SpriteAnimationTools.cpp
    )
    target_sources(EditorSpriteAnimationTools PRIVATE "${EDITOR_SOURCE_DIR}/Command/Private/CommandHistory.cpp")
    target_include_directories(EditorSpriteAnimationTools PRIVATE "${EDITOR_SOURCE_DIR}/Command/Public")

    bam_add_editor_panel_plugin(EditorConsolePanel "${EDITOR_SOURCE_DIR}/ImGui/ConsolePanel"
        "${EDITOR_SOURCE_DIR}/ImGui/ConsolePanel/Private/ConsolePanel.cpp"
        "${EDITOR_SOURCE_DIR}/ImGui/ConsolePanel/Private/ConsolePanelPlugin.cpp"
        "${EDITOR_SOURCE_DIR}/ImGui/Private/EditorUI.cpp"
    )
    bam_add_editor_panel_plugin(EditorMetricsPanel "${EDITOR_SOURCE_DIR}/ImGui/MetricsPanel"
        "${EDITOR_SOURCE_DIR}/ImGui/MetricsPanel/Private/MetricsPanel.cpp"
        "${EDITOR_SOURCE_DIR}/ImGui/MetricsPanel/Private/FrameMetricsWidget.cpp"
        "${EDITOR_SOURCE_DIR}/ImGui/MetricsPanel/Private/RenderMetricsWidget.cpp"
        "${EDITOR_SOURCE_DIR}/ImGui/MetricsPanel/Private/ResourceMetricsWidget.cpp"
        "${EDITOR_SOURCE_DIR}/ImGui/MetricsPanel/Private/EngineTaskMetricsWidget.cpp"
        "${EDITOR_SOURCE_DIR}/ImGui/MetricsPanel/Private/NetworkMetricsWidget.cpp"
        "${EDITOR_SOURCE_DIR}/ImGui/MetricsPanel/Private/MetricsPanelPlugin.cpp"
    )
    bam_add_editor_panel_plugin(EditorNetworkProfilerPanel "${EDITOR_SOURCE_DIR}/ImGui/NetworkProfilerPanel"
        "${EDITOR_SOURCE_DIR}/ImGui/NetworkProfilerPanel/Private/NetworkProfilerPanel.cpp"
        "${EDITOR_SOURCE_DIR}/ImGui/NetworkProfilerPanel/Private/NetworkProfilerPanelPlugin.cpp"
    )

    set(plugin_catalog_dir "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
    if(CMAKE_CONFIGURATION_TYPES)
        string(APPEND plugin_catalog_dir "/$<CONFIG>")
    endif()

    foreach(catalog IN ITEMS Plugins EditorResourcePlugins)
        get_property(plugin_entries DIRECTORY PROPERTY "${catalog}_ENTRIES")
        string(JOIN "\n" plugin_catalog ${plugin_entries})
        file(GENERATE OUTPUT "${plugin_catalog_dir}/${catalog}.txt" CONTENT "${plugin_catalog}\n")
    endforeach()
endif()