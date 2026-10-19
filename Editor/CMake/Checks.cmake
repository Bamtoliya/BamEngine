add_executable(TransformEditorCheck
    examples/TransformEditorCheck.cpp Source/ImGui/InspectorPanel/Private/TransformPropertyEditor.cpp
)

target_include_directories(TransformEditorCheck PRIVATE "${EDITOR_SOURCE_DIR}/ImGui/InspectorPanel/Public")
target_link_libraries(TransformEditorCheck PRIVATE BamEngine::Engine)

add_executable(TransformInheritanceCheck examples/TransformInheritanceCheck.cpp
    Source/ImGui/InspectorPanel/Private/TransformPropertyEditor.cpp
    Source/Command/Private/CommandHistory.cpp Source/Command/Private/SceneEditHistory.cpp)
target_include_directories(TransformInheritanceCheck PRIVATE "${EDITOR_SOURCE_DIR}/Command/Public"
    "${EDITOR_SOURCE_DIR}/ImGui/InspectorPanel/Public")
target_link_libraries(TransformInheritanceCheck PRIVATE BamEngine::Engine)
target_precompile_headers(TransformInheritanceCheck PRIVATE
    "${CMAKE_SOURCE_DIR}/Engine/Source/Core/Public/Engine_Includes.h")
set_target_properties(TransformInheritanceCheck PROPERTIES UNITY_BUILD OFF FOLDER "Checks")

add_executable(EditorHistoryCheck examples/EditorHistoryCheck.cpp
    Source/Command/Private/CommandHistory.cpp Source/Command/Private/SceneEditHistory.cpp)
target_include_directories(EditorHistoryCheck PRIVATE "${EDITOR_SOURCE_DIR}/Command/Public"
    "${EDITOR_SOURCE_DIR}/ImGui/InspectorPanel/Public")
target_link_libraries(EditorHistoryCheck PRIVATE BamEngine::Engine)
target_precompile_headers(EditorHistoryCheck PRIVATE
    "${CMAKE_SOURCE_DIR}/Engine/Source/Core/Public/Engine_Includes.h")
set_target_properties(EditorHistoryCheck PROPERTIES UNITY_BUILD OFF FOLDER "Checks")

add_executable(PickingMathCheck examples/PickingMathCheck.cpp)
target_include_directories(PickingMathCheck PRIVATE "${EDITOR_SOURCE_DIR}/Selection/Public")
target_link_libraries(PickingMathCheck PRIVATE BamEngine::Engine)
set_target_properties(PickingMathCheck PROPERTIES UNITY_BUILD OFF FOLDER "Checks")

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

add_executable(HierarchyEditorCheck examples/HierarchyEditorCheck.cpp
    Source/ImGui/HierarchyPanel/Private/EntityHierarchyView.cpp)
target_include_directories(HierarchyEditorCheck PRIVATE "${EDITOR_SOURCE_DIR}/ImGui/HierarchyPanel/Public")
target_include_directories(HierarchyEditorCheck PRIVATE "${CMAKE_SOURCE_DIR}/ThirdParty")
target_link_libraries(HierarchyEditorCheck PRIVATE BamEngine::Engine imgui::imgui)
target_precompile_headers(HierarchyEditorCheck PRIVATE
    "${CMAKE_SOURCE_DIR}/Engine/Source/Core/Public/Engine_Includes.h")
set_target_properties(HierarchyEditorCheck PROPERTIES UNITY_BUILD OFF FOLDER "Checks")

add_executable(ReflectionInspectorCheck examples/ReflectionInspectorCheck.cpp
    Source/ImGui/InspectorPanel/Private/ReflectionPropertyDrawer.cpp)
target_include_directories(ReflectionInspectorCheck PRIVATE
    "${EDITOR_SOURCE_DIR}/ImGui/InspectorPanel/Public" "${EDITOR_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/ThirdParty")
target_link_libraries(ReflectionInspectorCheck PRIVATE BamEngine::Engine imgui::imgui)
add_dependencies(ReflectionInspectorCheck GenerateLocalization)
target_precompile_headers(ReflectionInspectorCheck PRIVATE
    "${CMAKE_SOURCE_DIR}/Engine/Source/Core/Public/Engine_Includes.h")
set_target_properties(ReflectionInspectorCheck PROPERTIES UNITY_BUILD OFF FOLDER "Checks")
set_target_properties(TransformEditorCheck PROPERTIES UNITY_BUILD OFF FOLDER "Checks")

if(BUILD_TESTING)
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
