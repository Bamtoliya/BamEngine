#pragma once

#include "Texture.h"
#include "Renderer.h"

#pragma region Constructor&Destructor
EResult Texture::Initialize(void* arg)
{
	if (IsFailure(__super::Initialize(arg)))
		return EResult::Fail;
	CAST_DESC
	RHI* rhi = Renderer::Get().GetRHI();

	m_RHITexture = rhi->CreateTextureFromFile(m_Path.c_str());

	if (!m_RHITexture)
		return EResult::Fail;

	return EResult::Success;
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

	tagTextureBinaryHeader texHeader;

	if (m_RHITexture)
	{
		texHeader.Width = m_RHITexture->GetWidth();
		texHeader.Height = m_RHITexture->GetHeight();
		texHeader.Depth = m_RHITexture->GetDepth();
		texHeader.MipLevels = m_RHITexture->GetMipLevels();
		texHeader.ArraySize = m_RHITexture->GetArraySize();

		texHeader.Format = m_RHITexture->GetFormat();
		texHeader.Dimension = m_RHITexture->GetDimension();
		texHeader.DataSize = 0; // 실제 텍스처 데이터 크기는 저장하지 않음 (RHITexture에서 관리)
	}

	ar.Process("TextureHeader", texHeader);

	//if (texHeader.DataSize > 0 && m_RHITexture)
	//{
	//	// 텍스처 데이터를 저장하는 경우 (예: 바이너리 파일로 저장할 때)
	//	vector<uint8> textureData(texHeader.DataSize);
	//	m_RHITexture->GetNativeHandle();
	//	if (m_RHITexture->GetTextureData(textureData.data(), texHeader.DataSize))
	//	{
	//		ar.ProcessRaw("TextureData", textureData.data(), texHeader.DataSize);
	//	}
	//	else
	//	{
	//		fmt::print(stderr, "Failed to retrieve texture data for serialization.\n");
	//	}
	//}
}

void Texture::Deserialize(Archive& ar)
{
	Resource::Deserialize(ar);
	tagTextureBinaryHeader texHeader;
	ar.Process("TextureHeader", texHeader);
	if (texHeader.DataSize > 0)
	{
		std::vector<uint8> rawData(texHeader.DataSize);

		ar.ProcessRaw("PixelData", rawData.data(), rawData.size());

		// 4. 읽어들인 CPU 메모리 데이터를 이용해 실제 GPU 텍스처(RHI) 생성
		tagRHITextureDesc desc = {};
		desc.Width = texHeader.Width;
		desc.Height = texHeader.Height;
		desc.Depth = texHeader.Depth;
		desc.MipLevels = texHeader.MipLevels;
		desc.ArraySize = texHeader.ArraySize;
		desc.Format = texHeader.Format;
		desc.Dimension = texHeader.Dimension;
		desc.Data = rawData.data();
		desc.DataSize = texHeader.DataSize;

		// (이미 생성된 텍스처가 있다면 해제)
		Safe_Release(m_RHITexture); 

		// GPU에 최종 텍스처 리소스 생성
		m_RHITexture = Renderer::Get().GetRHI()->CreateTextureFromMemory(desc);
	}
}