#pragma once

#include "HotReloaderInterface.h"
#include <efsw/efsw.hpp>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>

BEGIN(Editor)

enum class EModuleBuildStatus : uint8 { Idle, Compiling, Succeeded, Failed };

class ModuleBuildManager final : public IHotReloader
{
#pragma region Constructor&Destructor
private:
    ModuleBuildManager() = default;
    virtual ~ModuleBuildManager() = default;
    EResult Initialize(const std::filesystem::path& projectRoot, const std::filesystem::path& buildDir);
public:
    static ModuleBuildManager* Create(const std::filesystem::path& projectRoot, const std::filesystem::path& buildDir);
    void Free() override;
#pragma endregion

public:
    void Update() override;

    // 수동 빌드 요청 (단축키 또는 버튼 클릭 시)
    bool RequestBuild();

    // IHotReloader의 자동 갱신 메서드를 빌더 명칭에 맞게 연결
    [[nodiscard]] bool IsAutoCompileEnabled() const { return IsAutoReloadEnabled(); }
    void SetAutoCompileEnabled(bool enable) { SetAutoReload(enable); }

    [[nodiscard]] EModuleBuildStatus GetStatus() const { return m_Status; }
    [[nodiscard]] bool IsCompiling() const { return m_Status == EModuleBuildStatus::Compiling; }
    [[nodiscard]] const std::string& GetLastBuildOutput() const { return m_LastBuildOutput; }
    [[nodiscard]] const std::filesystem::path& GetSourceDirectory() const { return m_SourceDir; }

    void SetBuildCompletedCallback(std::function<void(bool success, const std::string& output)> callback)
    {
        m_OnBuildCompleted = std::move(callback);
    }

private:
    void StartWatching(const std::filesystem::path& sourceDir);
    void StopWatching();
    void ExecuteBuildAsync();

    friend class ModuleSourceFileListener;

private:
    std::filesystem::path m_ProjectRoot;
    std::filesystem::path m_BuildDir;
    std::filesystem::path m_SourceDir;

    std::atomic<EModuleBuildStatus> m_Status = EModuleBuildStatus::Idle;

    // 비동기 빌드 스레드 및 결과
    std::thread m_BuildThread;
    std::string m_LastBuildOutput;
    std::mutex m_OutputMutex;
    std::atomic<bool> m_HasNewResult = false;
    bool m_LastBuildSuccess = false;

    std::function<void(bool success, const std::string& output)> m_OnBuildCompleted;

    // efsw 파일 감시자
    efsw::FileWatcher* m_FileWatcher = nullptr;
    efsw::FileWatchListener* m_FileListener = nullptr;
    efsw::WatchID m_WatchID = 0;
};

using ModuleBuilder = ModuleBuildManager;

END