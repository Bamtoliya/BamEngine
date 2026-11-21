#pragma once

#if defined(_WIN32)

#include "ClientStartupStage.h"

#include <windows.h>
#include <atomic>
#include <thread>

namespace Client
{
    class ClientStartupSplash final
    {
    public:
        ClientStartupSplash() = default;
        ClientStartupSplash(const ClientStartupSplash&) = delete;
        ClientStartupSplash& operator=(const ClientStartupSplash&) = delete;
        ~ClientStartupSplash();

        bool Show() noexcept;
        void SetStage(ClientStartupStage stage) noexcept;
        void SetBounds(HWND gameWindow) noexcept;
        void Hide() noexcept;

    private:
        void Run() noexcept;
        static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);

        std::thread m_Thread;
        HANDLE m_ReadyEvent = nullptr;
        HANDLE m_StopEvent = nullptr;
        std::atomic<HWND> m_Window{ nullptr };
        std::atomic<ClientStartupStage> m_Stage{ ClientStartupStage::Starting };
    };
}

#endif
