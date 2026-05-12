#pragma once

#include "Texture.h"
#include "ResourceHandle.h"

struct tagSpriteCreateDesc : public tagResourceCreateDesc
{
	ResourceHandle<Texture> Texture;
	wstring TexturePath = L"";
	Engine::Rect Region = { 0.f, 0.f, 0.f, 0.f };
	vec2 Pivot = { 0.5f, 0.5f };
};

BEGIN(Engine)

CLASS()
class ENGINE_API Sprite final : public Resource
{
	REFLECT_CLASS()
	DECLARE_RESOURCE(Sprite)
	using DESC = tagSpriteCreateDesc;
#pragma region Constructor&Destructor
private:
	Sprite() : Resource(EResourceType::Sprite) {}
	virtual ~Sprite() = default;
	EResult Initialize(void* arg = nullptr);
public:
	static Sprite* Create(void* arg = nullptr);
	virtual void Free() override;
#pragma endregion


#pragma region Bind
public:
	EResult Bind(uint32 slot);
#pragma endregion

#pragma region MyRegion
public:
	vec4 GetUVTransform();
#pragma endregion

#pragma region Region Management
public:
	Rect GetRegion() const { return m_Region; }
	EResult SetRegion(const Rect& region);
#pragma endregion

#pragma region Pivot Management
public:
	vec2 GetPivot() const { return m_Pivot; }
	EResult SetPivot(const vec2& pivot);
#pragma endregion

#pragma region Texture Management
public:
	Texture* GetTexture() { return m_Texture.Get(); }
	const ResourceHandle<Texture>& GetTextureHandle() const { return m_Texture; }
	EResult SetTexture(ResourceHandle<Texture> texture);
#pragma endregion

#pragma region Member Variable
private:
	PROPERTY(READONLY)
	ResourceHandle<Texture> m_Texture;

	PROPERTY(READONLY)
	Rect m_Region = { 0.f, 0.f, 0.f, 0.f };

	PROPERTY(DEFAULT(vec2(0.5f, 0.5f)))
	vec2 m_Pivot = { 0.5f, 0.5f };
#pragma endregion
};
END