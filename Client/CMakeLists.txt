project(Client)

include("${CMAKE_SOURCE_DIR}/Projects/ReflectionCMake/ReflectionBundles.cmake")

set(CLIENT_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Source")
set(CLIENT_GENERATED_SOURCE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Reflection/Private/Generated/ClientReflection.gen.cpp"
)

file(GLOB_RECURSE CLIENT_HEADERS CONFIGURE_DEPENDS "${CLIENT_SOURCE_DIR}/*.h")
file(GLOB_RECURSE CLIENT_SOURCES CONFIGURE_DEPENDS "${CLIENT_SOURCE_DIR}/*.cpp")
file(GLOB_RECURSE CLIENT_ALL_FILES CONFIGURE_DEPENDS "${CLIENT_SOURCE_DIR}/*")

list(APPEND CLIENT_SOURCES "${CLIENT_GENERATED_SOURCE}")

add_executable(Client
    ${CLIENT_HEADERS}
    ${CLIENT_SOURCES}
)

add_reflection_bundle(
    TARGET Client
    MODULE Client
    SOURCE_ROOT "${CLIENT_SOURCE_DIR}"
    OUTPUT_CPP "${CLIENT_GENERATED_SOURCE}"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

target_link_libraries(Client
    PRIVATE
        BamEngine::Engine
        Reflection::Core
)

set_target_properties(Client PROPERTIES
    FOLDER "Applications"
)