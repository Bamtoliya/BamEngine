include_guard(GLOBAL)

function(reflection_generate)
    cmake_parse_arguments(PARSE_ARGV 0 arg
        ""
        "TARGET;MODULE;GENERATOR;RESOURCE_DIR;VISIBILITY"
        "HEADERS;INCLUDE_DIRECTORIES;COMPILE_OPTIONS"
    )

    if(arg_UNPARSED_ARGUMENTS OR arg_KEYWORDS_MISSING_VALUES)
        message(FATAL_ERROR "Invalid arguments to reflection_generate()")
    endif()

    if(NOT DEFINED arg_VISIBILITY)
        set(arg_VISIBILITY PRIVATE)
    endif()

    if(NOT arg_VISIBILITY STREQUAL "PRIVATE" AND NOT arg_VISIBILITY STREQUAL "PUBLIC")
        message(FATAL_ERROR "VISIBILITY must be PRIVATE or PUBLIC")
    endif()

    if(NOT arg_TARGET OR NOT TARGET "${arg_TARGET}")
        message(FATAL_ERROR "TARGET must name an existing build target")
    endif()

    get_target_property(_target_type "${arg_TARGET}" TYPE)
    get_target_property(_target_imported "${arg_TARGET}" IMPORTED)
    get_target_property(_target_alias "${arg_TARGET}" ALIASED_TARGET)

    if(_target_imported OR _target_alias OR
       NOT _target_type MATCHES "^(EXECUTABLE|STATIC_LIBRARY|SHARED_LIBRARY|MODULE_LIBRARY|OBJECT_LIBRARY)$")
        message(FATAL_ERROR "TARGET must be a local executable or compiled library")
    endif()

    if(NOT arg_MODULE MATCHES "^[A-Za-z][A-Za-z0-9_]*$" OR arg_MODULE MATCHES "__")
        message(FATAL_ERROR "Invalid reflection module name: ${arg_MODULE}")
    endif()

    if(NOT arg_HEADERS)
        message(FATAL_ERROR "HEADERS must contain at least one reflection header")
    endif()

    if(NOT TARGET Reflection::Runtime)
        message(FATAL_ERROR "Call find_package(Reflection CONFIG REQUIRED) or add_subdirectory() before reflection_generate()")
    endif()

    get_target_property(_registered_modules "${arg_TARGET}" REFLECTION_MODULES)
    if(arg_MODULE IN_LIST _registered_modules)
        message(FATAL_ERROR "Module ${arg_MODULE} is already attached to ${arg_TARGET}")
    endif()

    if(arg_GENERATOR)
        set(_generator "${arg_GENERATOR}")
    elseif(TARGET ReflectionGenerator)
        set(_generator ReflectionGenerator)
    else()
        set(_generator "${REFLECTION_GENERATOR_EXECUTABLE}")
    endif()

    if(NOT _generator)
        message(FATAL_ERROR "Build ReflectionGenerator or provide GENERATOR")
    endif()

    if(TARGET "${_generator}")
        get_target_property(_generator_type "${_generator}" TYPE)
        if(NOT _generator_type STREQUAL "EXECUTABLE")
            message(FATAL_ERROR "GENERATOR target must be an executable")
        endif()

        set(_generator_command "$<TARGET_FILE:${_generator}>")
        set(_generator_dependency "${_generator}")
    else()
        get_filename_component(_generator_path "${_generator}" ABSOLUTE
            BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

        if(NOT EXISTS "${_generator_path}" OR IS_DIRECTORY "${_generator_path}")
            message(FATAL_ERROR "Generator executable does not exist: ${_generator_path}")
        endif()

        set(_generator_command "${_generator_path}")
        set(_generator_dependency "${_generator_path}")
    endif()

    if(arg_RESOURCE_DIR)
        set(_resource_dir "${arg_RESOURCE_DIR}")
    else()
        set(_resource_dir "${REFLECTION_CLANG_RESOURCE_DIR}")
    endif()

    if(NOT IS_ABSOLUTE "${_resource_dir}" OR
       NOT EXISTS "${_resource_dir}/include/stddef.h")
        message(FATAL_ERROR "RESOURCE_DIR must identify a Clang resource directory")
    endif()

    set(_runtime_includes
        "$<TARGET_PROPERTY:Reflection::Runtime,INTERFACE_INCLUDE_DIRECTORIES>"
    )

    set(_header_paths)
    foreach(_header IN LISTS arg_HEADERS)
        get_filename_component(_absolute_header "${_header}" ABSOLUTE
            BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

        if(NOT EXISTS "${_absolute_header}" OR IS_DIRECTORY "${_absolute_header}")
            message(FATAL_ERROR "Reflection header does not exist: ${_absolute_header}")
        endif()

        if(_absolute_header MATCHES "[\"\r\n]")
            message(FATAL_ERROR "Unsupported header path: ${_absolute_header}")
        endif()

        list(APPEND _header_paths "${_absolute_header}")
    endforeach()
    list(REMOVE_DUPLICATES _header_paths)

    set(_header_arguments)
    set(_input_content "")
    foreach(_header IN LISTS _header_paths)
        list(APPEND _header_arguments --header "${_header}")
        string(APPEND _input_content "#include \"${_header}\"\n")
    endforeach()

    set(_include_arguments
        "$<$<BOOL:${_runtime_includes}>:-I$<JOIN:${_runtime_includes},;-I>>"
    )
    foreach(_include IN LISTS arg_INCLUDE_DIRECTORIES)
        get_filename_component(_absolute_include "${_include}" ABSOLUTE
            BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        list(APPEND _include_arguments "-I${_absolute_include}")
    endforeach()

    set(_base_directory
        "${CMAKE_CURRENT_BINARY_DIR}/reflection/${arg_TARGET}/${arg_MODULE}")
    set(_generated_directory "${_base_directory}/$<CONFIG>")
    set(_input_cpp "${_base_directory}/ReflectionInput.cpp")
    set(_generated_cpp "${_generated_directory}/${arg_MODULE}.gen.cpp")
    set(_generated_header "${_generated_directory}/${arg_MODULE}.gen.h")
    set(_depfile "${_generated_directory}/${arg_MODULE}.gen.d")

    file(GENERATE OUTPUT "${_input_cpp}" CONTENT "${_input_content}")

    add_custom_command(
        OUTPUT "${_generated_cpp}" "${_generated_header}"

        COMMAND "${CMAKE_COMMAND}" -E make_directory "${_generated_directory}"

        COMMAND "${_generator_command}"
            --module "${arg_MODULE}"
            ${_header_arguments}
            --output "${_generated_cpp}"
            --depfile "${_depfile}"
            "${_input_cpp}"
            --
            -std=c++20
            "${_include_arguments}"
            "-resource-dir=${_resource_dir}"
            ${arg_COMPILE_OPTIONS}

        DEPENDS
            ${_generator_dependency}
            "${_input_cpp}"
            ${_header_paths}

        DEPFILE "${_depfile}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Generating ${arg_MODULE} reflection registration"
        COMMAND_EXPAND_LISTS
        VERBATIM
    )

    target_sources("${arg_TARGET}" PRIVATE "${_generated_cpp}" "${_generated_header}")
     target_include_directories("${arg_TARGET}" ${arg_VISIBILITY}
        "$<BUILD_INTERFACE:${_generated_directory}>")
    target_link_libraries("${arg_TARGET}" ${arg_VISIBILITY} Reflection::Runtime)
    set_property(TARGET "${arg_TARGET}" APPEND PROPERTY
        REFLECTION_MODULES "${arg_MODULE}")
endfunction()