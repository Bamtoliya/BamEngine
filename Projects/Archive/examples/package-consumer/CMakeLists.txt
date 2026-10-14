cmake_minimum_required(VERSION 3.21)
project(ArchivePackageConsumer LANGUAGES CXX)

find_package(Archive 0.1 CONFIG REQUIRED)

file(GLOB_RECURSE _archive_package_test_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
)

add_executable(ArchivePackageConsumer main.cpp ${_archive_package_test_sources})
target_link_libraries(ArchivePackageConsumer PRIVATE Archive::Json)
set_target_properties(ArchivePackageConsumer PROPERTIES CXX_EXTENSIONS OFF UNITY_BUILD OFF)
