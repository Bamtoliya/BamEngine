#pragma once
#include "Editor_Includes.h"
#include <string>
#include <SDL3/SDL.h>


BEGIN(Editor)
class EditorSplashScreen
{
public:
    // 스플래시 창을 화면 중앙에 생성합니다.
    static bool Show(const char* title = "BamEngine Editor", int width = 520, int height = 280);

    // 진행률(0.0f ~ 1.0f)과 상태 안내 문구를 갱신합니다.
    static void UpdateProgress(float progress, const std::string& statusText);

    // 스플래시 창을 닫고 리소스를 해제합니다.
    static void Hide();

    static bool IsShowing() { return s_Window != nullptr; }

private:
    static void RenderFrame(float progress, const std::wstring& wideStatus);

private:
    inline static SDL_Window* s_Window = nullptr;
    inline static int s_Width = 520;
    inline static int s_Height = 280;
};
END