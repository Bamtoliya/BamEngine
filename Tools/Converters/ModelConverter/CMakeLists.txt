project(ModelConverter LANGUAGES CXX)

find_package(assimp CONFIG REQUIRED)

add_library(ModelConverter STATIC
    Public/ModelConverter.h
    Private/ModelConverter.cpp
)

add_library(BamTools::ModelConverter ALIAS ModelConverter)

target_include_directories(ModelConverter PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Public"
)

target_compile_features(ModelConverter PUBLIC cxx_std_20)
target_link_libraries(ModelConverter PRIVATE assimp::assimp)

set_target_properties(ModelConverter PROPERTIES
    FOLDER "Tools"
    UNITY_BUILD OFF
)

if(MSVC)
    target_compile_options(ModelConverter PRIVATE /utf-8 /Zc:preprocessor)
endif()