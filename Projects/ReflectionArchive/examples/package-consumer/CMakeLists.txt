cmake_minimum_required(VERSION 3.21)

project(ReflectionArchivePackageConsumer LANGUAGES CXX)

include(CTest)

find_package(ReflectionArchive 0.1 CONFIG REQUIRED COMPONENTS Adapter)

add_executable(ReflectionArchivePackageConsumer main.cpp)
target_link_libraries(ReflectionArchivePackageConsumer PRIVATE ReflectionArchive::Adapter)

set_target_properties(ReflectionArchivePackageConsumer PROPERTIES CXX_EXTENSIONS OFF)

reflection_generate(
    TARGET ReflectionArchivePackageConsumer MODULE ArchiveConsumer HEADERS ConsumerTypes.h
    HEADER_INCLUDE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}"
)

if(BUILD_TESTING)
    add_test(NAME ReflectionArchivePackageConsumer COMMAND ReflectionArchivePackageConsumer)
endif()
