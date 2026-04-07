#pragma once

#include "RHITexture.h"
#include "Resource.h"

BEGIN(Engine)
STRUCT()
struct ENGINE_API tagTextureBinaryHeader
{
	REFLECT_STRUCT()

	PROPERTY()
	uint32 Width = 0;
	PROPERTY()
	uint32 Height = 0;
	PROPERTY()
	uint32 Depth = 0;
	PROPERTY()
	uint32 MipLevels = 0;
	PROPERTY()
	uint32 ArraySize = 0;
	PROPERTY()
	Engine::ETextureFormat Format = Engine::ETextureFormat::UNKNOWN;
	PROPERTY()
	Engine::ETextureDimension Dimension = Engine::ETextureDimension::Texture2D;
	PROPERTY()
	uint32 DataSize = 0;
};
struct tagTextureCreateDesc : public tagResourceCreateDesc
{
};

CLASS()
class ENGINE_API Texture final : public Resource
{
	REFLECT_CLASS()
	using DESC = tagTextureCreateDesc;
#pragma region Constructor&Destructor
private:
	Texture() : Resource(EResourceType::Texture) {}
	virtual ~Texture() = default;
	EResult Initialize(void* arg = nullptr);
public:
	static Texture* Create(void* arg = nullptr);
	virtual void Free() override;
#pragma endregion

#pragma region Bind
public:
	virtual EResult Bind(uint32 slot) override;
#pragma endregion

#pragma region Getter
public:
	RHITexture* GetRHITexture() const { return m_RHITexture; }
	f32 GetWorldWidth() const { return m_RHITexture->GetWidth() / m_PixelPerUnit; }
	f32 GetWorldHeight()const { return m_RHITexture->GetHeight() / m_PixelPerUnit; }
	f32 GetPixelPerUnit() const { return m_PixelPerUnit; }
#pragma endregion

#pragma region Setter
public:
	void SetPixelPerUnit(f32 ppu) { m_PixelPerUnit = ppu; IncreaseVersion(); }
#pragma endregion

#pragma region Save&Load
public:
	virtual void Serialize(Archive& ar);
	virtual void Deserialize(Archive& ar);
#pragma endregion

#pragma region Variable
private:
	class RHITexture* m_RHITexture = { nullptr };

	PROPERTY()
	f32 m_PixelPerUnit = { DEFAULT_PPU };
#pragma endregion
};
END