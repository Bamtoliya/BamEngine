#pragma once

#include "Base.h"
#include "RHI.h"

enum class ERenderTargetFormat : uint8
{
	RTF_RGBA8,
	RTF_RGBA16F,
	RTF_RGBA32F,
	RTF_DEPTH24STENCIL8,
};

enum class ERenderTargetUsage : uint8
{
	RTU_Color,
	RTU_DepthStencil,
};

enum class ERenderTargetBindFlag : uint8
{
	RTBF_None = 0,
	RTBF_ShaderResource = 1 << 0,
	RTBF_RenderTarget = 1 << 1,
	RTBF_DepthStencil = 1 << 2,
};

enum class ERenderTargetClearFlag : uint8
{
	RTCF_None = 0,
	RTCF_Color = 1 << 0,
	RTCF_Depth = 1 << 1,
	RTCF_Stencil = 1 << 2,
};

enum class ERenderTargetType : uint8
{
	Color,
	GBuffer,
	DepthStencil,
};

struct tagRenderTargetDesc
{
	ERenderTargetFormat		Format = ERenderTargetFormat::RTF_RGBA8;
	ERenderTargetUsage		Usage = ERenderTargetUsage::RTU_Color;
	ERenderTargetBindFlag	BindFlag = ERenderTargetBindFlag::RTBF_None;
	ERenderTargetType 		Type = ERenderTargetType::Color;
	uint32					Width = 800;
	uint32					Height = 600;
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
	uint32 GetWidth() const { return m_Width; }
	uint32 GetHeight() const { return m_Height; }
	vector<RHITexture*>& GetTextures() { return m_Textures; }
	uint32 GetTextureCount() const { return static_cast<uint32>(m_Textures.size()); }
	RHITexture* GetTexture(uint32 index) const { return m_Textures[index]; }
	RHITexture* GetDepthStencilTexture() const { return m_DepthStencilTexture; }
	EResult Resize(uint32 width, uint32 height);
#pragma endregion


#pragma region Members
private:
	vector<RHITexture*> m_Textures;
	RHITexture* m_DepthStencilTexture = { nullptr };
	uint32 m_Width = { 1 };
	uint32 m_Height = { 1 };

#pragma endregion
};
END