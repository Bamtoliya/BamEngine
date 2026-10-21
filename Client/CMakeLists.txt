project(Client)

set(CLIENT_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Source")

file(GLOB_RECURSE CLIENT_HEADERS CONFIGURE_DEPENDS "${CLIENT_SOURCE_DIR}/*.h")
file(GLOB_RECURSE CLIENT_SOURCES CONFIGURE_DEPENDS "${CLIENT_SOURCE_DIR}/*.cpp")

add_executable(Client ${CLIENT_HEADERS} ${CLIENT_SOURCES})

target_compile_definitions(Client PRIVATE
    BAMENGINE_PROJECT_ROOT="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(Client PRIVATE
    BamEngine::Engine
)

set_target_properties(Client PROPERTIES
    FOLDER "Applications"
    UNITY_BUILD OFF
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

if(MSVC)
    target_compile_options(Client PRIVATE
        $<$<CONFIG:Debug>:/ZI>
        $<$<CONFIG:Debug>:/FS>
        /Zc:preprocessor
        /utf-8
        /MP
    )
    target_link_options(Client PRIVATE
        $<$<CONFIG:Debug>:/INCREMENTAL>
    )
endif()

# FMOD 런타임 DLL 복사
add_custom_command(TARGET Client POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<IF:$<CONFIG:Debug>,${CMAKE_SOURCE_DIR}/ThirdParty/FMOD/lib/x64/fmodL.dll,${CMAKE_SOURCE_DIR}/ThirdParty/FMOD/lib/x64/fmod.dll>"
        "$<TARGET_FILE_DIR:Client>/"
    COMMENT "Copying FMOD runtime DLL to Client output directory"
)