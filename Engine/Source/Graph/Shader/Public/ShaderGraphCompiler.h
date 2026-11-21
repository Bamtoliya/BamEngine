#pragma once

#include "Graph.h"
#include "MaterialInterface.h"
#include <string>

namespace Engine
{
	class ENGINE_API ShaderGraphCompiler final
	{
	public:
		[[nodiscard]] static bool SupportsNode(const std::string& type);

		[[nodiscard]] static MaterialBufferLayout BuildMaterialBufferLayout(const Graph& graph);

		[[nodiscard]] static GraphDiagnostic GenerateGBufferMaterialVS(
			const Graph& graph, const GraphRegistry& registry, std::string& outSource);

		[[nodiscard]] static GraphDiagnostic GenerateColorHlsl(
			const Graph& graph, const GraphRegistry& registry, std::string& outSource);

		[[nodiscard]] static GraphDiagnostic GenerateUnlitMaterialHlsl(
			const Graph& graph, const GraphRegistry& registry, std::string& outSource);

		[[nodiscard]] static GraphDiagnostic GenerateGBufferMaterialHlsl(
			const Graph& graph, const GraphRegistry& registry, std::string& outSource);

		[[nodiscard]] static GraphDiagnostic GenerateForwardTransparentMaterialHlsl(
			const Graph& graph, const GraphRegistry& registry, std::string& outSource);
	};
}
