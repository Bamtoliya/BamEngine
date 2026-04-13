project(ReflectionCore LANGUAGES CXX)

file(GLOB_RECURSE REFLECTION_CORE_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
)

file(GLOB_RECURSE REFLECTION_CORE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

if(REFLECTION_CORE_SOURCES)
    add_library(ReflectionCore STATIC
        ${REFLECTION_CORE_HEADERS}
        ${REFLECTION_CORE_SOURCES}
    )
else()
    add_library(ReflectionCore INTERFACE)
endif()

add_library(Reflection::Core ALIAS ReflectionCore)

if(TARGET ReflectionCore AND NOT REFLECTION_CORE_SOURCES)
    target_include_directories(ReflectionCore
        INTERFACE
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
    )

    target_compile_features(ReflectionCore
        INTERFACE
            cxx_std_23
    )
else()
    target_include_directories(ReflectionCore
        PUBLIC
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
    )

    target_compile_features(ReflectionCore
        PUBLIC
            cxx_std_23
    )

    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/include" FILES ${REFLECTION_CORE_HEADERS})
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" FILES ${REFLECTION_CORE_SOURCES})
endif()

set_target_properties(ReflectionCore PROPERTIES
    FOLDER "Projects/Reflection"
)