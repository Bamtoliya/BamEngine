file(GLOB_RECURSE _archive_generated_test_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/examples/generated-tests/*.cpp"
)

add_executable(ArchiveGeneratedReflectionExample examples/GeneratedReflectionExample.cpp ${_archive_generated_test_sources})

target_link_libraries(ArchiveGeneratedReflectionExample PRIVATE ReflectionArchive::Adapter)
set_target_properties(ArchiveGeneratedReflectionExample PROPERTIES CXX_EXTENSIONS OFF UNITY_BUILD OFF)

reflection_generate(TARGET ArchiveGeneratedReflectionExample MODULE ArchiveDemo HEADERS examples/GeneratedArchiveTypes.h)
