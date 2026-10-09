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

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
option(BAMENGINE_BUILD_REFLECTION_CHECK "Build Engine and Editor reflection integration checks" OFF)

add_subdirectory(Projects/ReflectionCore)
add_subdirectory(Projects/Reflection)

set(ARCHIVE_BUILD_EXAMPLE OFF)
add_subdirectory(Projects/Archive)

if(NOT TARGET ReflectionArchive::Adapter)
    add_subdirectory(Projects/ReflectionArchive)
endif()

add_subdirectory(Engine)

add_subdirectory(Tools/TextureConverter)
add_subdirectory(Editor)
# add_subdirectory(Client)