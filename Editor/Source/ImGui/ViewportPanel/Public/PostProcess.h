#pragma once

#include "Editor_Includes.h"
#include "PostProcessPipeline.h"

BEGIN(Engine)
class Scene;
END

BEGIN(Editor)

struct ViewportPassInfo
{
	wstring Name;
	Engine::RenderPassID PassID = INVALID_PASS_ID;
	bool    Enabled = true;
};

class PostProcessChain
{
public:
	PostProcessChain() = default;
	~PostProcessChain() = default;

public:
	void Initialize(const wstring& prefix) { m_Pipeline.Initialize(prefix); }
    void Free();

	void PrepareResources(uint32 width, uint32 height) { m_Pipeline.PrepareResources(width, height); }
    EResult Resize(uint32 width, uint32 height) { return m_Pipeline.Resize(width, height); }

	// 입력 RT를 받아 최종 출력 RT 반환
	wstring ExecuteChain(f32 dt, const wstring& inputRT) { return m_Pipeline.Execute(dt, inputRT); }
	void DrawImGuiMenu();

	Engine::PostProcessPipeline& GetPipeline() { return m_Pipeline; }
	const Engine::PostProcessPipeline& GetPipeline() const { return m_Pipeline; }


public:
    struct SceneRestoreRequest
    {
        Engine::Scene* TargetScene = nullptr;
        entt::entity TargetEntity = entt::null;
    };

    void RequestSceneRestore(Engine::Scene& scene, entt::entity entity)
    {
        m_SceneRestoreRequest = SceneRestoreRequest{ &scene, entity };
        m_SceneRestoreResult.reset();
    }

    std::optional<SceneRestoreRequest> TakeSceneRestoreRequest()
    {
        const auto request = m_SceneRestoreRequest;
        m_SceneRestoreRequest.reset();
        return request;
    }

    void CompleteSceneRestore(bool restored)
    {
        m_SceneRestoreResult = restored;
    }

    std::optional<bool> TakeSceneRestoreResult()
    {
        const auto result = m_SceneRestoreResult;
        m_SceneRestoreResult.reset();
        return result;
    }

private:
	Engine::PostProcessPipeline m_Pipeline;
    std::optional<SceneRestoreRequest> m_SceneRestoreRequest;
    std::optional<bool> m_SceneRestoreResult;
};

END