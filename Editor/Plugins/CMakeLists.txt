if(WIN32)
    function(bam_add_editor_plugin target source_dir catalog)
        add_library(${target} MODULE EXCLUDE_FROM_ALL ${ARGN})
        set_target_properties(${target} PROPERTIES PREFIX "")

        if(MSVC)
            target_link_options(${target} PRIVATE "$<$<CONFIG:Debug>:/PDBALTPATH:%_PDB%>")
        endif()

        add_dependencies(Editor ${target})
        file(RELATIVE_PATH watch_path "${CMAKE_SOURCE_DIR}" "${source_dir}")
        set_property(DIRECTORY APPEND PROPERTY "${catalog}_ENTRIES" "${target}|${watch_path}")
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