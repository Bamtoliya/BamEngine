#pragma once

#include "ResourceEditorInterface.h"
#include "SpriteAnimation.h"
#include "ResourceHandle.h"

BEGIN(Editor)

class SpriteAnimationEditor final : public ResourceEditorInterface
{
public:
	SpriteAnimationEditor();
	virtual ~SpriteAnimationEditor() = default;

public:
	virtual bool IsSupported(const std::filesystem::path& assetPath) const override;
	virtual void SetTargetResource(const std::filesystem::path& assetPath) override;
	virtual void Update(f32 dt) override;
	virtual void Draw() override;

private:
	void LoadAnimation(const std::filesystem::path& path);
	void SaveAnimation();
	Engine::SpriteAnimation* GetAnimation() const { return m_AnimationHandle.Get(); }

	void DrawToolbar();
	void DrawPreviewCanvas(f32 availWidth, f32 availHeight);
	void DrawFramesList(f32 availWidth);
	void DrawFilmstrip(f32 height);

private:
	Engine::ResourceHandle<Engine::SpriteAnimation> m_AnimationHandle;
	bool m_IsPlaying = true;
	f32 m_CurrentTime = 0.0f;
	int32 m_CurrentFrameIndex = 0;
	f32 m_Zoom = 2.0f;
	ImVec2 m_PanOffset = { 0.0f, 0.0f };
	bool m_IsDirty = false;
};

END