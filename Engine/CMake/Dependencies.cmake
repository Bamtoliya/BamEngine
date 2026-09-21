# --- CMake/Dependencies.cmake ---

find_package(fmt CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_package(stb REQUIRED)
find_package(lz4 CONFIG REQUIRED)

find_package(directx-headers CONFIG REQUIRED)

find_package(glslang CONFIG REQUIRED)
find_package(SPIRV-Tools CONFIG REQUIRED)
find_package(SPIRV-Tools-opt CONFIG REQUIRED)

find_package(spirv_cross_core CONFIG REQUIRED)
find_package(spirv_cross_glsl CONFIG REQUIRED)
find_package(spirv_cross_hlsl CONFIG REQUIRED)
find_package(spirv_cross_msl CONFIG REQUIRED)
find_package(spirv_cross_cpp CONFIG REQUIRED)
find_package(spirv_cross_reflect CONFIG REQUIRED)
find_package(spirv_cross_c CONFIG REQUIRED)

find_path(CPPCODEC_INCLUDE_DIRS "cppcodec/base64_rfc4648.hpp" REQUIRED)