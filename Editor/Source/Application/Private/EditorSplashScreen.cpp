#include "EditorSplashScreen.h"
#include "Functions.h"
#include <windows.h>
#include <algorithm>


bool EditorSplashScreen::Show(const char* title, int width, int height)
{
    if (s_Window) return true;
    s_Width = width;
    s_Height = height;

    // 테두리 없는 항상 위(Always-on-top) 윈도우 생성
    SDL_WindowFlags flags = SDL_WINDOW_BORDERLESS | SDL_WINDOW_ALWAYS_ON_TOP;
    s_Window = SDL_CreateWindow(title, s_Width, s_Height, flags);
    if (!s_Window) return false;

    SDL_SetWindowPosition(s_Window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    UpdateProgress(0.0f, "Starting Editor...");
    return true;
}

void EditorSplashScreen::UpdateProgress(float progress, const std::string& statusText)
{
    if (!s_Window) return;
    progress = std::clamp(progress, 0.0f, 1.0f);

    // 엔진 공용 유틸리티 Engine::StrToWStr 활용
    RenderFrame(progress, Engine::Unicode::Utf8ToWide(statusText));

    // OS 메시지 큐를 비워 윈도우 '응답 없음'을 방지하고 화면을 즉시 갱신
    SDL_PumpEvents();
}

void EditorSplashScreen::Hide()
{
    if (s_Window)
    {
        SDL_DestroyWindow(s_Window);
        s_Window = nullptr;
    }
}

void EditorSplashScreen::RenderFrame(float progress, const std::wstring& wideStatus)
{
    HWND hwnd = static_cast<HWND>(SDL_GetPointerProperty(SDL_GetWindowProperties(s_Window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
    if (!hwnd) return;

    HDC hdc = GetDC(hwnd);
    if (!hdc) return;

    // 더블 버퍼링: 깜빡임 방지용 메모리 DC 생성
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, s_Width, s_Height);
    HBITMAP oldBmp = static_cast<HBITMAP>(SelectObject(memDC, memBmp));

    // 1. 다크 테마 배경 채우기 (#1E1E20)
    RECT fullRect = { 0, 0, s_Width, s_Height };
    HBRUSH bgBrush = CreateSolidBrush(RGB(30, 30, 32));
    FillRect(memDC, &fullRect, bgBrush);
    DeleteObject(bgBrush);

    // 2. 외곽 테두리 선 그리기 (#3C3C44)
    HBRUSH borderBrush = CreateSolidBrush(RGB(60, 60, 68));
    FrameRect(memDC, &fullRect, borderBrush);
    DeleteObject(borderBrush);

    SetBkMode(memDC, TRANSPARENT);

    // 3. 타이틀 텍스트 ("BamEngine Editor")
    HFONT titleFont = CreateFontW(26, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    HFONT oldFont = static_cast<HFONT>(SelectObject(memDC, titleFont));
    SetTextColor(memDC, RGB(245, 245, 250));
    RECT titleRect = { 32, 38, s_Width - 32, 75 };
    DrawTextW(memDC, L"BamEngine Editor", -1, &titleRect, DT_LEFT | DT_SINGLELINE);

    // 4. 서브 타이틀 텍스트
    HFONT subFont = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    SelectObject(memDC, subFont);
    SetTextColor(memDC, RGB(130, 130, 145));
    RECT subRect = { 32, 74, s_Width - 32, 98 };
    DrawTextW(memDC, L"v0.1.0 Developer Preview", -1, &subRect, DT_LEFT | DT_SINGLELINE);

    // 5. 현재 로딩 상태 문구 (한글 폰트)
    HFONT statusFont = CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"맑은 고딕");
    SelectObject(memDC, statusFont);
    SetTextColor(memDC, RGB(200, 200, 210));
    RECT statusRect = { 32, s_Height - 84, s_Width - 100, s_Height - 60 };
    DrawTextW(memDC, wideStatus.c_str(), -1, &statusRect, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    // 6. 진행률 퍼센트 텍스트 (우측 정렬)
    std::wstring pctStr = std::to_wstring(static_cast<int>(progress * 100.0f)) + L"%";
    RECT pctRect = { s_Width - 90, s_Height - 84, s_Width - 32, s_Height - 60 };
    DrawTextW(memDC, pctStr.c_str(), -1, &pctRect, DT_RIGHT | DT_SINGLELINE | DT_VCENTER);

    // 7. 프로그레스 바 트랙 (배경)
    const int barX = 32;
    const int barY = s_Height - 50;
    const int barW = s_Width - 64;
    const int barH = 7;
    RECT trackRect = { barX, barY, barX + barW, barY + barH };
    HBRUSH trackBrush = CreateSolidBrush(RGB(40, 40, 44));
    FillRect(memDC, &trackRect, trackBrush);
    DeleteObject(trackBrush);

    // 8. 프로그레스 바 채우기 (VS 블루 테마 #007ACC)
    int fillW = static_cast<int>(barW * progress);
    if (fillW > 0)
    {
        RECT fillRect = { barX, barY, barX + fillW, barY + barH };
        HBRUSH fillBrush = CreateSolidBrush(RGB(0, 122, 204));
        FillRect(memDC, &fillRect, fillBrush);
        DeleteObject(fillBrush);
    }

    // 백버퍼를 화면에 고속 전송 (BitBlt)
    BitBlt(hdc, 0, 0, s_Width, s_Height, memDC, 0, 0, SRCCOPY);

    // GDI 핸들 해제
    SelectObject(memDC, oldFont);
    DeleteObject(titleFont);
    DeleteObject(subFont);
    DeleteObject(statusFont);
    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);
    ReleaseDC(hwnd, hdc);
}