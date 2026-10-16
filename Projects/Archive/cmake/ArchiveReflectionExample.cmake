file(GLOB_RECURSE _archive_reflection_test_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/examples/reflection-tests/*.cpp"
)

add_executable(ArchiveReflectionExample
    examples/ReflectionExample.cpp
    ${_archive_reflection_test_sources}
)

target_link_libraries(ArchiveReflectionExample PRIVATE ReflectionArchive::Adapter)
set_target_properties(ArchiveReflectionExample PROPERTIES
    CXX_EXTENSIONS OFF
    UNITY_BUILD OFF
)

archive_add_example_test(ArchiveReflectionExample)

if(ARCHIVE_BUILD_GLM_EXAMPLE)
    set(_engine_archive_public "${CMAKE_CURRENT_SOURCE_DIR}/../../Engine/Source/Core/Archive/Public")

    target_include_directories(ArchiveReflectionExample PRIVATE "${_engine_archive_public}")
    target_link_libraries(ArchiveReflectionExample PRIVATE glm::glm-header-only)
    target_compile_definitions(ArchiveReflectionExample PRIVATE ARCHIVE_EXAMPLE_GLM)
endif()
