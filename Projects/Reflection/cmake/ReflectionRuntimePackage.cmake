include(CMakePackageConfigHelpers)

set(_reflection_package_dir "${CMAKE_INSTALL_LIBDIR}/cmake/Reflection")

install(TARGETS ReflectionRuntime EXPORT ReflectionTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}/$<CONFIG>"
)

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/reflection"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}" FILES_MATCHING PATTERN "*.h"
)

install(EXPORT ReflectionTargets
    FILE ReflectionTargets.cmake NAMESPACE Reflection::
    DESTINATION "${_reflection_package_dir}"
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ReflectionConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/ReflectionConfig.cmake"
    INSTALL_DESTINATION "${_reflection_package_dir}"
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/ReflectionConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}" COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/ReflectionConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/ReflectionConfigVersion.cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ReflectionGenerate.cmake"
    DESTINATION "${_reflection_package_dir}"
)