#pragma once

#include "UIRenderComponent.h"


BEGIN(Engine)
class Sprite;
CLASS()
class ENGINE_API UIImage : public UIRenderComponent
{
	REFLECT_CLASS()
#pragma region Constructor & Destructor
private:
	UIImage() {}
	virtual ~UIImage() = default;
	virtual EResult Initialize(void* arg = nullptr);
public:
	static UIImage* Create(void* arg = nullptr);
	virtual Component* Clone(GameObject* owner, void* arg = nullptr) override;
	virtual void Free() override;
#pragma endregion

#pragma region Loop
public:
	virtual void LateUpdate(f32 dt) override;
	virtual EResult Render(f32 dt, RenderPass* renderPass) override;
#pragma endregion

#pragma region Getter
public:
	vec2 GetTiling() const { return m_Tiling; }
	vec2 GetOffset() const { return m_Offset; }
	Sprite* GetSprite() const { return m_SpriteHandle.Get(); }
#pragma endregion

#pragma region Setter
public:
	void SetTiling(const vec2& tiling) { m_Tiling = tiling; }
	void SetOffset(const vec2& offset) { m_Offset = offset; }
	void SetSprite(const ResourceHandle<Sprite>& sprite) { m_SpriteHandle = sprite; }
#pragma endregion

#pragma region Member Variables
private:
	PROPERTY(EDITABLE, RANGE())
	vec2 m_Tiling = vec2(1.0f, 1.0f);
	PROPERTY(EDITABLE, RANGE())
	vec2 m_Offset = vec2(0.0f, 0.0f);
	PROPERTY(EDITABLE)
	ResourceHandle<Sprite> m_SpriteHandle;
#pragma endregion
};
END