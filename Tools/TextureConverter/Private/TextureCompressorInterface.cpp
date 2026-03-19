#pragma once
#include "TextureCompressorInterface.h"
#include "DirectXTexCompressor.h"

TextureCompressorInterface* TextureCompressorFactory::Create()
{
	return new DirectXTexCompressor();
}
