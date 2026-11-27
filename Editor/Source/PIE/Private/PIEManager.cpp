#include "PIEManager.h"
#include "Application.h"
#include "SceneManager.h"
#include "SelectionManager.h"
#include "AudioManager.h"
#include "ImGuiManager.h"
#include <archive/JsonArchive.h>
#include "EditorSettingsService.h"
#include "InputActionSettings.h"
#include "InputManager.h"
#include <chrono>

IMPLEMENT_SINGLETON(PIEManager)

PIEManager::PIEManager() = default;
PIEManager::~PIEManager() = default;

void PIEManager::Free()
{
    m_Snapshot.reset();
}

void PIEManager::StartPlay()
{
    if (m_State != EPlayState::Edit) return;

    using Clock = std::chrono::steady_clock;
    const auto requested = Clock::now();

    //Input Settings
    {
        Engine::InputActionSettings inputSettings;
        if (!EditorSettingsService::Get().LoadInputActionSettings(inputSettings) ||
            !Engine::InputManager::Get().SetInputActionSettings(inputSettings))
        {
            return;
        }
    }

    const auto inputReady = Clock::now();
    const EResult snapResult = SnapshotScene();
    const auto snapshotReady = Clock::now();
    if (snapResult != EResult::Success)
    {
        EDITOR_LOG_ERROR("[PIE] Start failed: Could not snapshot scene.");
        return;
    }

    // 스냅샷 저장에 성공한 뒤 에디터의 프리뷰와 선택을 정리한다.
    Engine::AudioManager::Get().StopAllSounds();
    SelectionManager::Get().ClearSelection(true);
    m_State = EPlayState::Play;
    m_SingleStepRequested = false;
    if (auto* gameView = ImGuiManager::Get().GetImGuiPanel(L"Game View"))
    {
        gameView->Open();
        gameView->Focus();
    }
    const auto started = Clock::now();
    using Milliseconds = std::chrono::duration<double, std::milli>;
    EDITOR_LOG_INFO("[PIE] Started Play Mode: input={:.2f} ms, snapshot={:.2f} ms, transition={:.2f} ms",
        Milliseconds(inputReady - requested).count(), Milliseconds(snapshotReady - inputReady).count(),
        Milliseconds(started - snapshotReady).count());
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
    m_SingleStepRequested = false;
    EDITOR_LOG_INFO("[PIE] Resumed");
}
void PIEManager::RequestSingleStep()
{
    if (m_State != EPlayState::Pause) return;
    m_SingleStepRequested = true;
}
bool PIEManager::ConsumeSingleStepRequest()
{
    if (m_State != EPlayState::Pause || !m_SingleStepRequested)
        return false;
    m_SingleStepRequested = false;
    return true;
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
    m_SingleStepRequested = false;
    Application::Get().RebindSceneDocumentAfterPlay();

    // 에디터 복귀 시 씬 뷰포트(Scene View)로 포커스 복원
    IImGui* sceneView = ImGuiManager::Get().GetImGuiPanel(L"Scene View");
    if (!sceneView)
        sceneView = ImGuiManager::Get().GetImGuiPanel(L"Scene Viewport");
    if (sceneView)
    {
        sceneView->Focus();
    }

    EDITOR_LOG_INFO("[PIE] Stopped — scene restored");
}

void PIEManager::ForceStopForShutdown()
{
    if (m_State == EPlayState::Edit) return;

    Engine::AudioManager::Get().StopAllSounds();
    SelectionManager::Get().ClearSelection(true);
    m_Snapshot.reset();
    m_State = EPlayState::Edit;
    m_SingleStepRequested = false;
    EDITOR_LOG_WARN("[PIE] Force-stopped during application shutdown (scene restore skipped).");
}

EResult PIEManager::SnapshotScene()
{
    const auto* scene = SceneManager::Get().GetCurrentScene();
    if (!scene) return EResult::Fail;
    auto snapshot = std::make_unique<archive::JsonArchive>();
    const auto result = scene->WriteArchive(*snapshot);
    if (!result)
    {
        EDITOR_LOG_ERROR("[PIE] Scene snapshot failed: code={}, field={}", static_cast<int32>(result.Code), result.Field);
        return EResult::Fail;
    }
    m_Snapshot = std::move(snapshot);
    m_TerrainSnapshot = Engine::TerrainDataFile::CaptureMemory(scene->GetRegistry());
    return EResult::Success;
}
EResult PIEManager::RestoreScene()
{
    if (!m_Snapshot) return EResult::Fail;
    Scene* candidate = nullptr;
    const auto restored = Scene::CreateFromArchive(*m_Snapshot, candidate);
    if (!restored)
    {
        EDITOR_LOG_ERROR("[PIE] Scene restore failed: code={}, field={}", static_cast<int32>(restored.Code), restored.Field);
        return EResult::Fail;
    }
    // 아카이브에는 지형 데이터 경로만 있으므로, PIE 전 메모리 사본을 먼저 되돌려 저장된 파일로 덮어쓰이지 않게 한다.
    Engine::TerrainDataFile::RestoreMemory(candidate->GetRegistry(), std::move(m_TerrainSnapshot));
    m_TerrainSnapshot = {};
    const auto result = SceneManager::Get().OpenScene(candidate);
    Safe_Release(candidate);
    if (result == EResult::Success)
        m_Snapshot.reset();
    return result;
}
