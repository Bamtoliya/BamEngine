#pragma once

#include "PostProcessPipeline.h"

BEGIN(Engine)

class Scene;

class ENGINE_API ScenePostProcessSettings
{
public:
	bool CaptureDefaults(const PostProcessPipeline& pipeline);
	bool Apply(PostProcessPipeline& pipeline, Scene* scene);
	void Reset();

private:
	string m_DefaultSettings;
	string m_LastPreset;
	string m_LastSettings;
	entt::entity m_LastEntity = entt::null;
	bool m_Usable = true;
};

END
