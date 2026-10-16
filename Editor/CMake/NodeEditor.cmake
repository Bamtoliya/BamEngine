include(FetchContent)

# ImGui 1.92 support is newer than imnodes' last release tag. Pin the upstream source.
set(IMNODES_EXAMPLES OFF CACHE BOOL "Build imnodes examples" FORCE)
set(IMNODES_IMGUI_TARGET_NAME imgui::imgui)
FetchContent_Declare(bam_imnodes
    GIT_REPOSITORY https://github.com/Nelarius/imnodes.git
    GIT_TAG eb36902c892548ef94f88f51ad7e7c9c7058a71c
    GIT_SUBMODULES ""
)
FetchContent_MakeAvailable(bam_imnodes)
set_target_properties(imnodes PROPERTIES UNITY_BUILD OFF FOLDER "ThirdParty")

# Keep upstream warnings visible without making a third-party /WX policy mandatory.
if(MSVC)
    target_compile_options(imnodes PRIVATE /WX-)
endif()
