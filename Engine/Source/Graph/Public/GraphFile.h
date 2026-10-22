#pragma once

#include "Graph.h"
#include <archive/ArchiveResult.h>
#include <reflection/Registry.h>
#include <reflection_archive/PropertyArchiveCodecs.h>
#include <filesystem>
#include <system_error>

namespace Engine
{

	enum class EGraphFileStage { None, Validate, Serialize, Read, Deserialize, Write, Replace };

	struct GraphFileResult
	{
		EGraphFileStage Stage = EGraphFileStage::None;
		archive::ArchiveResult ArchiveError;
		GraphDiagnostic GraphError;
		std::error_code SystemError;

		bool Succeeded() const { return Stage == EGraphFileStage::None; }
	};

	[[nodiscard]] ENGINE_API GraphFileResult SaveGraphJson(const std::filesystem::path& path, const Graph& graph,
		const GraphRegistry& nodes, const reflection::Registry& types,
		const archive_reflection::PropertyCodecs& codecs);

	[[nodiscard]] ENGINE_API GraphFileResult LoadGraphJson(const std::filesystem::path& path, Graph& graph,
		const GraphRegistry& nodes, const reflection::Registry& types,
		const archive_reflection::PropertyCodecs& codecs);

}