#include "PickerAssetCatalog.h"
#include "PlatformPath.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <exception>
#include <future>
#include <utility>

namespace Editor::Detail
{
	namespace
	{
		using Clock = std::chrono::steady_clock;
		using Roots = std::array<std::filesystem::path, 2>;
		std::atomic_bool pickerFilesChanged = false;

		struct PickerAssetCatalogState
		{
			PickerAssetCatalog Catalog;
			Roots CurrentRoots;
			Roots ScanRoots;
			std::future<std::vector<PickerAssetFile>> Scan;
			Clock::time_point ScanAfter{};
			bool ScanPending = true;
		};

		PickerAssetCatalogState& GetPickerAssetCatalogState()
		{
			static PickerAssetCatalogState state;
			return state;
		}

		std::vector<PickerAssetFile> ScanPickerAssetFiles(const Roots& roots)
		{
			std::vector<PickerAssetFile> files;
			for (std::size_t index = 0; index < roots.size(); ++index)
			{
				const auto& root = roots[index];
				if (root.empty() || (index != 0 && root == roots.front()))
					continue;

				std::error_code error;
				auto it = std::filesystem::recursive_directory_iterator(
					root, std::filesystem::directory_options::skip_permission_denied, error);
				for (; !error && it != std::filesystem::recursive_directory_iterator(); it.increment(error))
				{
					std::error_code statusError;
					if (!it->is_regular_file(statusError))
						continue;

					const auto utf8 = it->path().filename().u8string();
					std::string filename(utf8.begin(), utf8.end());
					std::string lowerFilename = filename;
					std::ranges::transform(lowerFilename, lowerFilename.begin(),
						[](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
					files.push_back({ it->path(), std::move(filename), std::move(lowerFilename) });
				}
			}
			return files;
		}
	}

	const PickerAssetCatalog& GetPickerAssetCatalog()
	{
		return GetPickerAssetCatalogState().Catalog;
	}

	void InvalidatePickerAssetCatalog()
	{
		pickerFilesChanged.store(true);
	}

	void UpdatePickerAssetCatalog()
	{
		auto& state = GetPickerAssetCatalogState();
		const auto now = Clock::now();
		const auto& projectRoot = Engine::PlatformPath::GetProjectAssetRoot();
		const auto& engineRoot = Engine::PlatformPath::GetEngineResourceRoot();
		if (projectRoot != state.CurrentRoots[0] || engineRoot != state.CurrentRoots[1])
		{
			state.CurrentRoots = { projectRoot, engineRoot };
			state.Catalog.Files.clear();
			state.Catalog.IsReady = false;
			++state.Catalog.Revision;
			state.ScanAfter = now;
			state.ScanPending = true;
		}

		if (pickerFilesChanged.exchange(false))
		{
			state.ScanAfter = now + std::chrono::milliseconds(150);
			state.ScanPending = true;
		}

		if (state.Scan.valid() && state.Scan.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
		{
			try
			{
				auto files = state.Scan.get();
				if (state.ScanRoots == state.CurrentRoots)
				{
					state.Catalog.Files = std::move(files);
					state.Catalog.IsReady = true;
					++state.Catalog.Revision;
				}
			}
			catch (const std::exception&)
			{
				state.ScanPending = true;
				state.ScanAfter = now + std::chrono::seconds(1);
			}
		}

		if (!state.ScanPending || state.Scan.valid() || now < state.ScanAfter)
			return;

		try
		{
			state.ScanRoots = state.CurrentRoots;
			state.Scan = std::async(std::launch::async, [scanRoots = state.ScanRoots]
				{
					return ScanPickerAssetFiles(scanRoots);
				});
			state.ScanPending = false;
		}
		catch (const std::exception&)
		{
			state.ScanAfter = now + std::chrono::seconds(1);
		}
	}
}
