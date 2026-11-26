#pragma once

#include "Graph.h"
#include "MaterialInterface.h"
#include <string>

namespace Engine
{
	struct ShaderGraphCompiledVS
	{
		std::string VertexShaderSource;
		std::string ShadowVertexShaderSource;
	};

	class ENGINE_API ShaderGraphCompiler final
	{
	public:
		[[nodiscard]] static bool SupportsNode(const std::string& type);
		[[nodiscard]] static bool HasWorldPositionOffset(const Graph& graph);

		[[nodiscard]] static MaterialBufferLayout BuildMaterialBufferLayout(const Graph& graph);
		[[nodiscard]] static std::vector<MaterialTextureDescriptor> CollectTextureDescriptors(const Graph& graph);
		[[nodiscard]] static std::vector<MaterialTextureDescriptor> CollectTextureParameters(const Graph& graph);

		[[nodiscard]] static GraphDiagnostic GenerateGBufferMaterialVS(
			const Graph& graph, const GraphRegistry& registry, std::string& outSource);

		[[nodiscard]] static GraphDiagnostic GenerateGBufferMaterialVS(
			const Graph& graph, const GraphRegistry& registry, ShaderGraphCompiledVS& outCompiled);

		[[nodiscard]] static GraphDiagnostic GenerateShadowDepthMaterialVS(
			const Graph& graph, const GraphRegistry& registry, std::string& outShadowVertexShaderSource);

		[[nodiscard]] static GraphDiagnostic GenerateColorHlsl(
			const Graph& graph, const GraphRegistry& registry, std::string& outSource);

		[[nodiscard]] static GraphDiagnostic GenerateUnlitMaterialHlsl(
			const Graph& graph, const GraphRegistry& registry, std::string& outSource);

		[[nodiscard]] static GraphDiagnostic GenerateGBufferMaterialHlsl(
			const Graph& graph, const GraphRegistry& registry, std::string& outSource);

		[[nodiscard]] static GraphDiagnostic GenerateForwardTransparentMaterialHlsl(
			const Graph& graph, const GraphRegistry& registry, std::string& outSource);

		// Terrain 도메인 픽셀 셰이더. 정점 셰이더는 엔진 terrainVS를 쓰므로(높이 변위·그림자·모션 벡터 공유) 픽셀 표면만 만든다.
		// MaterialOutput의 정점 입력(worldPositionOffset·customUV)은 쓰지 않는다. baseColor가 연결되지 않으면 레이어 합성을 쓴다.
		// Terrain.* 노드는 이 생성기에서만 컴파일된다.
		[[nodiscard]] static GraphDiagnostic GenerateTerrainMaterialHlsl(
			const Graph& graph, const GraphRegistry& registry, std::string& outSource);

		// 기본 지형 그래프: terrainPS.hlsl과 같은 결과(칠한 레이어 합성, 경사 0.55~0.78에서 레이어 1로 전환하는 절벽,
		// 거칠기 0.85→0.65, 금속성 0, 캐비티 0.5, AO 1)를 노드로 만든다. 지형 도메인 머티리얼의 새 그래프 시작점이다.
		[[nodiscard]] static GraphDiagnostic BuildDefaultTerrainGraph(const GraphRegistry& registry, Graph& outGraph);
	};
}
