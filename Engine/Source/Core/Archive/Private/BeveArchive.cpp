#pragma once

#include "BeveArchive.h"

#pragma region Save&Load
bool BeveArchive::SaveToFile(string_view filePath)
{
	if (IsReading()) return false;

	std::string buffer;
	auto ec = glz::write_beve(m_Root, buffer);
	if (ec) return false;

	std::ofstream outFile(filePath.data(), std::ios::binary);
	if (!outFile || !outFile.is_open()) return false;

	outFile.write(buffer.data(), buffer.size());
	return true;
}

bool BeveArchive::LoadFromFile(string_view filePath)
{
	if (IsWriting()) return false;
	std::ifstream inFile(filePath.data(), std::ios::binary);

	if (!inFile || !inFile.is_open()) return false;

	inFile.seekg(0, std::ios::end);
	size_t size = (size_t)inFile.tellg();
	inFile.seekg(0, std::ios::beg);

	std::string buffer(size, '\0');
	inFile.read(buffer.data(), size);

	auto ec = glz::read_json(m_Root, buffer);
	return !ec;
}
#pragma endregion
