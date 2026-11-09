#include "SpriteAnimation.h"
#include <archive/JsonArchive.h>
#include <algorithm>

EResult SpriteAnimation::Initialize(void* arg)
{
	if (arg)
	{
		auto* desc = static_cast<DESC*>(arg);
		m_FramesPerSecond = (desc->framesPerSecond > 0.0f) ? desc->framesPerSecond : 12.0f;
		m_PlayMode = desc->playMode;
		m_Frames = desc->frames;
	}
	return EResult::Success;
}

SpriteAnimation* SpriteAnimation::Create(void* arg)
{
	auto* instance = new SpriteAnimation();
	if (instance->Initialize(arg) != EResult::Success)
	{
		delete instance;
		return nullptr;
	}
	return instance;
}

void SpriteAnimation::Free()
{
	m_Frames.clear();
}

const SpriteKeyFrame* SpriteAnimation::GetFrame(int32 index) const
{
	if (index < 0 || index >= static_cast<int32>(m_Frames.size())) return nullptr;
	return &m_Frames[index];
}

void SpriteAnimation::AddFrame(const ResourceHandle<Sprite>& sprite, f32 duration)
{
	m_Frames.push_back({ sprite, std::max(duration, 0.01f) });
}

void SpriteAnimation::InsertFrame(int32 index, const ResourceHandle<Sprite>& sprite, f32 duration)
{
	const int32 clampedIndex = std::clamp(index, 0, static_cast<int32>(m_Frames.size()));
	m_Frames.insert(m_Frames.begin() + clampedIndex, { sprite, std::max(duration, 0.01f) });
}

void SpriteAnimation::RemoveFrame(int32 index)
{
	if (index >= 0 && index < static_cast<int32>(m_Frames.size()))
		m_Frames.erase(m_Frames.begin() + index);
}

void SpriteAnimation::ClearFrames()
{
	m_Frames.clear();
}

void SpriteAnimation::SwapFrames(int32 indexA, int32 indexB)
{
	const int32 count = static_cast<int32>(m_Frames.size());
	if (indexA >= 0 && indexA < count && indexB >= 0 && indexB < count && indexA != indexB)
		std::swap(m_Frames[indexA], m_Frames[indexB]);
}

f32 SpriteAnimation::GetTotalDuration() const
{
	if (m_Frames.empty() || m_FramesPerSecond <= 0.0f) return 0.0f;
	const f32 baseFrameTime = 1.0f / m_FramesPerSecond;
	f32 total = 0.0f;
	for (const auto& frame : m_Frames)
		total += baseFrameTime * frame.frameDuration;
	return total;
}

int32 SpriteAnimation::GetFrameIndexAtTime(f32 time, bool loop) const
{
	const int32 count = static_cast<int32>(m_Frames.size());
	if (count == 0) return -1;
	if (count == 1) return 0;

	const f32 total = GetTotalDuration();
	if (total <= 0.0f) return 0;

	if (loop)
	{
		time = std::fmod(time, total);
		if (time < 0.0f) time += total;
	}
	else
	{
		if (time <= 0.0f) return 0;
		if (time >= total) return count - 1;
	}

	const f32 baseFrameTime = 1.0f / m_FramesPerSecond;
	f32 accumulated = 0.0f;
	for (int32 i = 0; i < count; ++i)
	{
		accumulated += baseFrameTime * m_Frames[i].frameDuration;
		if (time < accumulated)
			return i;
	}
	return count - 1;
}

ResourceHandle<Sprite> SpriteAnimation::GetSpriteAtTime(f32 time, bool loop) const
{
	const int32 index = GetFrameIndexAtTime(time, loop);
	if (index >= 0 && index < static_cast<int32>(m_Frames.size()))
		return m_Frames[index].sprite;
	return ResourceHandle<Sprite>();
}

bool SpriteAnimation::SaveToFile(const std::wstring& filePath) const
{
	archive::JsonArchive ar;
	if (!WriteArchive(ar)) return false;
	return static_cast<bool>(ar.SaveToFile(filePath));
}

bool SpriteAnimation::LoadFromFile(const std::wstring& filePath)
{
	archive::JsonArchive ar;
	if (!ar.LoadFromFile(filePath)) return false;
	return static_cast<bool>(ReadArchive(ar));
}