cmake_minimum_required(VERSION 3.21)
project(UserPanel_Test LANGUAGES CXX)
find_package(imgui CONFIG REQUIRED)
add_library(UserPanel_Test MODULE Panel.cpp)
target_compile_features(UserPanel_Test PRIVATE cxx_std_20)
target_include_directories(UserPanel_Test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../Editor/Source/HotReload/Public"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../Engine/Source/Core/Module/Public")
target_link_libraries(UserPanel_Test PRIVATE imgui::imgui)
if(MSVC)
    target_compile_options(UserPanel_Test PRIVATE /utf-8 /Zc:preprocessor)
    target_link_options(UserPanel_Test PRIVATE "$<$<CONFIG:Debug>:/PDBALTPATH:%_PDB%>")
endif()
set_target_properties(UserPanel_Test PROPERTIES PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Binaries/Win64/$<0:>"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Binaries/Win64/$<0:>")
