find_package(glm CONFIG REQUIRED)

set(_engine_archive_public "${CMAKE_CURRENT_SOURCE_DIR}/../../Engine/Source/Core/Archive/Public")

file(GLOB_RECURSE _archive_glm_test_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/examples/glm-tests/*.cpp"
)

add_executable(ArchiveGlmExample examples/GlmExample.cpp ${_archive_glm_test_sources})

target_include_directories(ArchiveGlmExample PRIVATE "${_engine_archive_public}")
target_link_libraries(ArchiveGlmExample PRIVATE Archive::Json glm::glm-header-only)
set_target_properties(ArchiveGlmExample PROPERTIES CXX_EXTENSIONS OFF UNITY_BUILD OFF)

archive_add_example_test(ArchiveGlmExample)
