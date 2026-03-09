#pragma once

#include "TextureImporter.h"

EResult TextureImporter::Import(const filesystem::path & sourcePath, const filesystem::path & destDir)
{
	uint32 width = { 0 }, height = { 0 }, channel = { 0 };

	stbi_uc* data = stbi_load(sourcePath.string().c_str(), reinterpret_cast<int*>(&width), reinterpret_cast<int*>(&height), reinterpret_cast<int*>(&channel), 0);
	filesystem::path outputPath = destDir;
	outputPath.replace_extension(".bamtex");

	ofstream outputFile(outputPath, ios::binary);
	if (!outputFile)
	{
		stbi_image_free(data);
		return EResult::Fail;
	}

	const char magic[8] = "BAMTEX0";
	outputFile.write(magic, sizeof(magic));

	outputFile.write(reinterpret_cast<char*>(&width), sizeof(width));
	outputFile.write(reinterpret_cast<char*>(&height), sizeof(height));
	outputFile.write(reinterpret_cast<char*>(&channel), sizeof(channel));


	size_t dataSize = width * height * channel;
	outputFile.write(reinterpret_cast<char*>(data), dataSize);

	outputFile.close();

	stbi_image_free(data);

	return EResult::Success;
}