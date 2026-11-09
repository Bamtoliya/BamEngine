add_executable(TransformEditorCheck
    examples/TransformEditorCheck.cpp Source/ImGui/InspectorPanel/Private/TransformPropertyEditor.cpp
)

target_include_directories(TransformEditorCheck PRIVATE "${EDITOR_SOURCE_DIR}/ImGui/InspectorPanel/Public")
target_link_libraries(TransformEditorCheck PRIVATE BamEngine::Engine)

add_executable(TransformInheritanceCheck examples/TransformInheritanceCheck.cpp
    Source/ImGui/InspectorPanel/Private/TransformPropertyEditor.cpp
    Source/Command/Private/CommandHistory.cpp Source/Command/Private/SceneEditHistory.cpp)
target_include_directories(TransformInheritanceCheck PRIVATE "${EDITOR_SOURCE_DIR}/Command/Public"
    "${EDITOR_SOURCE_DIR}/ImGui/InspectorPanel/Public" ${EDITOR_PUBLIC_INCLUDE_DIRS}
    "${EDITOR_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/ThirdParty")
target_link_libraries(TransformInheritanceCheck PRIVATE BamEngine::Engine imgui::imgui)
target_precompile_headers(TransformInheritanceCheck PRIVATE
    "${CMAKE_SOURCE_DIR}/Engine/Source/Core/Public/Engine_Includes.h")
set_target_properties(TransformInheritanceCheck PROPERTIES UNITY_BUILD OFF FOLDER "Checks")

add_executable(EditorHistoryCheck examples/EditorHistoryCheck.cpp
    Source/Document/Private/SceneDocument.cpp
    Source/Command/Private/CommandHistory.cpp
    Source/Command/Private/SceneEditHistory.cpp
    Source/Document/Private/EditorDocumentManager.cpp
    Source/Document/Private/EditorDocumentController.cpp)

target_include_directories(EditorHistoryCheck PRIVATE
    ${EDITOR_PUBLIC_INCLUDE_DIRS} "${EDITOR_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/ThirdParty"
    "${EDITOR_SOURCE_DIR}/Command/Public"
    "${EDITOR_SOURCE_DIR}/Document/Public"
    "${EDITOR_SOURCE_DIR}/FileDialog/Public"
    "${EDITOR_SOURCE_DIR}/ImGui/InspectorPanel/Public")
target_link_libraries(EditorHistoryCheck PRIVATE BamEngine::Engine imgui::imgui)
target_precompile_headers(EditorHistoryCheck PRIVATE
    "${CMAKE_SOURCE_DIR}/Engine/Source/Core/Public/Engine_Includes.h")
set_target_properties(EditorHistoryCheck PROPERTIES UNITY_BUILD OFF FOLDER "Checks")

add_executable(PickingMathCheck examples/PickingMathCheck.cpp)
target_include_directories(PickingMathCheck PRIVATE "${EDITOR_SOURCE_DIR}/Selection/Public")
target_link_libraries(PickingMathCheck PRIVATE BamEngine::Engine)
set_target_properties(PickingMathCheck PROPERTIES UNITY_BUILD OFF FOLDER "Checks")

add_executable(PickingBVHBenchmark examples/PickingBVHBenchmark.cpp)
target_link_libraries(PickingBVHBenchmark PRIVATE BamEngine::Engine)
set_target_properties(PickingBVHBenchmark PROPERTIES UNITY_BUILD OFF FOLDER "Benchmarks")

add_executable(CanvasViewCheck examples/CanvasViewCheck.cpp)
target_include_directories(CanvasViewCheck PRIVATE "${EDITOR_SOURCE_DIR}/ImGui/ViewportPanel/Public")
target_link_libraries(CanvasViewCheck PRIVATE BamEngine::Engine)
set_target_properties(CanvasViewCheck PROPERTIES UNITY_BUILD OFF FOLDER "Checks")

add_executable(GraphPaletteCheck examples/GraphPaletteCheck.cpp)
target_include_directories(GraphPaletteCheck PRIVATE "${EDITOR_SOURCE_DIR}/ImGui/GraphPanel/Public")
target_link_libraries(GraphPaletteCheck PRIVATE BamEngine::Engine)
set_target_properties(GraphPaletteCheck PROPERTIES UNITY_BUILD OFF FOLDER "Checks")

