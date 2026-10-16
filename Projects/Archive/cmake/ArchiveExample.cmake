file(GLOB_RECURSE _archive_example_test_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/examples/tests/*.cpp"
)

add_executable(ArchiveExample examples/main.cpp ${_archive_example_test_sources})

target_link_libraries(ArchiveExample PRIVATE Archive::Json)
set_target_properties(ArchiveExample PROPERTIES CXX_EXTENSIONS OFF UNITY_BUILD OFF)

archive_add_example_test(ArchiveExample)
