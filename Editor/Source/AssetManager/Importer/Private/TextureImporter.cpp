#pragma once

#include "TextureImporter.h"

EResult TextureImporter::Import(const filesystem::path & sourcePath, const filesystem::path & destDir)
{
	uint32 width = { 0 }, height = { 0 }, channel = { 0 };

	stbi_uc* data = stbi_load(sourcePath.string().c_str(), reinterpret_cast<int*>(&width), reinterpret_cast<int*>(&height), reinterpret_cast<int*>(&channel), 4);
	if (!data) return EResult::Fail;
	channel = 4;


	filesystem::path outputPath = destDir;
	outputPath.replace_extension(".bamtex");

	BinaryArchive archive(EArchiveMode::Write);

	string magic = "BAMTEX0";
	archive.Process("Magic", magic);
	archive.Process("Width", width);
	archive.Process("Height", height);
	archive.Process("Channels", channel);

	size_t dataSize = width * height * channel;
	archive.ProcessRaw("Data", data, dataSize);
	stbi_image_free(data);
	return archive.SaveToFile(outputPath.string()) ? EResult::Success : EResult::Fail;
}