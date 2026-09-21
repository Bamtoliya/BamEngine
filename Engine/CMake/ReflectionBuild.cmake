# --- CMake/ReflectionBuild.cmake ---

add_custom_command(
    OUTPUT "${GENERATED_SOURCE}"
    COMMAND python "${CMAKE_SOURCE_DIR}/Projects/ReflectionAST/main.py" --source-root "${ENGINE_SOURCE_DIR}" --output "${GENERATED_SOURCE}"
    DEPENDS ${ENGINE_HEADERS} ${REFLECTION_AST_SOURCES}
    COMMENT "Generating Reflection Data via AST..."
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/Projects/ReflectionAST"
)

# [수정] 5. 쉐이더 컴파일과 더불어 리플렉션 코드 생성이 먼저 되도록 종속성 추가
add_custom_target(GenerateReflectionAST DEPENDS "${GENERATED_SOURCE}")