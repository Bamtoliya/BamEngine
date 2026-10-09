cmake_minimum_required(VERSION 3.21)

project(ReflectionArchivePackageConsumer LANGUAGES CXX)

find_package(ReflectionArchive 0.1 CONFIG REQUIRED COMPONENTS Adapter)

add_executable(ReflectionArchivePackageConsumer main.cpp)
target_link_libraries(ReflectionArchivePackageConsumer PRIVATE ReflectionArchive::Adapter)

set_target_properties(ReflectionArchivePackageConsumer PROPERTIES CXX_EXTENSIONS OFF)

message(STATUS "ReflectionArchive package: ${ReflectionArchive_DIR}")
message(STATUS "Reflection package: ${Reflection_DIR}")
message(STATUS "Archive package: ${Archive_DIR}")

enable_testing()
add_test(NAME ReflectionArchivePackageConsumer COMMAND ReflectionArchivePackageConsumer)