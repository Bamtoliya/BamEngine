#pragma once

#include "Editor_Includes.h"
#include <memory>

namespace archive { class JsonArchive; }

BEGIN(Editor)

enum class EPlayState : uint8 { Edit, Play, Pause };

class PIEManager final : public Base
{
    DECLARE_SINGLETON(PIEManager)

private:
    PIEManager();
    ~PIEManager() override;
	EResult Initialize(void* arg = nullptr) { return EResult::Success; }
public:
    void Free() override;

    void StartPlay();
    void Pause();
    void Resume();
    void Stop();

    [[nodiscard]] EPlayState GetState() const { return m_State; }
    [[nodiscard]] bool IsPlaying() const { return m_State == EPlayState::Play; }
    [[nodiscard]] bool IsPaused() const { return m_State == EPlayState::Pause; }
    [[nodiscard]] bool IsEditing() const { return m_State == EPlayState::Edit; }
    [[nodiscard]] bool CanEdit() const { return m_State == EPlayState::Edit || m_State == EPlayState::Pause; }

private:
    EResult SnapshotScene();
    EResult RestoreScene();

private:
    EPlayState m_State = EPlayState::Edit;
    std::unique_ptr<archive::JsonArchive> m_Snapshot;
};

END
