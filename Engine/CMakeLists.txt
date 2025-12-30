project("Engine")

#find_package(fmt CONFIG REQUIRED)
#find_package(SDL2 CONFIG REQUIRED)

add_library(Engine SHARED
	src/Core.cpp
	src/Core.h
 "src/EngineAPI.h")


target_compile_definitions(Engine PRIVATE ENGINE_EXPORTS)

target_include_directories(Engine INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})