add_executable(GraphMessageLocalizationCheck examples/GraphMessageLocalizationCheck.cpp)
target_include_directories(GraphMessageLocalizationCheck PRIVATE "${EDITOR_SOURCE_DIR}/ImGui/GraphPanel/Public")
target_link_libraries(GraphMessageLocalizationCheck PRIVATE BamEngine::Engine)
add_dependencies(GraphMessageLocalizationCheck GenerateLocalization)
set_target_properties(GraphMessageLocalizationCheck PROPERTIES UNITY_BUILD OFF FOLDER "Checks")

add_executable(ShortcutManagerCheck
    examples/ShortcutManagerCheck.cpp
    Source/Shortcut/Private/ShortcutManager.cpp
)
target_include_directories(ShortcutManagerCheck PRIVATE ${EDITOR_PUBLIC_INCLUDE_DIRS}
    "${EDITOR_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/ThirdParty")
target_link_libraries(ShortcutManagerCheck PRIVATE BamEngine::Engine imgui::imgui)
target_precompile_headers(ShortcutManagerCheck PRIVATE
    "${CMAKE_SOURCE_DIR}/Engine/Source/Core/Public/Engine_Includes.h")
set_target_properties(ShortcutManagerCheck PROPERTIES UNITY_BUILD OFF FOLDER "Checks")

add_executable(MainMenuBarLayoutCheck examples/MainMenuBarLayoutCheck.cpp)
target_include_directories(MainMenuBarLayoutCheck PRIVATE "${EDITOR_SOURCE_DIR}/ImGui/ToolBar/Public" "${EDITOR_SOURCE_DIR}")
target_link_libraries(MainMenuBarLayoutCheck PRIVATE BamEngine::Engine imgui::imgui)
set_target_properties(MainMenuBarLayoutCheck PROPERTIES UNITY_BUILD OFF FOLDER "Checks")

add_executable(HierarchyEditorCheck examples/HierarchyEditorCheck.cpp
    Source/ImGui/HierarchyPanel/Private/EntityHierarchyView.cpp
    Source/Shortcut/Private/ShortcutManager.cpp
    Source/Selection/Private/SelectionManager.cpp
)
target_include_directories(HierarchyEditorCheck PRIVATE
    ${EDITOR_PUBLIC_INCLUDE_DIRS}
    "${EDITOR_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/ThirdParty"
)
target_link_libraries(HierarchyEditorCheck PRIVATE BamEngine::Engine imgui::imgui)
target_precompile_headers(HierarchyEditorCheck PRIVATE
    "${CMAKE_SOURCE_DIR}/Engine/Source/Core/Public/Engine_Includes.h")
set_target_properties(HierarchyEditorCheck PROPERTIES UNITY_BUILD OFF FOLDER "Checks")
add_executable(ReflectionInspectorCheck examples/ReflectionInspectorCheck.cpp
    Source/ImGui/InspectorPanel/Private/ReflectionPropertyDrawer.cpp
    Source/ImGui/Private/EditorUI.cpp)
target_include_directories(ReflectionInspectorCheck PRIVATE
    ${EDITOR_PUBLIC_INCLUDE_DIRS}
    "${EDITOR_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/ThirdParty"
)
target_link_libraries(ReflectionInspectorCheck PRIVATE BamEngine::Engine imgui::imgui)
add_dependencies(ReflectionInspectorCheck GenerateLocalization)
target_precompile_headers(ReflectionInspectorCheck PRIVATE
    "${CMAKE_SOURCE_DIR}/Engine/Source/Core/Public/Engine_Includes.h")
set_target_properties(ReflectionInspectorCheck PROPERTIES UNITY_BUILD OFF FOLDER "Checks")
set_target_properties(TransformEditorCheck PROPERTIES UNITY_BUILD OFF FOLDER "Checks")

