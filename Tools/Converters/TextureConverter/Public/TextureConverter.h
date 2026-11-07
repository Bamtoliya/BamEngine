#pragma once

#include "Engine_Includes.h"
#include "Texture.h"
#include "TextureCompressorInterface.h"

BEGIN(Tools)
class TextureConverter
{
public:
	void Convert(const std::wstring& inputFilePath, const std::wstring& outputFilePath, Engine::ETextureFormat format, ECompressFlags flags = ECompressFlags::None);
};
END