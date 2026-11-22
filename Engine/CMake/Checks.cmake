add_executable(SceneReflectionCheck examples/SceneReflectionCheck.cpp)
target_link_libraries(SceneReflectionCheck PRIVATE BamEngine::Engine)
set_target_properties(SceneReflectionCheck PROPERTIES UNITY_BUILD OFF FOLDER "Checks")

add_executable(SceneArchiveCheck examples/SceneArchiveCheck.cpp)
target_link_libraries(SceneArchiveCheck PRIVATE BamEngine::Engine)
set_target_properties(SceneArchiveCheck PROPERTIES UNITY_BUILD OFF FOLDER "Checks")

add_executable(ActionTimelinePlaybackCheck examples/ActionTimelinePlaybackCheck.cpp)
target_link_libraries(ActionTimelinePlaybackCheck PRIVATE BamEngine::Engine)
set_target_properties(ActionTimelinePlaybackCheck PROPERTIES UNITY_BUILD OFF FOLDER "Checks")

add_executable(UILayoutCheck examples/UILayoutCheck.cpp)
target_link_libraries(UILayoutCheck PRIVATE BamEngine::Engine)
set_target_properties(UILayoutCheck PROPERTIES UNITY_BUILD OFF FOLDER "Checks")

add_executable(UIInteractionCheck examples/UIInteractionCheck.cpp)
target_link_libraries(UIInteractionCheck PRIVATE BamEngine::Engine)
set_target_properties(UIInteractionCheck PROPERTIES UNITY_BUILD OFF FOLDER "Checks")

add_executable(InputManagerCheck examples/InputManagerCheck.cpp)
target_link_libraries(InputManagerCheck PRIVATE BamEngine::Engine)
set_target_properties(InputManagerCheck PROPERTIES UNITY_BUILD OFF FOLDER "Checks")

