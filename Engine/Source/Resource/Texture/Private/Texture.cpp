#pragma once

#include "Texture.h"
#include "Renderer.h"
#include "ResourceManager.h"

#pragma region Constructor&Destructor
EResult Texture::Initialize(void* arg)
{
	if (IsFailure(__super::Initialize(arg)))
		return EResult::Fail;
	CAST_DESC
	if (m_Path.empty()) return EResult::Success;

	RHI* rhi = Renderer::Get().GetRHI();
	if (!rhi) return EResult::Fail;
	m_RHITexture = rhi->CreateTextureFromFile(m_Path.c_str());
	return m_RHITexture ?  EResult::Success : EResult::Fail;
}

Texture* Texture::Create(void* arg)
{
	Texture* instance = new Texture();
	if (IsFailure(instance->Initialize(arg)))
	{
		delete instance;
		return nullptr;
	}
	return instance;
}

Texture* Texture::Create(const tagTextureBinaryHeader& header, const vector<uint8>& pixelData, const wstring& key)
{
	Texture* instance = new Texture();
	instance->m_CachedHeader = header;
	instance->m_TempData = pixelData;
	return instance;
}

void Texture::Free()
{
	Safe_Release(m_RHITexture);
}
#pragma endregion

#pragma region Bind
EResult Texture::Bind(uint32 slot)
{
	RHI* rhi = Renderer::Get().GetRHI();
	if (!rhi || !m_RHITexture) return EResult::Fail;
	return rhi->BindTexture(m_RHITexture, slot);
}
#pragma endregion

void Texture::Serialize(Archive& ar)
{
	Resource::Serialize(ar);

	if (m_RHITexture)
	{
		m_CachedHeader.Width = m_RHITexture->GetWidth();
		m_CachedHeader.Height = m_RHITexture->GetHeight();
		m_CachedHeader.Depth = m_RHITexture->GetDepth();
		m_CachedHeader.MipLevels = m_RHITexture->GetMipLevels();
		m_CachedHeader.ArraySize = m_RHITexture->GetArraySize();
		m_CachedHeader.Format = m_RHITexture->GetFormat();
		m_CachedHeader.Dimension = m_RHITexture->GetDimension();
	}
	m_CachedHeader.DataSize = static_cast<uint32>(m_TempData.size());


	ar.Process("TextureHeader", m_CachedHeader);

	if (!m_TempData.empty())
	{
		ar.ProcessRaw("PixelData", m_TempData.data(), m_TempData.size());
	}
}

void Texture::Deserialize(Archive& ar)
{
	Resource::Deserialize(ar);

	ar.Process("TextureHeader", m_CachedHeader);

	if (m_CachedHeader.DataSize > 0)
	{
		std::vector<uint8> rawData(m_CachedHeader.DataSize);
		ar.ProcessRaw("PixelData", rawData.data(), rawData.size());

		tagRHITextureDesc desc = {};
		desc.Width = m_CachedHeader.Width;
		desc.Height = m_CachedHeader.Height;
		desc.Depth = m_CachedHeader.Depth;
		desc.MipLevels = m_CachedHeader.MipLevels;
		desc.ArraySize = m_CachedHeader.ArraySize;
		desc.Format = m_CachedHeader.Format;
		desc.Dimension = m_CachedHeader.Dimension;
		desc.Data = rawData.data();
		desc.DataSize = m_CachedHeader.DataSize;

		Safe_Release(m_RHITexture);
		m_RHITexture = Renderer::Get().GetRHI()->CreateTextureFromMemory(desc);
	}
}