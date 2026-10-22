#pragma once

#include "Graph.h"
#include <string>

namespace Engine
{
	class ENGINE_API ShaderGraphCompiler final
	{
	public:
		[[nodiscard]] static GraphDiagnostic GenerateColorHlsl(
			const Graph& graph, const GraphRegistry& registry, std::string& outSource);

		[[nodiscard]] static GraphDiagnostic GenerateUnlitMaterialHlsl(
			const Graph& graph, const GraphRegistry& registry, std::string& outSource);
	};
}