#pragma once

#include "RHI.h"

struct tagRenderTargetDesc
{
	ERenderTargetFormat			Format		= ERenderTargetFormat::RTF_RGBA8;
	ERenderTargetUsage			Usage		= ERenderTargetUsage::RTU_Color;
	ERenderTargetBindFlag		BindFlag	= ERenderTargetBindFlag::RTBF_None;
	ERenderTargetType 			Type		= ERenderTargetType::Color;
	ERenderTargetTextureType	TextureType = ERenderTargetTextureType::RTTT_2D;
	uint32						Width		= 800;
	uint32						Height		= 600;
	vec4						ClearColor	= vec4(0.0f, 0.0f, 0.0f, 1.0f);
};

BEGIN(Engine)
class ENGINE_API RenderTarget final : public Base
{
#pragma region Constructr&Destructor
private:
	using DESC = tagRenderTargetDesc;
	RenderTarget() {}
	virtual ~RenderTarget() = default;
	EResult Initialize(void* arg = nullptr);
public:
	static RenderTarget* Create(void* arg = nullptr);
	virtual void Free() override;
#pragma endregion

#pragma region Texture Management
public:
	uint32 GetWidth() const { return m_Desc.Width; }
	uint32 GetHeight() const { return m_Desc.Height; }
	RHITexture* GetTexture() const { return m_Texture; }
	EResult Resize(uint32 width, uint32 height);
	ERenderTargetFormat GetFormat() const { return m_Desc.Format; }
	ERenderTargetBindFlag GetBindFlag() const { return m_Desc.BindFlag; }
	ERenderTargetUsage GetUsage() const { return m_Desc.Usage; }
	ERenderTargetType GetType() const { return m_Desc.Type; }
	ERenderTargetTextureType GetTextureType() const { return m_Desc.TextureType; }
	vec4 GetClearColor() const { return m_Desc.ClearColor; }
#pragma endregion

#pragma region Name Management
public:
	const wstring& GetName() const { return m_Name; }
	void SetName(const wstring& name) { m_Name = name; }
#pragma endregion



#pragma region Members
private:
	RHITexture* m_Texture = { nullptr };
	DESC m_Desc;
	wstring m_Name;
#pragma endregion
};
END