#pragma once

#include "Reflection/ComponentAnnotations.h"
#include "ResourceHandle.h"
#include "AudioClip.h"

namespace FMOD
{
	class Channel;
}

namespace Engine
{
	class AudioSystem;

	STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_AUDIO_SOURCE"), CATEGORY("CATEGORY_AUDIO"))
	struct AudioSourceComponent
	{
	public:
		~AudioSourceComponent()
		{
			Stop();
		}
		REFLECT_BODY()

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_AUDIO_CLIP"))
		ResourceHandle<AudioClip> clip;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_VOLUME"))
		f32 volume = 1.f;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_PITCH"))
		f32 pitch = 1.f;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_LOOP"))
		bool loop = false;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_PLAY_ON_AWAKE"))
		bool playOnAwake = true;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_IS_3D"))
		bool is3D = true;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_MIN_DISTANCE"))
		f32 minDistance = 1.f;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_MAX_DISTANCE"))
		f32 maxDistance = 20.f;

	public:
		void Play() { m_PlayRequested = true; m_StopRequested = false; }
		void Stop() { m_StopRequested = true; m_PlayRequested = false; }
		void Pause() { m_PauseRequested = true; }
		void Resume() { m_ResumeRequested = true; }
		bool IsPlaying() const { return m_IsPlaying; }

	private:
		friend class AudioSystem;
		FMOD::Channel* m_Channel = { nullptr };
		bool m_IsPlaying = false;
		bool m_PlayRequested = false;
		bool m_StopRequested = false;
		bool m_PauseRequested = false;
		bool m_ResumeRequested = false;
		bool m_AwakeTriggered = false;
	};
}