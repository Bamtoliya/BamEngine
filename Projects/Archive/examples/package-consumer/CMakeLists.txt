cmake_minimum_required(VERSION 3.21)
project(ArchivePackageConsumer LANGUAGES CXX)

find_package(Archive 0.1 CONFIG REQUIRED)

add_executable(ArchivePackageConsumer main.cpp)
target_link_libraries(ArchivePackageConsumer PRIVATE Archive::Json)
set_target_properties(ArchivePackageConsumer PROPERTIES CXX_EXTENSIONS OFF)