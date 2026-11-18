#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace Editor::Detail
{
	struct PickerAssetFile
	{
		std::filesystem::path Path;
		std::string Filename;
		std::string LowerFilename;
	};

	struct PickerAssetCatalog
	{
		std::vector<PickerAssetFile> Files;
		std::uint64_t Revision = 0;
		bool IsReady = false;
	};

	// Read and update the published catalog on the editor thread.
	const PickerAssetCatalog& GetPickerAssetCatalog();
	void UpdatePickerAssetCatalog();

	// File watchers may request a refresh from any thread.
	void InvalidatePickerAssetCatalog();
}
