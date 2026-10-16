# Example
if(REFLECTION_BUILD_EXAMPLE)
    file(GLOB_RECURSE _reflection_example_test_sources CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/tests/*.cpp"
    )

    add_executable(ReflectionExample examples/main.cpp ${_reflection_example_test_sources})

    set_target_properties(ReflectionExample PROPERTIES
        CXX_EXTENSIONS OFF
        UNITY_BUILD OFF
    )

    if(MSVC)
    target_compile_options(ReflectionExample PRIVATE /utf-8)

        set_property(TARGET ReflectionExample PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
        )
    endif()

    reflection_generate(
        TARGET ReflectionExample
        MODULE Demo
        HEADERS
            examples/ExampleTypes.h
            examples/ExtraTypes.h
            examples/DiagnosticsTypes.h
    )

    reflection_generate(
        TARGET ReflectionExample
        MODULE Failure
        HEADERS examples/FailureTypes.h
    )

    if(BUILD_TESTING)
        set(_reflection_test_directory "${CMAKE_CURRENT_BINARY_DIR}/tests/ReflectionExample")
        file(MAKE_DIRECTORY "${_reflection_test_directory}")
        add_test(NAME ReflectionExample COMMAND ReflectionExample)
        set_tests_properties(ReflectionExample PROPERTIES WORKING_DIRECTORY "${_reflection_test_directory}")
    endif()

endif()
