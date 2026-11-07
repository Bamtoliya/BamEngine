#pragma once
#include "TextureConverter.h"
#include "TextureCompressorInterface.h"

using namespace Tools;

void TextureConverter::Convert(const std::wstring& inputFilePath, const std::wstring& outputFilePath, Engine::ETextureFormat format, ECompressFlags flags)
{
	TextureCompressorInterface* compressor = TextureCompressorFactory::Create();
	if (!compressor)
	{
		std::wcerr << L"[Error] Unsupported format.\n";
		return;
	}

	// 2. 압축 실행
	bool bSuccess = compressor->CompressTexture(inputFilePath, outputFilePath);

	if (bSuccess) std::wcout << L"Conversion Success: " << outputFilePath << L"\n";
	else std::wcerr << L"Conversion Failed: " << inputFilePath << L"\n";

	// 3. 메모리 해제
	delete compressor;
}
