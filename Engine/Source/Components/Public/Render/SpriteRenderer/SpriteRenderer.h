#pragma once

#include "RenderComponent.h"
#include "Mesh.h"
#include "Sprite.h"
#include "Material.h"


ENUM()
enum class EDrawMode
{
	Simple,
	Sliced,
	Tiled,
	Filled
};

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
	EResult SetSprite(Sprite* sprite);
	EResult SetSprite(Texture* texture);
	EResult SetPivot(vec2 pivot);
	EResult SetTiling(vec2 tiling);
private:
	EResult UpdateMesh();
	EResult UpdateMaterialInstance();
#pragma endregion

#pragma region Variable
private:
	PROPERTY(DEFAULT(vec2(1.f, 1.f)))
	vec2 m_Tiling = { 1.f, 1.f };

	Mesh* m_Mesh = { nullptr };

	PROPERTY()
	Sprite* m_Sprite = { nullptr };

	PROPERTY(CATEGORY("DETAIL"), READONLY)
	uint32 m_CachedSpriteVersion = { 0 };

	PROPERTY()
	EDrawMode m_DrawMode = EDrawMode::Simple;
#pragma endregion


};
END