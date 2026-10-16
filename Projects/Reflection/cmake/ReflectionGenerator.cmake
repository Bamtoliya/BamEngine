set(_reflection_bundled_cmake "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/llvm-20.1.8/lib/cmake")

if(NOT LLVM_DIR AND EXISTS "${_reflection_bundled_cmake}/llvm/LLVMConfig.cmake")
    set(LLVM_DIR "${_reflection_bundled_cmake}/llvm")
endif()

if(NOT Clang_DIR AND EXISTS "${_reflection_bundled_cmake}/clang/ClangConfig.cmake")
    set(Clang_DIR "${_reflection_bundled_cmake}/clang")
endif()

# Generator
find_package(LLVM CONFIG REQUIRED)
find_package(Clang CONFIG REQUIRED)

if(MSVC AND LLVM_ENABLE_DIA_SDK AND TARGET LLVMDebugInfoPDB)
    find_library(REFLECTION_DIA_GUIDS_LIBRARY
        NAMES diaguids
        HINTS "${CMAKE_GENERATOR_INSTANCE}/DIA SDK/lib/amd64"
        REQUIRED
    )

    get_target_property(_pdb_dependencies
        LLVMDebugInfoPDB INTERFACE_LINK_LIBRARIES
    )

    set(_fixed_pdb_dependencies)
    foreach(_dependency IN LISTS _pdb_dependencies)
        get_filename_component(_dependency_name "${_dependency}" NAME)

        if(_dependency_name STREQUAL "diaguids.lib")
            list(APPEND _fixed_pdb_dependencies
                "${REFLECTION_DIA_GUIDS_LIBRARY}"
            )
        else()
            list(APPEND _fixed_pdb_dependencies "${_dependency}")
        endif()
    endforeach()

    set_property(TARGET LLVMDebugInfoPDB PROPERTY
        INTERFACE_LINK_LIBRARIES "${_fixed_pdb_dependencies}"
    )
endif()

add_executable(ReflectionGenerator
    generator/main.cpp
    generator/MetadataParser.h
    generator/MetadataParser.cpp
    generator/ReflectionModel.h
    generator/ReflectionUnit.h
    generator/ReflectionUnit.cpp
    generator/RegistrationEmitter.h
    generator/RegistrationEmitter.cpp
)

target_compile_features(ReflectionGenerator PRIVATE cxx_std_20)

set_target_properties(ReflectionGenerator PROPERTIES
    CXX_EXTENSIONS OFF
    UNITY_BUILD OFF
)

target_include_directories(ReflectionGenerator SYSTEM PRIVATE
    ${LLVM_INCLUDE_DIRS}
    ${CLANG_INCLUDE_DIRS}
)

separate_arguments(
    REFLECTION_LLVM_DEFINITIONS
    NATIVE_COMMAND
    "${LLVM_DEFINITIONS}"
)

target_compile_options(ReflectionGenerator PRIVATE
    ${REFLECTION_LLVM_DEFINITIONS}
)

target_link_libraries(ReflectionGenerator PRIVATE
    clangTooling
    Reflection::Runtime
)

if(MSVC)
    target_compile_options(ReflectionGenerator PRIVATE /utf-8 /Zc:preprocessor /bigobj)
endif()
