include_guard(GLOBAL)

function(add_reflection_bundle)
    set(options)
    set(oneValueArgs
        TARGET
        MODULE
        SOURCE_ROOT
        OUTPUT_CPP
        WORKING_DIRECTORY
        TOOL_ENTRY
        PYTHON_EXECUTABLE
    )
    set(multiValueArgs
        DEPENDS
        EXTRA_ARGS
    )

    cmake_parse_arguments(ARB "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT ARB_TARGET)
        message(FATAL_ERROR "add_reflection_bundle: TARGET is required")
    endif()

    if(NOT ARB_MODULE)
        message(FATAL_ERROR "add_reflection_bundle: MODULE is required")
    endif()

    if(NOT ARB_SOURCE_ROOT)
        message(FATAL_ERROR "add_reflection_bundle: SOURCE_ROOT is required")
    endif()

    if(NOT ARB_OUTPUT_CPP)
        message(FATAL_ERROR "add_reflection_bundle: OUTPUT_CPP is required")
    endif()

    if(NOT TARGET "${ARB_TARGET}")
        message(FATAL_ERROR "add_reflection_bundle: target '${ARB_TARGET}' does not exist")
    endif()

    if(NOT ARB_TOOL_ENTRY)
        set(ARB_TOOL_ENTRY "${CMAKE_SOURCE_DIR}/Projects/ReflectionAST/main.py")
    endif()

    if(NOT ARB_PYTHON_EXECUTABLE)
        if(DEFINED REFLECTION_CODEGEN_PYTHON)
            set(ARB_PYTHON_EXECUTABLE "${REFLECTION_CODEGEN_PYTHON}")
        else()
            find_package(Python3 COMPONENTS Interpreter REQUIRED)
            set(ARB_PYTHON_EXECUTABLE "${Python3_EXECUTABLE}")
        endif()
    endif()

    if(NOT ARB_WORKING_DIRECTORY)
        set(ARB_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    endif()

    get_filename_component(_reflection_output_dir "${ARB_OUTPUT_CPP}" DIRECTORY)
    file(MAKE_DIRECTORY "${_reflection_output_dir}")

    string(MAKE_C_IDENTIFIER "${ARB_TARGET}_${ARB_MODULE}_Reflection" _bundle_target_name)

    set(_reflection_include_file "${CMAKE_CURRENT_BINARY_DIR}/${_bundle_target_name}_includes_$<CONFIG>.txt")

    file(GENERATE
        OUTPUT "${_reflection_include_file}"
        CONTENT "$<JOIN:$<TARGET_PROPERTY:${ARB_TARGET},INCLUDE_DIRECTORIES>,\n>\n"
    )

    add_custom_command(
        OUTPUT "${ARB_OUTPUT_CPP}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${_reflection_output_dir}"
        COMMAND "${ARB_PYTHON_EXECUTABLE}" "${ARB_TOOL_ENTRY}"
            --module "${ARB_MODULE}"
            --source-root "${ARB_SOURCE_ROOT}"
            --output "${ARB_OUTPUT_CPP}"
            --include-dirs-file "${_reflection_include_file}"
            ${ARB_EXTRA_ARGS}
        WORKING_DIRECTORY "${ARB_WORKING_DIRECTORY}"
        DEPENDS
            "${ARB_TOOL_ENTRY}"
            "${_reflection_include_file}"
            ${ARB_DEPENDS}
        COMMENT "Generating reflection bundle: ${ARB_MODULE} -> ${ARB_OUTPUT_CPP}"
        VERBATIM
    )

    add_custom_target("${_bundle_target_name}"
        DEPENDS "${ARB_OUTPUT_CPP}"
    )

    set_target_properties("${_bundle_target_name}" PROPERTIES
        FOLDER "Projects/Reflection"
    )

    add_dependencies("${ARB_TARGET}" "${_bundle_target_name}")

    target_sources("${ARB_TARGET}" PRIVATE "${ARB_OUTPUT_CPP}")
    set_source_files_properties("${ARB_OUTPUT_CPP}" PROPERTIES GENERATED TRUE)
endfunction()