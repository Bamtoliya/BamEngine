#pragma once

#include "Resource.h"

BEGIN(FMOD)
class Sound;
END

BEGIN(Engine)

struct AudioClipCreateDesc : public ResourceCreateDesc
{
	bool is3D = true;
	bool isStream = false;
	bool loop = false;
};

CLASS()
class ENGINE_API AudioClip final : public Resource
{
	REFLECT_CLASS()
	DECLARE_RESOURCE(AudioClip)
	using DESC = AudioClipCreateDesc;
private:
	AudioClip() : Resource(EResourceType::AudioClip) {}
	EResult Initialize(void* arg = nullptr) override;

public:
	virtual ~AudioClip() = default;
	static AudioClip* Create(void* arg = nullptr);
	virtual void Free() override;

public:
	FMOD::Sound* GetFMODSound() const { return m_Sound; }
	f32 GetDurationSeconds() const { return m_DurationSeconds; }
	uint32 GetLengthMs() const { return m_LengthMs; }
	bool Is3D() const { return m_Is3D; }
	bool IsStream() const { return m_IsStream; }
	bool IsLoop() const { return m_IsLoop; }

public:
	[[nodiscard]] archive::ArchiveResult WriteArchive(archive::IArchive& ar) const override;
	[[nodiscard]] archive::ArchiveResult ReadArchive(const archive::IArchive& ar) override;

private:
	[[nodiscard]] uint32 GetArchiveFormatVersion() const noexcept override { return 1; }

private:
	FMOD::Sound* m_Sound = { nullptr };

	PROPERTY()
		f32 m_DurationSeconds = { 0.f };

	PROPERTY()
		uint32 m_LengthMs = { 0 };

	PROPERTY()
		bool m_Is3D = { true };

	PROPERTY()
		bool m_IsStream = { false };

	PROPERTY()
		bool m_IsLoop = { false };
};

END