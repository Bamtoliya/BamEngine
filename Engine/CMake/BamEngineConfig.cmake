# Engine/CMake/BamEngineConfig.cmake
# External project C++ GameModule consumer interface.
# Usage in <ProjectRoot>/Source/CMakeLists.txt:
#   find_package(BamEngine REQUIRED PATHS "${BAMENGINE_ROOT}/Engine/CMake")
#   add_library(MyGameModule SHARED ...)
#   target_link_libraries(MyGameModule PRIVATE BamEngine::Engine)

if(TARGET BamEngine::Engine)
    set(BAMENGINE_CLANG_RESOURCE_DIR "")
file(GLOB _clang_res_candidates LIST_DIRECTORIES true
    "${BAMENGINE_ROOT}/Projects/Reflection/dependencies/llvm-*/lib/clang/*")
foreach(_res_cand ${_clang_res_candidates})
    if(IS_DIRECTORY "${_res_cand}/include")
        set(BAMENGINE_CLANG_RESOURCE_DIR "${_res_cand}")
        break()
    endif()
endforeach()

set(BamEngine_FOUND TRUE)
    return()
endif()

if(NOT DEFINED BAMENGINE_ROOT OR BAMENGINE_ROOT STREQUAL "")
    get_filename_component(BAMENGINE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
endif()
file(TO_CMAKE_PATH "${BAMENGINE_ROOT}" BAMENGINE_ROOT)

if(NOT DEFINED BAMENGINE_BUILD_DIR OR BAMENGINE_BUILD_DIR STREQUAL "")
    foreach(_candidate
        "${BAMENGINE_ROOT}/out/build/x64-ninja-debug"
        "${BAMENGINE_ROOT}/out/build/x64-ninja-release"
        "${BAMENGINE_ROOT}/out/build/x64-debug"
        "${BAMENGINE_ROOT}/out/build/x64-release"
        "${BAMENGINE_ROOT}/build")
        if(EXISTS "${_candidate}/CMakeCache.txt")
            set(BAMENGINE_BUILD_DIR "${_candidate}")
            break()
        endif()
    endforeach()
endif()
if(DEFINED BAMENGINE_BUILD_DIR AND NOT BAMENGINE_BUILD_DIR STREQUAL "")
    file(TO_CMAKE_PATH "${BAMENGINE_BUILD_DIR}" BAMENGINE_BUILD_DIR)
endif()

if(NOT EXISTS "${BAMENGINE_ROOT}/Engine/Source")
    message(FATAL_ERROR "BamEngineConfig: BAMENGINE_ROOT does not point to a valid BamEngine repository: ${BAMENGINE_ROOT}")
endif()

# Collect Engine Public include directories
set(_bam_engine_public_includes "")
file(GLOB_RECURSE _bam_engine_headers CONFIGURE_DEPENDS
    "${BAMENGINE_ROOT}/Engine/Source/*.h"
    "${BAMENGINE_ROOT}/Engine/Source/*.inl"
)
foreach(_hdr ${_bam_engine_headers})
    get_filename_component(_dir "${_hdr}" DIRECTORY)
    if(_dir MATCHES "/Public(/|$)" AND NOT _dir MATCHES "/Private(/|$)")
        list(APPEND _bam_engine_public_includes "${_dir}")
    endif()
endforeach()
list(REMOVE_DUPLICATES _bam_engine_public_includes)

# Include Reflection / Archive / ReflectionArchive public headers
foreach(_sub_inc
    "${BAMENGINE_ROOT}/Projects/Reflection/Include"
    "${BAMENGINE_ROOT}/Projects/Archive/Include"
    "${BAMENGINE_ROOT}/Projects/ReflectionArchive/Include")
    if(EXISTS "${_sub_inc}")
        list(APPEND _bam_engine_public_includes "${_sub_inc}")
    endif()
endforeach()

# vcpkg installed headers from engine build tree (if present)
if(DEFINED BAMENGINE_BUILD_DIR AND EXISTS "${BAMENGINE_BUILD_DIR}/vcpkg_installed/x64-windows/include")
    list(APPEND _bam_engine_public_includes "${BAMENGINE_BUILD_DIR}/vcpkg_installed/x64-windows/include")
endif()

# Resolve Engine.lib / Engine.dll from engine build tree
set(_bam_engine_implib "")
set(_bam_engine_dll "")
if(DEFINED BAMENGINE_BUILD_DIR AND NOT BAMENGINE_BUILD_DIR STREQUAL "")
    foreach(_lib_cand
        "${BAMENGINE_BUILD_DIR}/lib/$<CONFIG>/Engine.lib"
        "${BAMENGINE_BUILD_DIR}/lib/Debug/Engine.lib"
        "${BAMENGINE_BUILD_DIR}/lib/Release/Engine.lib"
        "${BAMENGINE_BUILD_DIR}/lib/Engine.lib")
        if(NOT _lib_cand MATCHES "\\$<" AND EXISTS "${_lib_cand}")
            set(_bam_engine_implib "${_lib_cand}")
            break()
        endif()
    endforeach()
    foreach(_dll_cand
        "${BAMENGINE_BUILD_DIR}/bin/Debug/Engine.dll"
        "${BAMENGINE_BUILD_DIR}/bin/Release/Engine.dll"
        "${BAMENGINE_BUILD_DIR}/bin/Engine.dll")
        if(EXISTS "${_dll_cand}")
            set(_bam_engine_dll "${_dll_cand}")
            break()
        endif()
    endforeach()
endif()

add_library(BamEngine::Engine SHARED IMPORTED GLOBAL)
set_target_properties(BamEngine::Engine PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${_bam_engine_public_includes}"
    INTERFACE_COMPILE_DEFINITIONS "GLM_FORCE_DEPTH_ZERO_TO_ONE;GLM_FORCE_LEFT_HANDED;GLM_ENABLE_EXPERIMENTAL;FMT_HEADER_ONLY"
)

if(MSVC)
    set_target_properties(BamEngine::Engine PROPERTIES
        INTERFACE_COMPILE_OPTIONS "/EHsc;/Zc:preprocessor;/utf-8"
    )
endif()

if(NOT _bam_engine_implib STREQUAL "")
    set_target_properties(BamEngine::Engine PROPERTIES IMPORTED_IMPLIB "${_bam_engine_implib}")
endif()
if(NOT _bam_engine_dll STREQUAL "")
    set_target_properties(BamEngine::Engine PROPERTIES IMPORTED_LOCATION "${_bam_engine_dll}")
endif()

# Helper macro to configure output into <ProjectRoot>/Binaries/Win64
function(bamengine_configure_game_module TARGET_NAME PROJECT_ROOT)
    set_target_properties(${TARGET_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${PROJECT_ROOT}/Binaries/Win64"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${PROJECT_ROOT}/Binaries/Win64"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${PROJECT_ROOT}/Binaries/Win64"
        LIBRARY_OUTPUT_DIRECTORY "${PROJECT_ROOT}/Binaries/Win64"
        LIBRARY_OUTPUT_DIRECTORY_DEBUG "${PROJECT_ROOT}/Binaries/Win64"
        LIBRARY_OUTPUT_DIRECTORY_RELEASE "${PROJECT_ROOT}/Binaries/Win64"
    )
    target_link_libraries(${TARGET_NAME} PRIVATE BamEngine::Engine)
endfunction()

# Reflection codegen support for external game modules
if(NOT TARGET Reflection::Runtime)
    set(_bam_reflection_libs "")
    if(DEFINED BAMENGINE_BUILD_DIR AND NOT BAMENGINE_BUILD_DIR STREQUAL "")
        foreach(_lib_name ReflectionRuntime.lib ArchiveJson.lib)
            foreach(_refl_cand
                "${BAMENGINE_BUILD_DIR}/lib/${_lib_name}"
                "${BAMENGINE_BUILD_DIR}/lib/Debug/${_lib_name}"
                "${BAMENGINE_BUILD_DIR}/lib/Release/${_lib_name}")
                if(EXISTS "${_refl_cand}")
                    list(APPEND _bam_reflection_libs "${_refl_cand}")
                    break()
                endif()
            endforeach()
        endforeach()
    endif()
    add_library(Reflection::Runtime INTERFACE IMPORTED GLOBAL)
    set_target_properties(Reflection::Runtime PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${BAMENGINE_ROOT}/Projects/Reflection/Include"
    )
    if(NOT _bam_reflection_libs STREQUAL "")
        set_target_properties(Reflection::Runtime PROPERTIES
            INTERFACE_LINK_LIBRARIES "${_bam_reflection_libs}"
        )
    endif()
endif()
if(EXISTS "${BAMENGINE_ROOT}/Projects/Reflection/cmake/ReflectionGenerate.cmake")
    include("${BAMENGINE_ROOT}/Projects/Reflection/cmake/ReflectionGenerate.cmake")
endif()

set(BAMENGINE_REFLECTION_GENERATOR "")
if(DEFINED BAMENGINE_BUILD_DIR AND NOT BAMENGINE_BUILD_DIR STREQUAL "")
    foreach(_gen_cand
        "${BAMENGINE_BUILD_DIR}/bin/ReflectionGenerator.exe"
        "${BAMENGINE_BUILD_DIR}/bin/Debug/ReflectionGenerator.exe"
        "${BAMENGINE_BUILD_DIR}/bin/Release/ReflectionGenerator.exe"
        "${BAMENGINE_BUILD_DIR}/Projects/Reflection/ReflectionGenerator.exe")
        if(EXISTS "${_gen_cand}")
            set(BAMENGINE_REFLECTION_GENERATOR "${_gen_cand}")
            break()
        endif()
    endforeach()
endif()

set(BAMENGINE_CLANG_RESOURCE_DIR "")
file(GLOB _clang_res_candidates LIST_DIRECTORIES true
    "${BAMENGINE_ROOT}/Projects/Reflection/dependencies/llvm-*/lib/clang/*")
foreach(_res_cand ${_clang_res_candidates})
    if(IS_DIRECTORY "${_res_cand}/include")
        set(BAMENGINE_CLANG_RESOURCE_DIR "${_res_cand}")
        break()
    endif()
endforeach()

set(BamEngine_FOUND TRUE)
