# --- CMake/ReflectionBuild.cmake ---

add_reflection_bundle(
    TARGET Editor
    MODULE Editor
    SOURCE_ROOT "${EDITOR_SOURCE_DIR}"
    OUTPUT_CPP "${EDITOR_GENERATED_SOURCE}"
    DEPENDS
        "${CMAKE_CURRENT_LIST_FILE}"
        ${EDITOR_HEADERS}
        ${REFLECTION_CODEGEN_SOURCES}
    EXTRA_ARGS
        --strip-namespace Editor
        --strip-namespace Engine
        --namespace-fallback Editor
        --namespace-fallback Engine
        --resource-handle-template ResourceHandle
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)