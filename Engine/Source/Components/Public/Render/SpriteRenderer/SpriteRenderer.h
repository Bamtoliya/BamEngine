#pragma once

#include "RenderComponent.h"
#include "Texture.h"

BEGIN(Engine)


CLASS()
class ENGINE_API SpriteRenderer : public RenderComponent
{
	REFLECT_CLASS(SpriteRenderer)
#pragma region Constructor&Destructor
private:
	SpriteRenderer() {}
	virtual ~SpriteRenderer() = default;
	virtual EResult Initialize(void* arg = nullptr);
public:
	static SpriteRenderer* Create(void* arg = nullptr);
	virtual Component* Clone(GameObject* owner, void* arg = nullptr) override;
	virtual void Free() override;
#pragma endregion
#pragma region Loop
public:
	virtual void LateUpdate(f32 dt) override;
	virtual EResult Render(f32 dt) override;
#pragma endregion

#pragma region Getter
public:
	vec2 GetTiling() const { return m_Tiling; }
#pragma endregion
#pragma region Setter
public:
	EResult SetTexture(Texture* texture);
	EResult SetPivot(vec2 pivot);
	EResult SetTiling(vec2 tiling);
private:
	EResult UpdateMesh();
#pragma endregion

#pragma region Variable
private:
	PROPERTY(DEFAULT(vec2(0.5f, 0.5f)))
	vec2 m_Pivot = { 0.5f, 0.5f };

	vec2 m_PrevPivot = { -1.f, -1.f };

	PROPERTY(DEFAULT(vec2(1.f, 1.f)))
	vec2 m_Tiling = { 1.f, 1.f };

	PROPERTY()
	Mesh* m_Mesh = { nullptr };

	PROPERTY()
	Texture* m_Texture = { nullptr };

	f32 m_CachedPPU = { 0.f };

	//PROPERTY(READONLY)
	//uint32 m_CachedTextureVersion = { 0 };
#pragma endregion


};
END