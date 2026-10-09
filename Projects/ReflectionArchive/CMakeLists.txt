cmake_minimum_required(VERSION 3.21)

project(ReflectionArchive VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

if(NOT TARGET Reflection::Runtime)
    find_package(Reflection 0.1 CONFIG REQUIRED)
endif()

if(NOT TARGET Archive::Json)
    find_package(Archive 0.1 CONFIG REQUIRED)
endif()

add_library(ReflectionArchiveAdapter INTERFACE)
add_library(ReflectionArchive::Adapter ALIAS ReflectionArchiveAdapter)

target_include_directories(ReflectionArchiveAdapter INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_compile_features(ReflectionArchiveAdapter INTERFACE cxx_std_23)
target_link_libraries(ReflectionArchiveAdapter INTERFACE Reflection::Runtime Archive::Json)

set_target_properties(ReflectionArchiveAdapter PROPERTIES EXPORT_NAME Adapter)

set(_reflection_archive_package_dir "${CMAKE_INSTALL_LIBDIR}/cmake/ReflectionArchive")

install(TARGETS ReflectionArchiveAdapter EXPORT ReflectionArchiveTargets)

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/reflection_archive"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    FILES_MATCHING PATTERN "*.h"
)

install(EXPORT ReflectionArchiveTargets
    FILE ReflectionArchiveTargets.cmake
    NAMESPACE ReflectionArchive::
    DESTINATION "${_reflection_archive_package_dir}"
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ReflectionArchiveConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/ReflectionArchiveConfig.cmake"
    INSTALL_DESTINATION "${_reflection_archive_package_dir}"
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/ReflectionArchiveConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/ReflectionArchiveConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/ReflectionArchiveConfigVersion.cmake"
    DESTINATION "${_reflection_archive_package_dir}"
)