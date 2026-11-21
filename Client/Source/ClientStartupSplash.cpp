#include "ClientStartupSplash.h"

#if defined(_WIN32)

#include <algorithm>

namespace
{
    constexpr wchar_t WindowClassName[] = L"BamEngine.Client.StartupSplash";
    constexpr UINT_PTR AnimationTimer = 1;
    constexpr COLORREF Background = RGB(16, 20, 28);
    constexpr COLORREF Track = RGB(41, 49, 62);
    constexpr COLORREF Accent = RGB(242, 163, 83);

    RECT StartupBounds()
    {
        POINT cursor{};
        GetCursorPos(&cursor);
        MONITORINFO monitor{};
        monitor.cbSize = sizeof(monitor);
        if (!GetMonitorInfoW(MonitorFromPoint(cursor, MONITOR_DEFAULTTOPRIMARY), &monitor))
            return { 0, 0, 960, 540 };

        const auto& work = monitor.rcWork;
        const LONG width = (std::min)(1280L, work.right - work.left);
        const LONG height = (std::min)(720L, work.bottom - work.top);
        const LONG left = work.left + (work.right - work.left - width) / 2;
        const LONG top = work.top + (work.bottom - work.top - height) / 2;
        return { left, top, left + width, top + height };
    }

    void Fill(HDC dc, RECT bounds, COLORREF color)
    {
        SetDCBrushColor(dc, color);
        FillRect(dc, &bounds, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
    }

    void DrawSplash(HDC dc, RECT bounds, Client::ClientStartupStage stage)
    {
        const LONG width = bounds.right;
        const LONG height = bounds.bottom;
        Fill(dc, bounds, Background);

        const float scale = std::clamp((std::min)(width / 960.0f, height / 540.0f), 0.45f, 1.4f);
        const auto px = [scale](int logical) { return static_cast<LONG>(logical * scale); };
        const LONG cx = width / 2;
        const LONG cy = height / 2;

        // The mark suggests a B formed by two blocks, with an offset cap above the stem.
        const LONG markX = cx - px(26);
        const LONG markY = cy - px(82);
        Fill(dc, { markX, markY, markX + px(12), markY + px(51) }, Accent);
        Fill(dc, { markX + px(17), markY, markX + px(45), markY + px(22) }, RGB(255, 192, 115));
        Fill(dc, { markX + px(17), markY + px(28), markX + px(52), markY + px(51) }, Accent);

        const HFONT font = CreateFontW(-px(50), 0, 0, 0, FW_DEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        const HGDIOBJ oldFont = font ? SelectObject(dc, font) : nullptr;
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(239, 242, 249));
        RECT title{ 0, cy - px(15), width, cy + px(47) };
        DrawTextW(dc, L"BamEngine", -1, &title, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        if (oldFont && oldFont != HGDI_ERROR)
            SelectObject(dc, oldFont);
        if (font)
            DeleteObject(font);

        constexpr LONG segmentCount = 5;
        const auto currentSegment = static_cast<LONG>(stage);
        const LONG segmentWidth = px(28);
        const LONG gap = px(5);
        const LONG barX = cx - (segmentCount * segmentWidth + (segmentCount - 1) * gap) / 2;
        const LONG barY = cy + px(80);
        const auto step = static_cast<LONG>(GetTickCount64() % 700ULL);
        for (LONG i = 0; i < segmentCount; ++i)
        {
            const LONG x = barX + i * (segmentWidth + gap);
            const RECT segment{ x, barY, x + segmentWidth, barY + px(4) };
            Fill(dc, segment, i < currentSegment ? Accent : Track);
            if (i == currentSegment)
            {
                const LONG pulseWidth = segmentWidth / 2;
                const LONG pulseX = x + (segmentWidth - pulseWidth) * (step <= 350 ? step : 700 - step) / 350;
                Fill(dc, { pulseX, segment.top, pulseX + pulseWidth, segment.bottom }, Accent);
            }
        }
    }

    void PaintSplash(HWND window, Client::ClientStartupStage stage)
    {
        PAINTSTRUCT paint{};
        const HDC target = BeginPaint(window, &paint);
        if (!target)
            return;

        RECT bounds{};
        if (GetClientRect(window, &bounds) && bounds.right > 0 && bounds.bottom > 0)
        {
            const HDC buffer = CreateCompatibleDC(target);
            const HBITMAP bitmap = buffer ? CreateCompatibleBitmap(target, bounds.right, bounds.bottom) : nullptr;
            const HGDIOBJ oldBitmap = bitmap ? SelectObject(buffer, bitmap) : nullptr;
            if (oldBitmap && oldBitmap != HGDI_ERROR)
            {
                DrawSplash(buffer, bounds, stage);
                BitBlt(target, 0, 0, bounds.right, bounds.bottom, buffer, 0, 0, SRCCOPY);
                SelectObject(buffer, oldBitmap);
            }
            else
            {
                DrawSplash(target, bounds, stage);
            }
            if (bitmap)
                DeleteObject(bitmap);
            if (buffer)
                DeleteDC(buffer);
        }
        EndPaint(window, &paint);
    }
}

Client::ClientStartupSplash::~ClientStartupSplash()
{
    Hide();
}

bool Client::ClientStartupSplash::Show() noexcept
{
    if (m_Thread.joinable())
        return m_Window.load() != nullptr;

    m_Stage.store(ClientStartupStage::Starting);
    m_ReadyEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    m_StopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (m_ReadyEvent && m_StopEvent)
    {
        try
        {
            m_Thread = std::thread(&ClientStartupSplash::Run, this);
        }
        catch (...)
        {
            Hide();
            return false;
        }
        if (WaitForSingleObject(m_ReadyEvent, INFINITE) == WAIT_OBJECT_0 && m_Window.load())
            return true;
    }

    Hide();
    return false;
}

void Client::ClientStartupSplash::SetStage(ClientStartupStage stage) noexcept
{
    m_Stage.store(stage);
    if (const HWND window = m_Window.load())
        InvalidateRect(window, nullptr, FALSE);
}

void Client::ClientStartupSplash::SetBounds(HWND gameWindow) noexcept
{
    RECT bounds{};
    if (const HWND window = m_Window.load(); window && gameWindow && GetWindowRect(gameWindow, &bounds))
    {
        SetWindowPos(window, HWND_TOPMOST, bounds.left, bounds.top, bounds.right - bounds.left, bounds.bottom - bounds.top,
            SWP_ASYNCWINDOWPOS | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
    }
}

void Client::ClientStartupSplash::Hide() noexcept
{
    if (m_Thread.joinable())
    {
        SetEvent(m_StopEvent);
        m_Thread.join();
    }
    if (m_StopEvent)
    {
        CloseHandle(m_StopEvent);
        m_StopEvent = nullptr;
    }
    if (m_ReadyEvent)
    {
        CloseHandle(m_ReadyEvent);
        m_ReadyEvent = nullptr;
    }
}

void Client::ClientStartupSplash::Run() noexcept
{
    const auto instance = GetModuleHandleW(nullptr);
    WNDCLASSW splashClass{};
    splashClass.lpfnWndProc = &WindowProc;
    splashClass.hInstance = instance;
    splashClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    splashClass.lpszClassName = WindowClassName;
    if (!RegisterClassW(&splashClass))
    {
        SetEvent(m_ReadyEvent);
        return;
    }

    const RECT bounds = StartupBounds();
    const HWND window = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
        WindowClassName, L"BamEngine", WS_POPUP, bounds.left, bounds.top, bounds.right - bounds.left,
        bounds.bottom - bounds.top, nullptr, nullptr, instance, this);
    if (window)
    {
        ShowWindow(window, SW_SHOWNOACTIVATE);
        UpdateWindow(window);
        SetTimer(window, AnimationTimer, 60, nullptr);
        m_Window.store(window);
    }
    SetEvent(m_ReadyEvent);

    if (window)
    {
        while (MsgWaitForMultipleObjects(1, &m_StopEvent, FALSE, INFINITE, QS_ALLINPUT) == WAIT_OBJECT_0 + 1)
        {
            MSG message{};
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
            {
                if (message.message == WM_QUIT)
                {
                    SetEvent(m_StopEvent);
                    break;
                }
                DispatchMessageW(&message);
            }
        }
        m_Window.store(nullptr);
        KillTimer(window, AnimationTimer);
        DestroyWindow(window);
    }
    UnregisterClassW(WindowClassName, instance);
}

LRESULT CALLBACK Client::ClientStartupSplash::WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message == WM_NCCREATE)
    {
        const auto* creation = reinterpret_cast<const CREATESTRUCTW*>(lparam);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(creation->lpCreateParams));
        return TRUE;
    }

    switch (message)
    {
    case WM_ERASEBKGND:
        return 1;
    case WM_CLOSE:
        return 0;
    case WM_TIMER:
    case WM_SIZE:
        InvalidateRect(window, nullptr, FALSE);
        return 0;
    case WM_PAINT:
        if (auto* splash = reinterpret_cast<ClientStartupSplash*>(GetWindowLongPtrW(window, GWLP_USERDATA)))
        {
            PaintSplash(window, splash->m_Stage.load());
            return 0;
        }
        break;
    default:
        break;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

#endif
