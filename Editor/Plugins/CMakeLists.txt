if(WIN32)
    function(bam_add_editor_plugin target)
        add_library(${target} MODULE EXCLUDE_FROM_ALL ${ARGN})
        set_target_properties(${target} PROPERTIES PREFIX "")
        if(MSVC)
            target_link_options(${target} PRIVATE "$<$<CONFIG:Debug>:/PDBALTPATH:%_PDB%>")
        endif()
    endfunction()

    function(bam_add_resource_panel_plugin target source_dir)
        bam_add_editor_plugin(${target} ${ARGN} "${EDITOR_SOURCE_DIR}/ImGui/Private/EditorUI.cpp")
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
        add_dependencies(Editor ${target})
        file(RELATIVE_PATH watch_path "${CMAKE_SOURCE_DIR}" "${source_dir}")
        set_property(DIRECTORY APPEND PROPERTY BAM_RESOURCE_PANEL_PLUGINS "${target}|${watch_path}")
    endfunction()

    bam_add_editor_plugin(EditorPanelLifecycleProbe PanelLifecycleProbe/PanelLifecycleProbe.cpp)
    target_include_directories(EditorPanelLifecycleProbe PRIVATE
       "${EDITOR_SOURCE_DIR}/HotReload/Public"
        "${CMAKE_SOURCE_DIR}/Engine/Source/Core/Module/Public"
    )

    bam_add_resource_panel_plugin(EditorSpriteAnimationTools "${CMAKE_CURRENT_SOURCE_DIR}/SpriteAnimationTools"
        SpriteAnimationTools/Private/SpriteAnimationTools.cpp
    )

    get_property(plugin_entries DIRECTORY PROPERTY BAM_RESOURCE_PANEL_PLUGINS)
    string(JOIN "\n" plugin_catalog ${plugin_entries})
    set(plugin_catalog_dir "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
    if(CMAKE_CONFIGURATION_TYPES)
        string(APPEND plugin_catalog_dir "/$<CONFIG>")
    endif()
    file(GENERATE OUTPUT "${plugin_catalog_dir}/EditorResourcePlugins.txt" CONTENT "${plugin_catalog}\n")
endif()
