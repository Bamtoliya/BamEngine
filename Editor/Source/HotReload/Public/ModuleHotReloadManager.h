#pragma once

#include "HotReloaderInterface.h"
#include "Module/ModuleInterface.h"
#include <efsw/efsw.hpp>

BEGIN(Editor)

enum class EHotReloadState : uint8 { Idle, Reloading, Failed };

class ModuleHotReloadManager final : public IHotReloader
{
#pragma region Constructor&Destructor
private:
    ModuleHotReloadManager() = default;
    virtual ~ModuleHotReloadManager() = default;
    EResult Initialize(void* arg = nullptr);
public:
    static ModuleHotReloadManager* Create(void* arg = nullptr);
    void Free() override;
#pragma endregion

public:
    void Update() override;

    EResult LoadModule(const std::filesystem::path& dllPath);
    EResult UnloadModule();
    EResult ReloadModule();

    [[nodiscard]] bool IsLoaded() const { return m_CurrentModule != nullptr; }
    [[nodiscard]] EHotReloadState GetState() const { return m_State; }
    [[nodiscard]] Engine::IModule* GetModule() const { return m_CurrentModule; }
    [[nodiscard]] uint32 GetReloadIndex() const { return m_ReloadIndex; }
    [[nodiscard]] const std::filesystem::path& GetOriginalDllPath() const { return m_OriginalDllPath; }

public:
    using PostReloadCallback = std::function<void()>;
    void RegisterPostReloadCallback(PostReloadCallback callback)
    {
        if (callback) m_PostReloadCallbacks.push_back(std::move(callback));
    }
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
    std::vector<PostReloadCallback> m_PostReloadCallbacks;
    // efsw 자동 감지 관련
    efsw::FileWatcher* m_FileWatcher = nullptr;
    efsw::FileWatchListener* m_FileListener = nullptr;
    efsw::WatchID m_WatchID = 0;
    std::string m_WatchedFileName;
};

END