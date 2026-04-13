project(Editor)

include("${CMAKE_SOURCE_DIR}/Projects/ReflectionCMake/ReflectionBundles.cmake")

set(EDITOR_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Source")
set(EDITOR_GENERATED_SOURCE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Reflection/Private/Generated/EditorReflection.gen.cpp"
)

file(GLOB_RECURSE EDITOR_HEADERS CONFIGURE_DEPENDS "${EDITOR_SOURCE_DIR}/*.h")
file(GLOB_RECURSE EDITOR_SOURCES CONFIGURE_DEPENDS "${EDITOR_SOURCE_DIR}/*.cpp")
file(GLOB_RECURSE EDITOR_ALL_FILES CONFIGURE_DEPENDS "${EDITOR_SOURCE_DIR}/*")

list(APPEND EDITOR_SOURCES "${EDITOR_GENERATED_SOURCE}")

source_group(TREE "${EDITOR_SOURCE_DIR}" FILES ${EDITOR_HEADERS} ${EDITOR_SOURCES})

set(EDITOR_PUBLIC_INCLUDE_DIRS "")
foreach(FILE_PATH ${EDITOR_ALL_FILES})
    get_filename_component(DIR_PATH "${FILE_PATH}" DIRECTORY)
    if(DIR_PATH MATCHES "/Public$")
        list(APPEND EDITOR_PUBLIC_INCLUDE_DIRS "${DIR_PATH}")
    endif()
endforeach()

if(EDITOR_PUBLIC_INCLUDE_DIRS)
    list(REMOVE_DUPLICATES EDITOR_PUBLIC_INCLUDE_DIRS)
endif()

add_executable(Editor
    ${EDITOR_HEADERS}
    ${EDITOR_SOURCES}
)

add_reflection_bundle(
    TARGET Editor
    MODULE Editor
    SOURCE_ROOT "${EDITOR_SOURCE_DIR}"
    OUTPUT_CPP "${EDITOR_GENERATED_SOURCE}"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

target_include_directories(Editor
    PUBLIC
        ${EDITOR_PUBLIC_INCLUDE_DIRS}
    PRIVATE
        ${EDITOR_SOURCE_DIR}
)

target_link_libraries(Editor
    PRIVATE
        BamEngine::Engine
        Reflection::Core
)

set_target_properties(Editor PROPERTIES
    FOLDER "Applications"
)