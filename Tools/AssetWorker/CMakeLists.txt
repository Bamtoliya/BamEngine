project(BamAssetWorker LANGUAGES CXX)

add_executable(BamAssetWorker
    main.cpp
    HeadlessThumbnailRasterizer.h
    HeadlessThumbnailRasterizer.cpp
)

target_compile_definitions(BamAssetWorker PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

target_include_directories(BamAssetWorker PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/ThirdParty"
)

target_link_libraries(BamAssetWorker PRIVATE
    BamEngine::Engine
    BamTools::ModelConverter
    TextureConverter
)

set_target_properties(BamAssetWorker PROPERTIES
    FOLDER "Tools"
    UNITY_BUILD OFF
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

if(MSVC)
    target_compile_options(BamAssetWorker PRIVATE /utf-8 /Zc:preprocessor /MP /EHsc)
endif()
