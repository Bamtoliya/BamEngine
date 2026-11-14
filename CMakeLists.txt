cmake_minimum_required(VERSION 3.21)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

set(CMAKE_UNITY_BUILD ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

if(POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT
      "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project(BamEngine)

include(CTest)
option(BAMENGINE_BUILD_REFLECTION_CHECK "Build engine and editor reflection integration checks" OFF)
if(EMSCRIPTEN)
    set(_bamengine_with_editor_default OFF)
else()
    set(_bamengine_with_editor_default ON)
endif()
option(BAMENGINE_WITH_EDITOR "Build with editor support and editor-only engine APIs" ${_bamengine_with_editor_default})

if(EMSCRIPTEN AND BAMENGINE_WITH_EDITOR)
    message(FATAL_ERROR "BAMENGINE_WITH_EDITOR is not supported by the Emscripten build.")
endif()
if(EMSCRIPTEN AND BAMENGINE_BUILD_REFLECTION_CHECK)
    message(FATAL_ERROR "BAMENGINE_BUILD_REFLECTION_CHECK is not supported by the Emscripten build.")
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_subdirectory(Projects/Reflection)
add_subdirectory(Projects/Archive)

if(NOT TARGET ReflectionArchive::Adapter)
    add_subdirectory(Projects/ReflectionArchive)
endif()

if(EMSCRIPTEN)
    if(REFLECTION_BUILD_GENERATOR)
        message(FATAL_ERROR "Build ReflectionGenerator with a host toolchain and set REFLECTION_BUILD_GENERATOR=OFF for Emscripten.")
    endif()
    if(NOT REFLECTION_GENERATOR_EXECUTABLE OR NOT EXISTS "${REFLECTION_GENERATOR_EXECUTABLE}")
        message(FATAL_ERROR "Set REFLECTION_GENERATOR_EXECUTABLE to an existing host-native reflection generator.")
    endif()
    if(NOT REFLECTION_CLANG_RESOURCE_DIR OR NOT EXISTS "${REFLECTION_CLANG_RESOURCE_DIR}/include/stddef.h")
        message(FATAL_ERROR "Set REFLECTION_CLANG_RESOURCE_DIR to the host Clang resource directory.")
    endif()
endif()

add_subdirectory(Engine)

if(NOT EMSCRIPTEN)
    add_subdirectory(Tools/Converters/ModelConverter)
    add_subdirectory(Tools/Converters/TextureConverter)

    if(BAMENGINE_WITH_EDITOR)
        add_subdirectory(Editor)
    endif()
    add_subdirectory(Projects/Plugins/GameplayScore)
    add_subdirectory(Projects/GameModule)
    add_subdirectory(Client)
endif()
