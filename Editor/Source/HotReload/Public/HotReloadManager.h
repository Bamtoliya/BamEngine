#pragma once

#include "Editor_Includes.h"
#include "Module/ModuleInterface.h"
#include <efsw/efsw.hpp>
#include <filesystem>
#include <atomic>
#include <chrono>

BEGIN(Editor)

enum class EHotReloadState : uint8 { Idle, Reloading, Failed };

class HotReloadManager final : public Base
{
    DECLARE_SINGLETON(HotReloadManager)

private:
    HotReloadManager() = default;
    virtual ~HotReloadManager() = default;
    EResult Initialize(void* arg = nullptr);

public:
    void Free() override;

    // 매 프레임 메인 스레드에서 자동 리로드 큐 검사
    void Update();

    EResult LoadModule(const std::filesystem::path& dllPath);
    EResult UnloadModule();
    EResult ReloadModule();

    [[nodiscard]] bool IsLoaded() const { return m_CurrentModule != nullptr; }
    [[nodiscard]] EHotReloadState GetState() const { return m_State; }
    [[nodiscard]] Engine::IModule* GetModule() const { return m_CurrentModule; }
    [[nodiscard]] const std::filesystem::path& GetOriginalDllPath() const { return m_OriginalDllPath; }

    [[nodiscard]] bool IsAutoReloadEnabled() const { return m_AutoReload; }
    void SetAutoReload(bool enable) { m_AutoReload = enable; }
    [[nodiscard]] bool& GetAutoReloadRef() { return m_AutoReload; }

    void NotifyFileModified();

private:
    std::filesystem::path CreateShadowCopy(const std::filesystem::path& srcDllPath);
    void CleanupShadowCopies();
    void StartWatching(const std::filesystem::path& dllPath);
    void StopWatching();

private:
    EHotReloadState m_State = EHotReloadState::Idle;
    std::filesystem::path m_OriginalDllPath;
    std::filesystem::path m_CurrentShadowDllPath;
    std::filesystem::path m_ShadowDirectory = L"Temp/HotReload";

    HMODULE m_ModuleHandle = nullptr;
    Engine::IModule* m_CurrentModule = nullptr;
    DestroyModuleFunc m_DestroyFunc = nullptr;
    uint32 m_ReloadIndex = 0;

    // efsw 자동 감지 관련
    efsw::FileWatcher* m_FileWatcher = nullptr;
    efsw::FileWatchListener* m_FileListener = nullptr;
    efsw::WatchID m_WatchID = 0;
    std::string m_WatchedFileName;
    bool m_AutoReload = true;
    std::atomic<bool> m_PendingAutoReload = false;
    std::chrono::steady_clock::time_point m_LastModifiedTime;
};

END