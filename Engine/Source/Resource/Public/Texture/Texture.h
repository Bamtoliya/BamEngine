#pragma once

#include "RHITexture.h"
#include "Resource.h"

BEGIN(Engine)
typedef struct tagTextureCreateInfo
{
	wstring FilePath;
} TEXTUREDESC;

CLASS()
class ENGINE_API Texture final : public Resource
{
	REFLECT_CLASS(Texture)
#pragma region Constructor&Destructor
private:
	Texture() {}
	virtual ~Texture() = default;
	EResult Initialize(void* arg = nullptr);
public:
	static Texture* Create(void* arg = nullptr);
	virtual void Free() override;
#pragma endregion



#pragma region Load
	virtual EResult LoadFromFile(const wstring& path) override;
#pragma endregion

#pragma region Bind
	virtual EResult Bind(uint32 slot) override;
#pragma endregion



#pragma region Getter
	RHITexture* GetRHITexture() const { return m_RHITexture; }
	f32 GetWorldWidth() const { return m_RHITexture->GetWidth() / m_PixelPerUnit; }
	f32 GetWorldHeight()const { return m_RHITexture->GetHeight() / m_PixelPerUnit; }
	f32 GetPixelPerUnit() const { return m_PixelPerUnit; }
#pragma endregion

#pragma region Setter
	void SetPixelPerUnit(f32 ppu) { m_PixelPerUnit = ppu; IncreaseVersion(); }
#pragma endregion

#pragma region Variable
private:
	class RHITexture* m_RHITexture = { nullptr };

	PROPERTY()
	f32 m_PixelPerUnit = { DEFAULT_PPU };

	PROPERTY(DIRECTORY, READONLY)
	wstring m_Path = L"";
#pragma endregion
};
END