if(BUILD_TESTING)
    add_test(NAME MainMenuBarLayoutCheck COMMAND MainMenuBarLayoutCheck)
    set_tests_properties(MainMenuBarLayoutCheck PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    add_test(NAME ShortcutManagerCheck COMMAND ShortcutManagerCheck)
    add_test(NAME GraphMessageLocalizationCheck COMMAND GraphMessageLocalizationCheck)
    set_tests_properties(GraphMessageLocalizationCheck PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    add_test(NAME TransformInheritanceCheck COMMAND TransformInheritanceCheck)
    add_test(NAME GraphPaletteCheck COMMAND GraphPaletteCheck)
    add_test(NAME EditorHistoryCheck COMMAND EditorHistoryCheck)
    add_test(NAME CanvasViewCheck COMMAND CanvasViewCheck)
    add_test(NAME HierarchyEditorCheck COMMAND HierarchyEditorCheck)
    add_test(NAME PickingMathCheck COMMAND PickingMathCheck)
    add_test(NAME TransformEditorCheck COMMAND TransformEditorCheck)
    add_test(NAME ReflectionInspectorCheck COMMAND ReflectionInspectorCheck)
    set_tests_properties(ReflectionInspectorCheck PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endif()

if(MSVC)
    target_compile_options(MainMenuBarLayoutCheck PRIVATE /utf-8)
    target_compile_options(ShortcutManagerCheck PRIVATE /utf-8)
    target_compile_options(GraphMessageLocalizationCheck PRIVATE /utf-8)
    target_compile_options(TransformInheritanceCheck PRIVATE /utf-8)
    target_compile_options(GraphPaletteCheck PRIVATE /utf-8)
    target_compile_options(EditorHistoryCheck PRIVATE /utf-8)
    target_compile_options(CanvasViewCheck PRIVATE /utf-8)
    target_compile_options(HierarchyEditorCheck PRIVATE /utf-8)
    target_compile_options(PickingMathCheck PRIVATE /utf-8)
    target_compile_options(TransformEditorCheck PRIVATE /utf-8)
    target_compile_options(ReflectionInspectorCheck PRIVATE /utf-8)
endif()

add_executable(EditorSettingsCheck examples/EditorSettingsCheck.cpp)

target_include_directories(EditorSettingsCheck PRIVATE
    "${EDITOR_SOURCE_DIR}"
    "${EDITOR_SOURCE_DIR}/Settings/Public"
)

target_link_libraries(EditorSettingsCheck PRIVATE BamEngine::Engine)
set_target_properties(EditorSettingsCheck PROPERTIES UNITY_BUILD OFF FOLDER "Checks")

reflection_generate(
    TARGET EditorSettingsCheck
    MODULE BamEditorSettingsCheck
    CXX_STANDARD 23
    HEADERS "${EDITOR_SOURCE_DIR}/Settings/Public/EditorSettings.h"
    HEADER_INCLUDE_ROOT "${EDITOR_SOURCE_DIR}"
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Generated/SettingsCheck/$<CONFIG>"
    VISIBILITY PRIVATE
)

if(MSVC)
    target_compile_options(EditorSettingsCheck PRIVATE /utf-8 /Zc:preprocessor)
endif()

if(BUILD_TESTING)
    add_test(NAME EditorSettingsCheck COMMAND EditorSettingsCheck
        "${CMAKE_CURRENT_BINARY_DIR}/editor-settings-check")
endif()


add_executable(SpriteSlicingCheck
    examples/SpriteSlicingCheck.cpp
    Source/AssetManager/Private/SpriteSlicingService.cpp
    Source/AssetManager/Private/StbImageImpl.cpp
)
target_include_directories(SpriteSlicingCheck PRIVATE
    "${EDITOR_SOURCE_DIR}/AssetManager/Public"
    "${EDITOR_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/ThirdParty"
)
target_link_libraries(SpriteSlicingCheck PRIVATE BamEngine::Engine)
target_precompile_headers(SpriteSlicingCheck PRIVATE
    "${CMAKE_SOURCE_DIR}/Engine/Source/Core/Public/Engine_Includes.h")
set_target_properties(SpriteSlicingCheck PROPERTIES UNITY_BUILD OFF FOLDER "Checks")
if(MSVC)
    target_compile_options(SpriteSlicingCheck PRIVATE /utf-8)
endif()

if(BUILD_TESTING)
    add_test(NAME SpriteSlicingCheck COMMAND SpriteSlicingCheck)
    set_tests_properties(SpriteSlicingCheck PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endif()
