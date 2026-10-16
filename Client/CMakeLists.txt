project(Client)

set(CLIENT_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Source")

file(GLOB_RECURSE CLIENT_HEADERS CONFIGURE_DEPENDS "${CLIENT_SOURCE_DIR}/*.h")
file(GLOB_RECURSE CLIENT_SOURCES CONFIGURE_DEPENDS "${CLIENT_SOURCE_DIR}/*.cpp")

add_executable(Client ${CLIENT_HEADERS} ${CLIENT_SOURCES})

target_link_libraries(Client PRIVATE BamEngine::Engine)

set_target_properties(Client PROPERTIES
    FOLDER "Applications"
    UNITY_BUILD OFF
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:Client>"
)

if(MSVC)
    target_compile_options(Client PRIVATE /Zc:preprocessor /utf-8)
endif()