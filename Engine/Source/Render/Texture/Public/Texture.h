#pragma once

#include "RHITexture.h"
#include "TextureBinaryHeader.h"
#include "Resource.h"
#include "TextureMipCache.h"

BEGIN(Engine)
struct TextureCreateDesc : public ResourceCreateDesc
{
	Engine::ETextureDimension dimension = Engine::ETextureDimension::Texture2D;
	Engine::ETextureUsage usage = Engine::ETextureUsage::Sampler;
	bool sRGB = false;
	bool generateMips = true;
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
	EResult InitializeFromEncodedMemory(const vector<uint8>& encodedData, bool generateMips = true);
	EResult InitializeFromDDSMemory(const vector<uint8>& ddsData);
public:
	virtual ~Texture() = default;
public:
	static uint32 CalculateMipLevels(uint32 width, uint32 height);
	// 픽셀을 해석하지 않고 이미지 크기만 읽는다.
	static bool ReadImageSize(const std::filesystem::path& path, uint32& width, uint32& height);
	static Texture* Create(void* arg = nullptr);
	static Texture* Create(const TextureBinaryHeader& header, const vector<uint8>& pixelData, const wstring& key = L"");
	static Texture* CreateFromRHITexture(RHITexture* rhiTexture, const wstring& key = L"");
	static Texture* CreateFromEncodedMemory(const vector<uint8>& encodedData, const wstring& key = L"", bool generateMips = true);
	static Texture* CreateFromDDSMemory(const vector<uint8>& ddsData, const wstring& key = L"");
	virtual void Free() override;
#pragma endregion

#pragma region Bind
public:
	virtual EResult Bind(uint32 slot) override;
#pragma endregion

#pragma region Getter
public:
	// 자리만 등록한 텍스처는 원본을 먼저 읽는다.
	RHITexture* GetRHITexture() const { RequireResident(); return m_RHITexture; }
	// 원본을 기다리지 않고 지금 있는 텍스처(임시 텍스처일 수 있음). 크기를 쓰지 않는 재질 바인딩에만 쓴다.
	RHITexture* GetAvailableRHITexture() const { return m_RHITexture; }
	f32 GetWorldWidth() const { return GetRHITexture()->GetWidth() / m_PixelPerUnit; }
	f32 GetWorldHeight()const { return GetRHITexture()->GetHeight() / m_PixelPerUnit; }
	f32 GetPixelPerUnit() const { return m_PixelPerUnit; }
	const TextureBinaryHeader& GetCachedHeader() const { RequireResident(); return m_CachedHeader; }
	const vector<uint8>& GetTempData() const { RequireResident(); return m_TempData; }
#pragma endregion

#pragma region Setter
public:
	void SetPixelPerUnit(f32 ppu) { m_PixelPerUnit = ppu; IncreaseVersion(); }
#pragma endregion

#pragma region TempData
public:
	void SetTempData(const vector<uint8>& data)
	{
		RequireResident();
		m_TempData = data;
		m_CachedHeader.dataSize = static_cast<uint32>(data.size());
		if (m_CachedHeader.mipLevels == 0) m_CachedHeader.mipLevels = 1;
		if (m_CachedHeader.arraySize == 0) m_CachedHeader.arraySize = 1;
	}
	void ClearTempData() { RequireResident(); m_TempData.clear(); }
#pragma endregion

#pragma region Save&Load
public:
	[[nodiscard]] archive::ArchiveResult WriteArchive(archive::IArchive& ar) const override;
	[[nodiscard]] archive::ArchiveResult ReadArchive(const archive::IArchive& ar) override;
	EResult Reload(const std::filesystem::path& filePath = {}) override;
	EResult ReloadFromEncodedMemory(const vector<uint8>& encodedData, bool generateMips = true);
	EResult UploadToGPU() override;
	// 자리만 등록한 텍스처의 GPU 텍스처를 일부 밉으로 바꾼다. CPU 픽셀은 보관하지 않으며 여전히 자리만 등록한 상태다.
	EResult ApplyStreamedMips(const TextureMipData& mips);
	[[nodiscard]] size_t GetUploadByteSize() const override { return m_TempData.size(); }
protected:
	EResult AdoptPrepared(Resource& prepared) override;
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