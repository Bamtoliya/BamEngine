#include "ShaderGraphCompiler.h"
#include "GraphValue.h"
#include <algorithm>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

namespace Engine
{
	namespace
	{
		GraphDiagnostic ResolveShaderColorLiteral(const Graph& graph, const GraphRegistry& registry, std::string& outLiteral)
		{
			if (graph.GetDomain() != EGraphDomain::Shader)
				return { EGraphError::UnsupportedDomain, 0, {}, 0, "ShaderGraph.DomainRequired" };

			const auto diagnostics = graph.Validate(registry);
			if (!diagnostics.empty()) return diagnostics.front();

			const GraphNode* output = nullptr;
			for (const auto& node : graph.GetNodes())
			{
				if (node.Type == "Shader.ColorOutput")
				{
					if (output)
						return { EGraphError::InvalidDocument, node.Id, "color", 0, "ShaderGraph.SingleOutput" };

					output = &node;
				}
				else if (node.Type != "Core.Float4")
				{
					return { EGraphError::UnsupportedCompilation, node.Id, {}, 0, "ShaderGraph.UnsupportedNode" };
				}
			}

			if (!output)
				return { EGraphError::InvalidDocument, 0, {}, 0, "ShaderGraph.SingleOutput" };

			GraphPinReference input{ output->Id, "color" };

			for (std::size_t depth = 0; depth < graph.GetNodes().size(); ++depth)
			{
				const auto* node = graph.FindNode(input.Node);
				const auto* pin = node ? registry.FindPin(node->Type, input.Pin) : nullptr;

				if (!pin || pin->Direction != EGraphPinDirection::Input || pin->Type != EGraphPinType::Float4)
					return { EGraphError::PinNotFound, input.Node, input.Pin, 0, "ShaderGraph.InvalidInput" };

				const auto links = graph.GetLinks();
				const auto link = std::find_if(links.begin(), links.end(),
					[&](const GraphLink& item) { return item.To == input; });

				if (link != links.end())
				{
					const auto* producer = graph.FindNode(link->From.Node);
					if (!producer || producer->Type != "Core.Float4" || link->From.Pin != "result")
					{
						return { EGraphError::UnsupportedCompilation, link->From.Node,
							link->From.Pin, link->Id, "ShaderGraph.UnsupportedNode" };
					}

					input = { producer->Id, "value" };
					continue;
				}

				const auto overrideValue = node->InputValues.find(input.Pin);
				const auto& value = overrideValue != node->InputValues.end()
					? overrideValue->second : pin->DefaultValue;

				const auto* color = std::get_if<std::array<float, 4>>(&value);
				if (!color || !IsFiniteGraphValue(value))
					return { EGraphError::InvalidDefault, input.Node, input.Pin, 0, "ShaderGraph.InvalidColor" };

				std::ostringstream literal;
				literal.exceptions(std::ios::badbit | std::ios::failbit);
				literal.imbue(std::locale::classic());
				literal << std::scientific << std::setprecision(std::numeric_limits<float>::max_digits10 - 1);

				literal << "float4(";
				for (std::size_t channel = 0; channel < color->size(); ++channel)
				{
					if (channel != 0) literal << ", ";
					literal << (*color)[channel] << 'f';
				}
				literal << ')';

				auto generated = literal.str();
				outLiteral.swap(generated);
				return {};
			}

			return { EGraphError::DataCycle, input.Node, input.Pin, 0, "ShaderGraph.Cycle" };
		}
	}

	GraphDiagnostic ShaderGraphCompiler::GenerateColorHlsl(
		const Graph& graph, const GraphRegistry& registry, std::string& outSource)
	{
		std::string literal;
		const auto result = ResolveShaderColorLiteral(graph, registry, literal);
		if (!result.Succeeded()) return result;

		std::string source = "float4 main() : SV_Target0\n{\n\treturn " + literal + ";\n}\n";
		outSource.swap(source);
		return {};
	}

	GraphDiagnostic ShaderGraphCompiler::GenerateUnlitMaterialHlsl(
		const Graph& graph, const GraphRegistry& registry, std::string& outSource)
	{
		std::string literal;
		const auto result = ResolveShaderColorLiteral(graph, registry, literal);
		if (!result.Succeeded()) return result;

		std::string source = R"(#include "shading_flags.hlsli"

struct PS_INPUT
{
	float4 Position : SV_POSITION;
	float3 WorldPos : WORLD_POSITION;
	float3 Normal : NORMAL;
	float2 TexCoord : TEXCOORD0;
	float3 Tangent : TANGENT;
	float4 Color : COLOR0;
	nointerpolation float PickingId : TEXCOORD5;
};

struct PS_OUTPUT
{
	float4 Diffuse : SV_TARGET0;
	float4 Normal : SV_TARGET1;
	float4 PBR : SV_TARGET2;
	float4 Emission : SV_TARGET3;
	float4 Position : SV_TARGET4;
};

PS_OUTPUT main(PS_INPUT input)
{
	PS_OUTPUT output;
	output.Diffuse = )";

		source += literal;
		source += R"(;
	output.Normal = float4(0.5f, 0.5f, 1.0f, PackShadingData(SHADING_MODEL_UNLIT, 0u));
	output.PBR = float4(1.0f, 0.0f, 0.0f, 1.0f);
	output.Emission = float4(0.0f, 0.0f, 0.0f, 0.0f);
	output.Position = float4(input.WorldPos, input.PickingId);
	return output;
}
)";

		outSource.swap(source);
		return {};
	}
}