if(BUILD_TESTING)
    add_test(NAME InputManagerCheck COMMAND InputManagerCheck)
    add_test(NAME UIInteractionCheck COMMAND UIInteractionCheck)
    add_test(NAME UILayoutCheck COMMAND UILayoutCheck)
    add_test(NAME SceneReflectionCheck COMMAND SceneReflectionCheck)
    add_test(NAME SceneArchiveCheck COMMAND SceneArchiveCheck)
    add_test(NAME ActionTimelinePlaybackCheck COMMAND ActionTimelinePlaybackCheck)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/scene-archive-check")
    set_tests_properties(SceneArchiveCheck PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/scene-archive-check")
endif()

if(MSVC)
    target_compile_options(UIInteractionCheck PRIVATE /utf-8)
    target_compile_options(UILayoutCheck PRIVATE /utf-8)
    target_compile_options(SceneReflectionCheck PRIVATE /utf-8)
    target_compile_options(SceneArchiveCheck PRIVATE /utf-8)
    target_compile_options(ActionTimelinePlaybackCheck PRIVATE /utf-8)
endif()

foreach(check_target IN ITEMS
    TerrainDataCheck
    Physics2DCheck
    CollisionGeometryCheck
    KinematicCharacterCheck
    SplineCurveCheck
    BuiltInMeshWindingCheck
    RaytracedShadowSettingsCheck
    RaytracingBuildPolicyCheck
    LODSelectionCheck
    SkyLightDirectionCheck
    CloudResourcesCheck
)
    add_executable(${check_target} examples/${check_target}.cpp)
    target_link_libraries(${check_target} PRIVATE BamEngine::Engine)
    set_target_properties(${check_target} PROPERTIES UNITY_BUILD OFF FOLDER "Checks")

    if(MSVC)
        target_compile_options(${check_target} PRIVATE /utf-8 /Zc:preprocessor)
    endif()

    if(BUILD_TESTING)
        add_test(NAME ${check_target} COMMAND ${check_target})
    endif()
endforeach()

add_executable(BlendTree2DCheck examples/BlendTree2DCheck.cpp)
target_link_libraries(BlendTree2DCheck PRIVATE BamEngine::Engine)

if(BUILD_TESTING)
    add_test(NAME BlendTree2DCheck COMMAND BlendTree2DCheck)
endif()

add_executable(VirtualPathCheck examples/VirtualPathCheck.cpp)
target_link_libraries(VirtualPathCheck PRIVATE BamEngine::Engine)
set_target_properties(VirtualPathCheck PROPERTIES UNITY_BUILD OFF FOLDER "Checks")

if(MSVC)
    target_compile_options(VirtualPathCheck PRIVATE /utf-8)
endif()

if(BUILD_TESTING)
    add_test(NAME VirtualPathCheck COMMAND VirtualPathCheck)
endif()

add_executable(TextureMipmapCheck examples/TextureMipmapCheck.cpp)
target_link_libraries(TextureMipmapCheck PRIVATE BamEngine::Engine)
set_target_properties(TextureMipmapCheck PROPERTIES UNITY_BUILD OFF FOLDER "Checks")

if(MSVC)
    target_compile_options(TextureMipmapCheck PRIVATE /utf-8)
endif()

if(BUILD_TESTING)
    add_test(NAME TextureMipmapCheck COMMAND TextureMipmapCheck)
endif()

add_executable(PixelPerfectCameraCheck examples/PixelPerfectCameraCheck.cpp)
target_link_libraries(PixelPerfectCameraCheck PRIVATE BamEngine::Engine)
set_target_properties(PixelPerfectCameraCheck PROPERTIES UNITY_BUILD OFF FOLDER "Checks")

if(MSVC)
    target_compile_options(PixelPerfectCameraCheck PRIVATE /utf-8)
endif()

if(BUILD_TESTING)
    add_test(NAME PixelPerfectCameraCheck COMMAND PixelPerfectCameraCheck)
endif()

add_executable(TilemapCheck examples/TilemapCheck.cpp)
target_link_libraries(TilemapCheck PRIVATE BamEngine::Engine)
set_target_properties(TilemapCheck PROPERTIES UNITY_BUILD OFF FOLDER "Checks")

if(MSVC)
    target_compile_options(TilemapCheck PRIVATE /utf-8)
endif()

if(BUILD_TESTING)
    add_test(NAME TilemapCheck COMMAND TilemapCheck)
endif()

add_executable(SpriteAtlasCheck examples/SpriteAtlasCheck.cpp)
target_link_libraries(SpriteAtlasCheck PRIVATE BamEngine::Engine)
set_target_properties(SpriteAtlasCheck PROPERTIES UNITY_BUILD OFF FOLDER "Checks")

if(MSVC)
    target_compile_options(SpriteAtlasCheck PRIVATE /utf-8)
endif()

if(BUILD_TESTING)
    add_test(NAME SpriteAtlasCheck COMMAND SpriteAtlasCheck)
endif()

add_executable(RenderPipelineProfileCheck examples/RenderPipelineProfileCheck.cpp)
target_link_libraries(RenderPipelineProfileCheck PRIVATE BamEngine::Engine)
set_target_properties(RenderPipelineProfileCheck PROPERTIES UNITY_BUILD OFF FOLDER "Checks")

if(BUILD_TESTING)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/render-pipeline-profile-check")
    add_test(NAME RenderPipelineProfileCheck COMMAND RenderPipelineProfileCheck
        "${CMAKE_CURRENT_SOURCE_DIR}/Resources/RenderPipelines/Deferred.bamrenderpipeline.json")
    set_tests_properties(RenderPipelineProfileCheck PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/render-pipeline-profile-check")
endif()

add_executable(MeshInstancingCheck examples/MeshInstancingCheck.cpp)
target_link_libraries(MeshInstancingCheck PRIVATE BamEngine::Engine)
set_target_properties(MeshInstancingCheck PROPERTIES UNITY_BUILD OFF FOLDER "Checks")

if(MSVC)
    target_compile_options(MeshInstancingCheck PRIVATE /utf-8)
endif()

if(BUILD_TESTING)
    add_test(NAME MeshInstancingCheck COMMAND MeshInstancingCheck)
endif()

add_executable(IBLShadingCheck examples/IBLShadingCheck.cpp)
target_link_libraries(IBLShadingCheck PRIVATE BamEngine::Engine)
set_target_properties(IBLShadingCheck PROPERTIES UNITY_BUILD OFF FOLDER "Checks")

if(MSVC)
    target_compile_options(IBLShadingCheck PRIVATE /utf-8)
endif()

if(BUILD_TESTING)
    add_test(NAME IBLShadingCheck COMMAND IBLShadingCheck)
endif()

add_executable(TiledLightingCheck examples/TiledLightingCheck.cpp)
target_link_libraries(TiledLightingCheck PRIVATE BamEngine::Engine)
set_target_properties(TiledLightingCheck PROPERTIES UNITY_BUILD OFF FOLDER "Checks")

if(MSVC)
    target_compile_options(TiledLightingCheck PRIVATE /utf-8)
endif()

if(BUILD_TESTING)
    add_test(NAME TiledLightingCheck COMMAND TiledLightingCheck)
endif()

foreach(sprint_c_check IN ITEMS
    PakArchiveCheck
    SaveGameCheck
    NavigationAICheck
    AudioMixerCheck
    NetworkRpcCheck
    SceneStreamingCheck
    RLSimulationCheck
)
    add_executable(${sprint_c_check} examples/${sprint_c_check}.cpp)
    target_link_libraries(${sprint_c_check} PRIVATE BamEngine::Engine)
    set_target_properties(${sprint_c_check} PROPERTIES UNITY_BUILD OFF FOLDER "Checks")

    if(MSVC)
        target_compile_options(${sprint_c_check} PRIVATE /utf-8 /Zc:preprocessor)
    endif()

    if(BUILD_TESTING)
        add_test(NAME ${sprint_c_check} COMMAND ${sprint_c_check})
    endif()
endforeach()

