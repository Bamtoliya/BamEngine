#pragma once

#include "RHITexture.h"
#include "TextureBinaryHeader.h"
#include "Resource.h"

BEGIN(Engine)
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
	EResult Initialize(void* arg = nullptr) override;
public:
	virtual ~Texture() = default;
public:
	static Texture* Create(void* arg = nullptr);
	static Texture* Create(const TextureBinaryHeader& header, const vector<uint8>& pixelData, const wstring& key = L"");
	static Texture* CreateFromRHITexture(RHITexture* rhiTexture, const wstring& key = L"");
	virtual void Free() override;
public:
	EResult Reload();
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
	[[nodiscard]] archive::ArchiveResult WriteArchive(archive::IArchive& ar) const override;
	[[nodiscard]] archive::ArchiveResult ReadArchive(const archive::IArchive& ar) override;
private:
	[[nodiscard]] uint32 GetArchiveFormatVersion() const noexcept override { return 1; }
#pragma endregion

#pragma region Variable
private:
	class RHITexture* m_RHITexture = { nullptr };

	PROPERTY()
	f32 m_PixelPerUnit = { DEFAULT_PPU };

	TextureBinaryHeader m_CachedHeader; // 보관한 CPU 픽셀의 형식과 크기
	vector<uint8> m_TempData; // 저장과 재로드에 사용할 CPU 픽셀
#pragma endregion
};
END