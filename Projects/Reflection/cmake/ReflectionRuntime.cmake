include(GNUInstallDirs)

add_library(ReflectionRuntime STATIC src/Metadata.cpp src/Registry.cpp)
add_library(Reflection::Runtime ALIAS ReflectionRuntime)

target_include_directories(ReflectionRuntime PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_compile_features(ReflectionRuntime PUBLIC cxx_std_20)
set_target_properties(ReflectionRuntime PROPERTIES CXX_EXTENSIONS OFF EXPORT_NAME Runtime)

if(MSVC)
    target_compile_options(ReflectionRuntime PRIVATE /utf-8 PUBLIC /Zc:preprocessor)
endif()