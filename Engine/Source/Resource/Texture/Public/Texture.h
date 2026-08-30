#pragma once

#include "RHITexture.h"
#include "Resource.h"

BEGIN(Engine)
STRUCT()
struct ENGINE_API TextureBinaryHeader
{
	REFLECT_STRUCT()

	PROPERTY()
	uint32 width = 0;
	PROPERTY()
	uint32 height = 0;
	PROPERTY()
	uint32 depth = 0;
	PROPERTY()
	uint32 mipLevels = 0;
	PROPERTY()
	uint32 arraySize = 0;
	PROPERTY()
	Engine::ETextureFormat format = Engine::ETextureFormat::UNKNOWN;
	PROPERTY()
	Engine::ETextureDimension dimension = Engine::ETextureDimension::Texture2D;
	PROPERTY()
	uint32 dataSize = 0;
};
struct TextureCreateDesc : public ResourceCreateDesc
{
	Engine::ETextureDimension dimension = Engine::ETextureDimension::Texture2D;
	Engine::ETextureUsage usage = Engine::ETextureUsage::Sampler;
	bool sRGB = false;
};

CLASS()
class ENGINE_API Texture final : public Resource
{
	REFLECT_CLASS()
	DECLARE_RESOURCE(Texture)
	using DESC = TextureCreateDesc;
#pragma region Constructor&Destructor
private:
	Texture() : Resource(EResourceType::Texture) {}
	EResult Initialize(void* arg = nullptr);
public:
	virtual ~Texture() = default;
public:
	static Texture* Create(void* arg = nullptr);
	static Texture* Create(const TextureBinaryHeader& header, const vector<uint8>& pixelData, const wstring& key = L"");
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

#pragma region TempData
public:
	void SetTempData(const vector<uint8>& data) { m_TempData = data; }
	void ClearTempData() { m_TempData.clear(); }
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

	TextureBinaryHeader m_CachedHeader; // Serialize 시 텍스처 헤더 정보를 저장하는 용도 (BinaryArchive에서만 사용)
	vector<uint8> m_TempData; // Serialize 시 임시로 원본 텍스처 데이터를 저장하는 용도 (BinaryArchive에서만 사용)
#pragma endregion
};
END