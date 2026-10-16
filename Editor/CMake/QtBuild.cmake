find_package(Qt6 CONFIG REQUIRED COMPONENTS Widgets)

target_link_libraries(Editor PRIVATE Qt6::Widgets)
target_compile_definitions(Editor PRIVATE QT_NO_KEYWORDS)
set_target_properties(Editor PROPERTIES AUTOMOC ON)

set_source_files_properties(
    "${CMAKE_CURRENT_LIST_DIR}/../Source/Application/Private/Main.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../Source/Application/Private/Application.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../Source/Qt/Private/QtManager.cpp"
    PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON
)

if(MSVC)
    target_compile_options(Editor PRIVATE /utf-8)
endif()