add_executable(TransformEditorCheck
    examples/TransformEditorCheck.cpp Source/ImGui/InspectorPanel/Private/TransformPropertyEditor.cpp
)

target_include_directories(TransformEditorCheck PRIVATE "${EDITOR_SOURCE_DIR}/ImGui/InspectorPanel/Public")
target_link_libraries(TransformEditorCheck PRIVATE BamEngine::Engine)
set_target_properties(TransformEditorCheck PROPERTIES UNITY_BUILD OFF FOLDER "Checks")

if(MSVC)
    target_compile_options(TransformEditorCheck PRIVATE /utf-8)
endif()