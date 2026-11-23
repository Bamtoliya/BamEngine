#include "ScenePostProcessSettings.h"
#include "ComponentEnable.h"

#include "PlatformPath.h"
#include "PostProcessComponent.h"
#include "Scene.h"

bool Engine::ScenePostProcessSettings::CaptureDefaults(const PostProcessPipeline& pipeline)
{
	Reset();
	return pipeline.SaveToString(m_DefaultSettings);
}

bool Engine::ScenePostProcessSettings::Apply(PostProcessPipeline& pipeline, Scene* scene)
{
	// 다른 씬의 자동 노출 등 이전 프레임 값을 이어 쓰지 않는다.
	pipeline.SetHistoryScene(scene);
	if (scene)
	{
		auto& world = scene->GetRegistry();
		for (auto [entity, source] : world.view<PostProcessComponent>().each())
		{
			if (!IsComponentEnabled(world, entity, source))
				continue;
			if (m_LastEntity == entity && m_LastPreset == source.PresetName && m_LastSettings == source.PipelineSettings)
				return m_Usable;

			m_LastEntity = entity;
			m_LastPreset = source.PresetName;
			m_LastSettings = source.PipelineSettings;
			m_Usable = pipeline.LoadFromString(m_DefaultSettings);
			if (m_Usable && !source.PipelineSettings.empty())
				m_Usable = pipeline.LoadFromString(source.PipelineSettings);
			else if (m_Usable && !source.PresetName.empty())
			{
				const auto preset = Utf8ToPath("Resources/Config/PostProcess/Presets/" + source.PresetName + ".json");
				m_Usable = pipeline.LoadFromFile(PlatformPath::ResolveUri(preset).wstring());
			}

			if (!m_Usable)
				ENGINE_LOG_ERROR("Failed to apply scene post-processing settings for preset '{}'.", source.PresetName);
			return m_Usable;
		}
	}

	if (m_LastEntity != entt::null)
	{
		m_LastEntity = entt::null;
		m_LastPreset.clear();
		m_LastSettings.clear();
		m_Usable = pipeline.LoadFromString(m_DefaultSettings);
	}
	return m_Usable;
}

void Engine::ScenePostProcessSettings::Reset()
{
	m_DefaultSettings.clear();
	m_LastPreset.clear();
	m_LastSettings.clear();
	m_LastEntity = entt::null;
	m_Usable = true;
}
