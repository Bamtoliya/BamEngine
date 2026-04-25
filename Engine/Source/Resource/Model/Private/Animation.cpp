#include "Animation.h"

EResult Animation::Initialize(void* arg)
{
	TODO("Animation Initialize Implement");
	return EResult::Success;
}

Animation* Animation::Create(void* arg)
{
	Animation* instance = new Animation();
	if (IsFailure(instance->Initialize(arg)))
	{
		instance->Free();
		delete instance;
		return nullptr;
	}
	return instance;
}

void Animation::Free()
{
	TODO("Animation Free Implement");
	m_Tracks.clear();
	m_Duration = 0.0;
	m_TicksPerSecond = 0.0;
}
