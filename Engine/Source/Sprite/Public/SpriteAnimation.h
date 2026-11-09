#pragma once

#include "Resource.h"
#include "ResourceHandle.h"
#include "Sprite.h"
#include <vector>

BEGIN(Engine)

ENUM()
	enum class ESpriteAnimationPlayMode : uint8
{
	Loop = 0,    // 무한 반복 재생
	Once,        // 1회 재생 후 정지
	PingPong     // 끝에 도달하면 역방향으로 왕복 재생
};

STRUCT()
	struct ENGINE_API SpriteKeyFrame
{
	REFLECT_BODY()

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_SPRITE"))
		ResourceHandle<Sprite> sprite;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_FRAME_DURATION"))
		f32 frameDuration = 1.0f; // 기본 프레임 간격 대비 가중치 (1.0 = 표준 1프레임)
};

struct SpriteAnimationCreateDesc : public ResourceCreateDesc
{
	f32 framesPerSecond = 12.0f;
	ESpriteAnimationPlayMode playMode = ESpriteAnimationPlayMode::Loop;
	std::vector<SpriteKeyFrame> frames;
};

CLASS()
	class ENGINE_API SpriteAnimation final : public Resource
{
	REFLECT_CLASS()
		DECLARE_RESOURCE(SpriteAnimation)
	using DESC = SpriteAnimationCreateDesc;

private:
	SpriteAnimation() : Resource(EResourceType::SpriteAnimation) {}
	virtual ~SpriteAnimation() = default;
	EResult Initialize(void* arg = nullptr) override;

public:
	static SpriteAnimation* Create(void* arg = nullptr);
	virtual void Free() override;

public:
	// ── 프레임 시퀀스 관리 ──
	const std::vector<SpriteKeyFrame>& GetFrames() const { return m_Frames; }
	std::vector<SpriteKeyFrame>& GetFrames() { return m_Frames; }
	int32 GetFrameCount() const { return static_cast<int32>(m_Frames.size()); }
	const SpriteKeyFrame* GetFrame(int32 index) const;
	void AddFrame(const ResourceHandle<Sprite>& sprite, f32 duration = 1.0f);
	void InsertFrame(int32 index, const ResourceHandle<Sprite>& sprite, f32 duration = 1.0f);
	void RemoveFrame(int32 index);
	void ClearFrames();
	void SwapFrames(int32 indexA, int32 indexB);

	// ── 재생 설정 ──
	f32 GetFPS() const { return m_FramesPerSecond; }
	void SetFPS(f32 fps) { m_FramesPerSecond = (fps > 0.0f) ? fps : 12.0f; }

	ESpriteAnimationPlayMode GetPlayMode() const { return m_PlayMode; }
	void SetPlayMode(ESpriteAnimationPlayMode mode) { m_PlayMode = mode; }

	// ── 시간 평가 헬퍼 ──
	f32 GetTotalDuration() const;
	int32 GetFrameIndexAtTime(f32 time, bool loop = true) const;
	ResourceHandle<Sprite> GetSpriteAtTime(f32 time, bool loop = true) const;

	// ── 간편 파일 I/O (내부적으로 Resource::WriteArchive/ReadArchive 호출) ──
	bool SaveToFile(const std::wstring& filePath) const;
	bool LoadFromFile(const std::wstring& filePath);

private:
	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_FPS"))
		f32 m_FramesPerSecond = 12.0f;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_PLAY_MODE"))
		ESpriteAnimationPlayMode m_PlayMode = ESpriteAnimationPlayMode::Loop;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_FRAMES"))
		std::vector<SpriteKeyFrame> m_Frames;
};
END