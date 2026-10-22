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
    // 0. 플레이 중이던 모든 런타임 사운드 채널 즉시 정지
    Engine::AudioManager::Get().StopAllSounds();
    m_State = EPlayState::Edit;
    // 1. 선택 해제
    SelectionManager::Get().ClearSelection(true);
    // 2. 스냅샷으로부터 씬 복원
    const EResult restoreResult = RestoreScene();
    if (restoreResult != EResult::Success)
    {
        EDITOR_LOG_WARN("[PIE] Stopped without restoring scene: {}", ResultToString(restoreResult));
        return;
    }
    // 3. 에디트 히스토리(Undo/Redo) 초기화
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

    // 복원 후 사용 완료된 임시 스냅샷 파일 삭제
    std::error_code ec;
    std::filesystem::remove(m_SnapshotPath, ec);
    return result;
}