include(FetchContent)

# ------------------------------------------------------------------------------
# imgui-node-editor (Thedmd)
# ------------------------------------------------------------------------------
FetchContent_Declare(bam_imgui_node_editor
    GIT_REPOSITORY https://github.com/thedmd/imgui-node-editor.git
    GIT_TAG        develop
    GIT_SUBMODULES ""
)
FetchContent_MakeAvailable(bam_imgui_node_editor)

# imgui-node-editor 자체 타깃 또는 소스 파일 라이브러리 구성
if(TARGET imgui-node-editor)
    set_target_properties(imgui-node-editor PROPERTIES FOLDER "ThirdParty")
    target_link_libraries(imgui-node-editor PUBLIC imgui::imgui)
else()
    # 소스 기반 수동 타깃 생성 (예제 빌드 방지 및 ImGui 바인딩)
    file(GLOB NODE_EDITOR_SOURCES
        "${bam_imgui_node_editor_SOURCE_DIR}/*.cpp"
        "${bam_imgui_node_editor_SOURCE_DIR}/*.h"
        "${bam_imgui_node_editor_SOURCE_DIR}/*.inl"
    )
    add_library(imgui-node-editor STATIC ${NODE_EDITOR_SOURCES})
    target_include_directories(imgui-node-editor PUBLIC "${bam_imgui_node_editor_SOURCE_DIR}")
    target_link_libraries(imgui-node-editor PUBLIC imgui::imgui)
    set_target_properties(imgui-node-editor PROPERTIES FOLDER "ThirdParty")
endif()

if(MSVC)
    target_compile_options(imgui-node-editor PRIVATE /WX-)
endif()