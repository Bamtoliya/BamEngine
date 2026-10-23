#include "PIEManager.h"
#include "Application.h"
#include "SceneManager.h"
#include "SelectionManager.h"
#include "AudioManager.h"
#include <archive/JsonArchive.h>

IMPLEMENT_SINGLETON(PIEManager)

void PIEManager::StartPlay()
{
    if (m_State != EPlayState::Edit) return;
    // 0. 에디터 프리뷰 사운드 정리
    Engine::AudioManager::Get().StopAllSounds();
    // 1. 선택 해제 및 씬 스냅샷 저장
    SelectionManager::Get().ClearSelection(true);
    const EResult snapResult = SnapshotScene();
    if (snapResult != EResult::Success)
    {
        EDITOR_LOG_ERROR("[PIE] Start failed: Could not snapshot scene.");
        return;
    }
    // 2. 플레이 상태로 전이
    m_State = EPlayState::Play;
    EDITOR_LOG_INFO("[PIE] Started Play Mode");
}

void PIEManager::Pause()
{
    if (m_State != EPlayState::Play) return;
    m_State = EPlayState::Pause;
    EDITOR_LOG_INFO("[PIE] Paused");
}

void PIEManager::Resume()
{
    if (m_State != EPlayState::Pause) return;
    m_State = EPlayState::Play;
    EDITOR_LOG_INFO("[PIE] Resumed");
}

void PIEManager::Stop()
{
    if (m_State == EPlayState::Edit) return;

    Engine::AudioManager::Get().StopAllSounds();
    SelectionManager::Get().ClearSelection(true);

    const EResult restoreResult = RestoreScene();
    if (restoreResult != EResult::Success)
    {
        EDITOR_LOG_WARN("[PIE] Stop aborted; PIE state is unchanged. Scene restore failed: {}",
            ResultToString(restoreResult));
        return;
    }

    m_State = EPlayState::Edit;
    Application::Get().GetEditHistory().Clear();
    EDITOR_LOG_INFO("[PIE] Stopped — scene restored");
}

EResult PIEManager::SnapshotScene()
{
    if (!SceneManager::Get().GetCurrentScene()) return EResult::Fail;

    std::filesystem::create_directories(std::filesystem::path(m_SnapshotPath).parent_path());
    archive::JsonArchive ar;
    const EResult result = SceneManager::Get().SaveScene(ar, m_SnapshotPath);
    if (result != EResult::Success)
        EDITOR_LOG_ERROR("[PIE] Scene snapshot failed: {}", ResultToString(result));
    return result;
}

EResult PIEManager::RestoreScene()
{
    if (!std::filesystem::exists(m_SnapshotPath)) return EResult::FileNotFound;

    archive::JsonArchive ar;
    const EResult result = SceneManager::Get().LoadScene(ar, m_SnapshotPath);
    if (result != EResult::Success)
        return result;

    std::error_code ec;
    std::filesystem::remove(m_SnapshotPath, ec);
    if (ec)
    {
        EDITOR_LOG_WARN("[PIE] Scene restored, but snapshot cleanup failed: {} ({})",
            WStrToStr(m_SnapshotPath), ec.message());
    }

    return EResult::Success;
}