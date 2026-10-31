reflection_generate(
    TARGET Editor
    MODULE BamEditorSettings
    CXX_STANDARD 23
    HEADERS "${EDITOR_SOURCE_DIR}/Settings/Public/EditorSettings.h"
    HEADER_INCLUDE_ROOT "${EDITOR_SOURCE_DIR}"
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Generated/Settings/$<CONFIG>"
    VISIBILITY PRIVATE
)