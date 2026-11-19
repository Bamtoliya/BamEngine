project(BamAssetWorker LANGUAGES CXX)

find_package(directxtex CONFIG REQUIRED)

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
    Microsoft::DirectXTex
)

set_target_properties(BamAssetWorker PROPERTIES
    FOLDER "Tools"
    UNITY_BUILD OFF
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

if(MSVC)
    target_compile_options(BamAssetWorker PRIVATE /utf-8 /Zc:preprocessor /MP /EHsc)
endif()

if(BUILD_TESTING)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(NAME AssetWorkerModelCheck
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/ModelWorkerCheck.py" $<TARGET_FILE:BamAssetWorker>)
    set_tests_properties(AssetWorkerModelCheck PROPERTIES TIMEOUT 45)
endif()
