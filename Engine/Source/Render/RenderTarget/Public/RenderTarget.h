#pragma once

#include "RHI.h"

BEGIN(Engine)
struct RenderTargetDesc
{
	ETextureFormat				format = ETextureFormat::UNKNOWN;
	ETextureUsage				usage = ETextureUsage::RenderTarget;
	ERenderTargetBindFlag		bindFlag = ERenderTargetBindFlag::RTBF_None;
	ERenderTargetType 			type = ERenderTargetType::Color;
	ETextureDimension			dimension = ETextureDimension::Texture2D;
	uint32						width = 800;
	uint32						height = 600;
	vec4						clearColor = vec4(0.0f, 0.0f, 0.0f, 1.0f);
	wstring						name = L"RenderTarget";
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
	uint32 GetWidth() const { return m_Desc.width; }
	uint32 GetHeight() const { return m_Desc.height; }
	RHITexture* GetTexture() const { return m_Texture; }
	EResult Resize(uint32 width, uint32 height);
	ETextureFormat GetFormat() const { return m_Desc.format; }
	ERenderTargetBindFlag GetBindFlag() const { return m_Desc.bindFlag; }
	ETextureUsage GetUsage() const { return m_Desc.usage; }
	ERenderTargetType GetType() const { return m_Desc.type; }
	ETextureDimension GetTextureDimension() const { return m_Desc.dimension; }
	vec4 GetClearColor() const { return m_Desc.clearColor; }
#pragma endregion

#pragma region Name Management
public:
	const wstring& GetName() const { return m_Desc.name; }
	void SetName(const wstring& name) { m_Desc.name = name; }
#pragma endregion



#pragma region Members
private:
	RHITexture* m_Texture = { nullptr };
	DESC m_Desc;
#pragma endregion
};
END