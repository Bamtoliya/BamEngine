#include "GraphFile.h"
#include <archive/JsonArchive.h>
#include <reflection_archive/ReflectionArchiveAdapter.h>
#include <chrono>
#include <string>
#include <utility>

namespace
{
	namespace fs = std::filesystem;

	struct GraphTempDirectory
	{
		fs::path Directory;
		bool Owned = false;

		GraphTempDirectory() = default;
		GraphTempDirectory(const GraphTempDirectory&) = delete;
		GraphTempDirectory& operator=(const GraphTempDirectory&) = delete;

		~GraphTempDirectory()
		{
			if (!Owned) return;
			std::error_code ignored;
			fs::remove(Directory / "graph.tmp", ignored);
			fs::remove(Directory, ignored);
		}
	};
}

namespace Engine
{

	GraphFileResult SaveGraphJson(const fs::path& path, const Graph& graph, const GraphRegistry& nodes,
		const reflection::Registry& types, const archive_reflection::PropertyCodecs& codecs)
	{
		if (path.empty() || !path.has_filename())
			return { EGraphFileStage::Write, { archive::ArchiveErrorCode::InvalidValue, "Path" } };

		const auto data = graph.ExportData();
		Graph candidate;
		const auto validation = candidate.ImportData(nodes, data);
		if (!validation.Succeeded())
			return { EGraphFileStage::Validate, {}, validation };

		const auto* type = types.FindType<GraphData>();
		if (!type)
			return { EGraphFileStage::Serialize, { archive::ArchiveErrorCode::UnsupportedType, "Engine::GraphData" } };

		archive::JsonArchive writer;
		const auto serialized = archive_reflection::WriteObject(
			writer, "Graph", *type, reflection::ObjectView::From(data), &types, &codecs);
		if (!serialized)
			return { EGraphFileStage::Serialize, serialized };

		std::error_code error;
		const auto target = fs::absolute(path, error);
		if (error)
			return { EGraphFileStage::Write, {}, {}, error };

		GraphTempDirectory temporary;
		const auto stamp = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());

		for (unsigned attempt = 0; attempt < 64; ++attempt)
		{
			auto directory = target.parent_path() / (".bamgraph-save-" + stamp + "-" + std::to_string(attempt));
			if (fs::create_directory(directory, error))
			{
				temporary.Directory = std::move(directory);
				temporary.Owned = true;
				break;
			}

			if (error && error != std::errc::file_exists)
				return { EGraphFileStage::Write, { archive::ArchiveErrorCode::FileOpenFailed, {} }, {}, error };
		}

		if (!temporary.Owned)
			return { EGraphFileStage::Write, {}, {}, std::make_error_code(std::errc::file_exists) };

		const auto stagedFile = temporary.Directory / "graph.tmp";
		const auto written = writer.SaveToFile(stagedFile);
		if (!written)
			return { EGraphFileStage::Write, written };

		fs::rename(stagedFile, target, error);
		if (error)
			return { EGraphFileStage::Replace, {}, {}, error };

		return {};
	}

	GraphFileResult LoadGraphJson(const fs::path& path, Graph& graph, const GraphRegistry& nodes,
		const reflection::Registry& types, const archive_reflection::PropertyCodecs& codecs)
	{
		if (path.empty() || !path.has_filename())
			return { EGraphFileStage::Read, { archive::ArchiveErrorCode::InvalidValue, "Path" } };

		const auto* type = types.FindType<GraphData>();
		if (!type)
			return { EGraphFileStage::Deserialize, { archive::ArchiveErrorCode::UnsupportedType, "Engine::GraphData" } };

		archive::JsonArchive reader;
		const auto loaded = reader.LoadFromFile(path);
		if (!loaded)
			return { EGraphFileStage::Read, loaded };

		GraphData data;
		const auto decoded = archive_reflection::ReadObject(reader, "Graph", *type, data, &types, &codecs);
		if (!decoded)
			return { EGraphFileStage::Deserialize, decoded };

		const auto validation = graph.ImportData(nodes, data);
		if (!validation.Succeeded())
			return { EGraphFileStage::Validate, {}, validation };

		return {};
	}

}