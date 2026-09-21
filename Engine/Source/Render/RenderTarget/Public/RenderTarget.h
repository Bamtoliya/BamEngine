#pragma once

#include "RHI.h"
#include "RHITexture.h"

BEGIN(Engine)
struct RenderTargetDesc
{
	RHITextureDesc				textureDesc = {};
	wstring						name = L"RenderTarget";
	bool						fixedSize = false;
	f32							sizeScale = 1.0f;
};
class ENGINE_API RenderTarget final : public Base
{
#pragma region Constructr&Destructor
private:
	using DESC = RenderTargetDesc;
	RenderTarget() {}
	virtual ~RenderTarget() = default;
	EResult Initialize(void* arg = nullptr);
public:
	static RenderTarget* Create(void* arg = nullptr);
	virtual void Free() override;
#pragma endregion

#pragma region Texture Management
public:
	uint32 GetWidth() const { return m_Texture->GetWidth(); }
	uint32 GetHeight() const { return m_Texture->GetHeight(); }
	RHITexture* GetTexture() const { return m_Texture; }
	EResult Resize(uint32 width, uint32 height);
	ETextureFormat GetFormat() const { return m_Texture->GetFormat(); }
	ETextureUsage GetUsage() const { return m_Texture->GetUsage(); }
	ETextureDimension GetTextureDimension() const { return m_Texture->GetDimension(); }
	vec4 GetClearColor() const { return m_Texture->GetClearColor(); }
#pragma endregion

#pragma region Name Management
public:
	const wstring& GetName() const { return m_Name; }
	void SetName(const wstring& name) { m_Name = name; }
#pragma endregion

#pragma region Members
private:
	RHITexture* m_Texture = { nullptr };
	wstring m_Name = { L"RenderTarget" };
	bool m_FixedSize = { false };
	f32 m_SizeScale = { 1.0f };
#pragma endregion
};
END