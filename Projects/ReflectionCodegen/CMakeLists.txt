project(ReflectionCodegen LANGUAGES NONE)

find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(REFLECTION_CODEGEN_ROOT
    "${CMAKE_CURRENT_LIST_DIR}"
    CACHE INTERNAL "Reflection code generator root"
)

set(REFLECTION_CODEGEN_ENTRY
    "${CMAKE_CURRENT_LIST_DIR}/main.py"
    CACHE INTERNAL "Reflection code generator entry script"
)

set(REFLECTION_CODEGEN_PYTHON
    "${Python3_EXECUTABLE}"
    CACHE INTERNAL "Python executable for reflection code generation"
)

if(NOT TARGET ReflectionCodegen)
    add_custom_target(ReflectionCodegen
        COMMAND "${CMAKE_COMMAND}" -E echo "ReflectionCodegen entry: ${REFLECTION_CODEGEN_ENTRY}"
        COMMAND "${CMAKE_COMMAND}" -E echo "Python executable: ${REFLECTION_CODEGEN_PYTHON}"
        VERBATIM
    )

    set_target_properties(ReflectionCodegen PROPERTIES
        FOLDER "Projects/Reflection"
    )
endif()