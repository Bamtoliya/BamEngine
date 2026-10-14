include(GNUInstallDirs)

find_package(glaze CONFIG REQUIRED)

file(GLOB_RECURSE _archive_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
add_library(ArchiveJson STATIC ${_archive_sources})
add_library(Archive::Json ALIAS ArchiveJson)

target_include_directories(ArchiveJson PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_compile_features(ArchiveJson PUBLIC cxx_std_23)
target_link_libraries(ArchiveJson PUBLIC glaze::glaze)

set_target_properties(ArchiveJson PROPERTIES EXPORT_NAME Json)

if(MSVC)
    target_compile_options(ArchiveJson PUBLIC /utf-8)
endif()