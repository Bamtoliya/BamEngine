include(CMakePackageConfigHelpers)

set(_archive_package_dir "${CMAKE_INSTALL_LIBDIR}/cmake/Archive")

install(TARGETS ArchiveJson EXPORT ArchiveTargets)

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/archive"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT Archive
    FILES_MATCHING PATTERN "*.h"
)

install(EXPORT ArchiveTargets
    FILE ArchiveTargets.cmake
    NAMESPACE Archive::
    DESTINATION "${_archive_package_dir}"
    COMPONENT Archive
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ArchiveConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/ArchiveConfig.cmake"
    INSTALL_DESTINATION "${_archive_package_dir}"
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/ArchiveConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}" COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/ArchiveConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/ArchiveConfigVersion.cmake"
    DESTINATION "${_archive_package_dir}"
    COMPONENT Archive
)