# Engine/CMake/BamEngineConfig.cmake
# External C++ GameModule consumer interface.

if(NOT DEFINED BAMENGINE_ROOT OR BAMENGINE_ROOT STREQUAL "")
    get_filename_component(BAMENGINE_ROOT
        "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
endif()

file(TO_CMAKE_PATH "${BAMENGINE_ROOT}" BAMENGINE_ROOT)

if(NOT EXISTS "${BAMENGINE_ROOT}/Engine/Source")
    message(FATAL_ERROR
        "BamEngineConfig: Invalid BAMENGINE_ROOT: ${BAMENGINE_ROOT}")
endif()

# ------------------------------------------------------------
# Reflection Clang resource directory
# ------------------------------------------------------------

set(BAMENGINE_CLANG_RESOURCE_DIR "")

file(GLOB _clang_res_candidates LIST_DIRECTORIES true
    "${BAMENGINE_ROOT}/Projects/Reflection/dependencies/llvm-*/lib/clang/*"
)

foreach(_res_cand IN LISTS _clang_res_candidates)
    if(IS_DIRECTORY "${_res_cand}/include")
        set(BAMENGINE_CLANG_RESOURCE_DIR "${_res_cand}")
        break()
    endif()
endforeach()

# ------------------------------------------------------------
# GameModule output helper
# ------------------------------------------------------------

function(bamengine_configure_game_module TARGET_NAME PROJECT_ROOT)
    get_target_property(_imported BamEngine::Engine IMPORTED)

    if(_imported)
        set(_with_editor "${_bam_engine_editor}")
        set(_shipping "${_bam_engine_shipping}")
    else()
        set(_with_editor "${BAMENGINE_WITH_EDITOR}")
        set(_shipping "${BAMENGINE_SHIPPING}")
    endif()

    if(_shipping)
        set(_module_role "Shipping")
    elseif(_with_editor)
        set(_module_role "Editor")
    else()
        set(_module_role "Client")
    endif()

    get_filename_component(_project_root "${PROJECT_ROOT}" ABSOLUTE)

    set(_module_output
        "${_project_root}/Binaries/Win64/${_module_role}/$<CONFIG>")

    set_target_properties(${TARGET_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${_module_output}"
        LIBRARY_OUTPUT_DIRECTORY "${_module_output}"
        ARCHIVE_OUTPUT_DIRECTORY "${_module_output}"
        PDB_OUTPUT_DIRECTORY "${_module_output}"
    )

    if(MSVC)
        set_target_properties(${TARGET_NAME} PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
        )
    endif()

    target_link_libraries(${TARGET_NAME} PRIVATE BamEngine::Engine)

    message(STATUS "[BamEngine SDK] Module: ${TARGET_NAME}")
    message(STATUS "[BamEngine SDK] Module role: ${_module_role}")
    message(STATUS "[BamEngine SDK] Module output: ${_module_output}")
endfunction()

# In-tree consumers already have the real Engine target.
if(TARGET BamEngine::Engine)
    set(BamEngine_FOUND TRUE)
    return()
endif()

# ------------------------------------------------------------
# External SDK platform validation
# ------------------------------------------------------------

if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR
        "BamEngineConfig: External SDK currently requires Windows x64 MSVC.")
endif()

# ------------------------------------------------------------
# Resolve requested consumer configuration
# ------------------------------------------------------------

get_property(_bam_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)

if(_bam_multi_config)
    list(LENGTH CMAKE_CONFIGURATION_TYPES _bam_config_count)

    if(NOT _bam_config_count EQUAL 1)
        message(FATAL_ERROR
            "BamEngineConfig: Select one configuration using "
            "-DCMAKE_CONFIGURATION_TYPES=Debug or Release.")
    endif()

    list(GET CMAKE_CONFIGURATION_TYPES 0 _bam_requested_config)
else()
    set(_bam_requested_config "${CMAKE_BUILD_TYPE}")
endif()

string(TOUPPER "${_bam_requested_config}" _bam_config_upper)

if(_bam_config_upper STREQUAL "DEBUG")
    set(_bam_config "Debug")
elseif(_bam_config_upper STREQUAL "RELEASE")
    set(_bam_config "Release")
else()
    message(FATAL_ERROR
        "BamEngineConfig: Unsupported configuration: ${_bam_requested_config}. "
        "Select Debug or Release explicitly.")
endif()

string(TOLOWER "${_bam_config}" _bam_config_lower)

# ------------------------------------------------------------
# Validate an engine build directory
# ------------------------------------------------------------

function(_bam_validate_build BUILD_DIR CONFIG OUT_VALID)
    set(${OUT_VALID} FALSE PARENT_SCOPE)

    if(NOT EXISTS "${BUILD_DIR}/CMakeCache.txt")
        return()
    endif()

    load_cache("${BUILD_DIR}"
        READ_WITH_PREFIX _sdk_
        CMAKE_HOME_DIRECTORY
        CMAKE_BUILD_TYPE
        CMAKE_CONFIGURATION_TYPES
        CMAKE_GENERATOR
        BAMENGINE_WITH_EDITOR
        BAMENGINE_SHIPPING
    )

    # Prevent using a build tree belonging to another repository.
    if(NOT DEFINED _sdk_CMAKE_HOME_DIRECTORY)
        return()
    endif()

    file(REAL_PATH "${_sdk_CMAKE_HOME_DIRECTORY}" _sdk_source_root)
    file(REAL_PATH "${BAMENGINE_ROOT}" _expected_source_root)

    if(NOT _sdk_source_root STREQUAL _expected_source_root)
        return()
    endif()

    if(NOT DEFINED _sdk_BAMENGINE_WITH_EDITOR OR
        NOT DEFINED _sdk_BAMENGINE_SHIPPING)
        return()
    endif()

    if(_sdk_CMAKE_GENERATOR MATCHES "^(Visual Studio|Ninja Multi-Config)")
        string(TOUPPER "${CONFIG}" _requested_upper)
        string(TOUPPER "${_sdk_CMAKE_CONFIGURATION_TYPES}" _available_upper)

        if(NOT _requested_upper IN_LIST _available_upper)
            return()
        endif()

        set(_lib_dir "${BUILD_DIR}/lib/${CONFIG}")
        set(_bin_dir "${BUILD_DIR}/bin/${CONFIG}")
    else()
        string(TOUPPER "${CONFIG}" _requested_upper)
        string(TOUPPER "${_sdk_CMAKE_BUILD_TYPE}" _actual_upper)

        if(NOT _requested_upper STREQUAL _actual_upper)
            return()
        endif()

        set(_lib_dir "${BUILD_DIR}/lib")
        set(_bin_dir "${BUILD_DIR}/bin")
    endif()

    # A matching import library and DLL must both exist.
    if(NOT EXISTS "${_lib_dir}/Engine.lib" OR
        NOT EXISTS "${_bin_dir}/Engine.dll")
        return()
    endif()

    if(_sdk_BAMENGINE_WITH_EDITOR)
        set(_editor 1)
    else()
        set(_editor 0)
    endif()

    if(_sdk_BAMENGINE_SHIPPING)
        set(_shipping 1)
    else()
        set(_shipping 0)
    endif()

    set(_bam_engine_lib_dir "${_lib_dir}" PARENT_SCOPE)
    set(_bam_engine_bin_dir "${_bin_dir}" PARENT_SCOPE)
    set(_bam_engine_editor "${_editor}" PARENT_SCOPE)
    set(_bam_engine_shipping "${_shipping}" PARENT_SCOPE)

    set(${OUT_VALID} TRUE PARENT_SCOPE)
endfunction()

# ------------------------------------------------------------
# Resolve engine build directory
# ------------------------------------------------------------

if(DEFINED BAMENGINE_BUILD_DIR AND NOT BAMENGINE_BUILD_DIR STREQUAL "")
    file(TO_CMAKE_PATH "${BAMENGINE_BUILD_DIR}" BAMENGINE_BUILD_DIR)

    _bam_validate_build("${BAMENGINE_BUILD_DIR}" "${_bam_config}" _bam_valid)

    if(NOT _bam_valid)
        message(FATAL_ERROR
            "BamEngineConfig: Explicit engine build directory is invalid "
            "or incompatible with ${_bam_config}: ${BAMENGINE_BUILD_DIR}")
    endif()
else()
    set(_bam_candidates
        "${BAMENGINE_ROOT}/out/build/x64-ninja-${_bam_config_lower}"
        "${BAMENGINE_ROOT}/out/build/x64-${_bam_config_lower}"
        "${BAMENGINE_ROOT}/build"
    )

    foreach(_candidate IN LISTS _bam_candidates)
        _bam_validate_build("${_candidate}" "${_bam_config}" _bam_valid)

        if(_bam_valid)
            set(BAMENGINE_BUILD_DIR "${_candidate}")
            break()
        endif()
    endforeach()

    if(NOT _bam_valid)
        message(FATAL_ERROR
            "BamEngineConfig: No compatible ${_bam_config} engine build found. "
            "Build the engine or specify BAMENGINE_BUILD_DIR.")
    endif()
endif()

# ------------------------------------------------------------
# Collect public include directories
# ------------------------------------------------------------

set(_bam_engine_public_includes "")

file(GLOB_RECURSE _bam_engine_headers CONFIGURE_DEPENDS
    "${BAMENGINE_ROOT}/Engine/Source/*.h"
    "${BAMENGINE_ROOT}/Engine/Source/*.inl"
)

foreach(_hdr IN LISTS _bam_engine_headers)
    get_filename_component(_dir "${_hdr}" DIRECTORY)

    if(_dir MATCHES "/Public(/|$)" AND NOT _dir MATCHES "/Private(/|$)")
        list(APPEND _bam_engine_public_includes "${_dir}")
    endif()
endforeach()

foreach(_sub_inc
    "${BAMENGINE_ROOT}/Projects/Reflection/include"
    "${BAMENGINE_ROOT}/Projects/Archive/include"
    "${BAMENGINE_ROOT}/Projects/ReflectionArchive/include")

    if(EXISTS "${_sub_inc}")
        list(APPEND _bam_engine_public_includes "${_sub_inc}")
    endif()
endforeach()

if(EXISTS "${BAMENGINE_BUILD_DIR}/vcpkg_installed/x64-windows/include")
    list(APPEND _bam_engine_public_includes
        "${BAMENGINE_BUILD_DIR}/vcpkg_installed/x64-windows/include")
endif()

list(REMOVE_DUPLICATES _bam_engine_public_includes)

# ------------------------------------------------------------
# Imported Engine target
# ------------------------------------------------------------

add_library(BamEngine::Engine SHARED IMPORTED GLOBAL)

set_target_properties(BamEngine::Engine PROPERTIES
    IMPORTED_CONFIGURATIONS "${_bam_config}"

    "IMPORTED_IMPLIB_${_bam_config_upper}"
        "${_bam_engine_lib_dir}/Engine.lib"

    "IMPORTED_LOCATION_${_bam_config_upper}"
        "${_bam_engine_bin_dir}/Engine.dll"

    INTERFACE_INCLUDE_DIRECTORIES "${_bam_engine_public_includes}"
)

target_compile_definitions(BamEngine::Engine INTERFACE
    BAM_EDITOR=${_bam_engine_editor}
    BAM_SHIPPING=${_bam_engine_shipping}
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    GLM_FORCE_LEFT_HANDED
    GLM_ENABLE_EXPERIMENTAL
    FMT_HEADER_ONLY
)

target_compile_features(BamEngine::Engine INTERFACE cxx_std_23)

if(MSVC)
    target_compile_options(BamEngine::Engine INTERFACE
        /EHsc
        /Zc:preprocessor
        /utf-8
    )
endif()

# ------------------------------------------------------------
# Reflection runtime dependencies
# ------------------------------------------------------------

if(NOT TARGET Reflection::Runtime)
    set(_bam_reflection_libs
        "${_bam_engine_lib_dir}/ReflectionRuntime.lib"
        "${_bam_engine_lib_dir}/ArchiveJson.lib"
    )

    foreach(_lib IN LISTS _bam_reflection_libs)
        if(NOT EXISTS "${_lib}")
            message(FATAL_ERROR
                "BamEngineConfig: Missing ${_bam_config} library: ${_lib}")
        endif()
    endforeach()

    add_library(Reflection::Runtime INTERFACE IMPORTED GLOBAL)

    set_target_properties(Reflection::Runtime PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES
            "${BAMENGINE_ROOT}/Projects/Reflection/include"
        INTERFACE_LINK_LIBRARIES
            "${_bam_reflection_libs}"
    )

    target_compile_features(Reflection::Runtime INTERFACE cxx_std_23)
endif()

if(EXISTS "${BAMENGINE_ROOT}/Projects/Reflection/cmake/ReflectionGenerate.cmake")
    include("${BAMENGINE_ROOT}/Projects/Reflection/cmake/ReflectionGenerate.cmake")
endif()

# ------------------------------------------------------------
# ReflectionGenerator
# ------------------------------------------------------------

set(BAMENGINE_REFLECTION_GENERATOR "")

foreach(_gen_cand
    "${_bam_engine_bin_dir}/ReflectionGenerator.exe"
    "${BAMENGINE_BUILD_DIR}/Projects/Reflection/ReflectionGenerator.exe")

    if(EXISTS "${_gen_cand}")
        set(BAMENGINE_REFLECTION_GENERATOR "${_gen_cand}")
        break()
    endif()
endforeach()

if(BAMENGINE_REFLECTION_GENERATOR AND NOT REFLECTION_GENERATOR_EXECUTABLE)
    set(REFLECTION_GENERATOR_EXECUTABLE "${BAMENGINE_REFLECTION_GENERATOR}")
endif()

if(BAMENGINE_CLANG_RESOURCE_DIR AND NOT REFLECTION_CLANG_RESOURCE_DIR)
    set(REFLECTION_CLANG_RESOURCE_DIR "${BAMENGINE_CLANG_RESOURCE_DIR}")
endif()

# ------------------------------------------------------------
# SDK diagnostics
# ------------------------------------------------------------

message(STATUS "[BamEngine SDK] Configuration: ${_bam_config}")
message(STATUS "[BamEngine SDK] Build directory: ${BAMENGINE_BUILD_DIR}")
message(STATUS "[BamEngine SDK] Engine library: ${_bam_engine_lib_dir}/Engine.lib")
message(STATUS "[BamEngine SDK] Engine DLL: ${_bam_engine_bin_dir}/Engine.dll")
message(STATUS "[BamEngine SDK] BAM_EDITOR=${_bam_engine_editor}")
message(STATUS "[BamEngine SDK] BAM_SHIPPING=${_bam_engine_shipping}")

set(BamEngine_FOUND TRUE)