if(NOT TARGET Reflection::Runtime)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../Reflection" "${CMAKE_CURRENT_BINARY_DIR}/reflection")
endif()

if(NOT TARGET ReflectionArchive::Adapter)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../ReflectionArchive" "${CMAKE_CURRENT_BINARY_DIR}/reflection-archive")
endif()
