#include "ShaderGraphCompiler.h"
#include "GraphValue.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <unordered_set>
#include <vector>

namespace Engine
{
	namespace
	{
		std::string FormatFloat(float value)
		{
			std::ostringstream literal;
			literal.exceptions(std::ios::badbit | std::ios::failbit);
			literal.imbue(std::locale::classic());
			literal << std::scientific << std::setprecision(std::numeric_limits<float>::max_digits10 - 1);
			literal << value << 'f';
			return literal.str();
		}

		std::string FormatFloat3(const std::array<float, 3>& value)
		{
			return "float3(" + FormatFloat(value[0]) + ", " + FormatFloat(value[1]) + ", " + FormatFloat(value[2]) + ")";
		}

		std::string FormatFloat4(const std::array<float, 4>& value)
		{
			return "float4(" + FormatFloat(value[0]) + ", " + FormatFloat(value[1]) + ", " +
				FormatFloat(value[2]) + ", " + FormatFloat(value[3]) + ")";
		}

		bool IsSupportedShaderNode(const std::string& type)
		{
			static const std::unordered_set<std::string> supported{
				"Shader.ColorOutput", "Shader.MaterialOutput",
				"Core.Float", "Core.Float3", "Core.Float4",
				"Core.Reroute.Float", "Core.Reroute.Float3", "Core.Reroute.Float4",
				"Math.MakeFloat3", "Math.BreakFloat3",
				"Math.MakeFloat4", "Math.BreakFloat4"
			};
			return supported.contains(type);
		}

		GraphDiagnostic ValidateShaderGraph(
			const Graph& graph, const GraphRegistry& registry, const GraphNode*& outOutputNode)
		{
			outOutputNode = nullptr;
			if (graph.GetDomain() != EGraphDomain::Shader)
				return { EGraphError::UnsupportedDomain, 0, {}, 0, "ShaderGraph.DomainRequired" };

			const auto diagnostics = graph.Validate(registry);
			if (!diagnostics.empty()) return diagnostics.front();

			for (const auto& node : graph.GetNodes())
			{
				if (!IsSupportedShaderNode(node.Type))
					return { EGraphError::UnsupportedCompilation, node.Id, {}, 0, "ShaderGraph.UnsupportedNode" };

				if (node.Type == "Shader.ColorOutput" || node.Type == "Shader.MaterialOutput")
				{
					if (outOutputNode)
						return { EGraphError::InvalidDocument, node.Id, {}, 0, "ShaderGraph.SingleOutput" };
					outOutputNode = &node;
				}
			}

			if (!outOutputNode)
				return { EGraphError::InvalidDocument, 0, {}, 0, "ShaderGraph.SingleOutput" };

			return {};
		}

		GraphDiagnostic ResolveProducerOutput(
			const Graph& graph, const GraphRegistry& registry,
			GraphPinReference outputPin, EGraphPinType expectedType,
			std::vector<GraphPinReference>& callStack, std::string& outExpression);

		GraphDiagnostic ResolveInputExpression(
			const Graph& graph, const GraphRegistry& registry,
			GraphPinReference inputPin, EGraphPinType expectedType,
			std::vector<GraphPinReference>& callStack, std::string& outExpression)
		{
			if (std::find(callStack.begin(), callStack.end(), inputPin) != callStack.end() ||
				callStack.size() > graph.GetNodes().size() * 8)
			{
				return { EGraphError::DataCycle, inputPin.Node, inputPin.Pin, 0, "ShaderGraph.Cycle" };
			}

			const auto* node = graph.FindNode(inputPin.Node);
			const auto* pin = node ? registry.FindPin(node->Type, inputPin.Pin) : nullptr;
			if (!pin || pin->Direction != EGraphPinDirection::Input || pin->Type != expectedType)
				return { EGraphError::PinNotFound, inputPin.Node, inputPin.Pin, 0, "ShaderGraph.InvalidInput" };

			const auto links = graph.GetLinks();
			const auto link = std::find_if(links.begin(), links.end(),
				[&](const GraphLink& item) { return item.To == inputPin; });

			if (link != links.end())
			{
				callStack.push_back(inputPin);
				const auto result = ResolveProducerOutput(
					graph, registry, link->From, expectedType, callStack, outExpression);
				callStack.pop_back();
				return result;
			}

			const auto overrideValue = node->InputValues.find(inputPin.Pin);
			const auto& value = overrideValue != node->InputValues.end() ? overrideValue->second : pin->DefaultValue;

			if (!IsFiniteGraphValue(value) || !GraphValueMatchesType(value, expectedType))
				return { EGraphError::InvalidDefault, inputPin.Node, inputPin.Pin, 0, "ShaderGraph.InvalidColor" };

			if (expectedType == EGraphPinType::Float)
			{
				outExpression = FormatFloat(std::get<float>(value));
				return {};
			}
			if (expectedType == EGraphPinType::Float3)
			{
				outExpression = FormatFloat3(std::get<std::array<float, 3>>(value));
				return {};
			}
			if (expectedType == EGraphPinType::Float4)
			{
				outExpression = FormatFloat4(std::get<std::array<float, 4>>(value));
				return {};
			}

			return { EGraphError::UnsupportedCompilation, inputPin.Node, inputPin.Pin, 0, "ShaderGraph.UnsupportedNode" };
		}

		GraphDiagnostic ResolveProducerOutput(
			const Graph& graph, const GraphRegistry& registry,
			GraphPinReference outputPin, EGraphPinType expectedType,
			std::vector<GraphPinReference>& callStack, std::string& outExpression)
		{
			const auto* producer = graph.FindNode(outputPin.Node);
			if (!producer)
				return { EGraphError::PinNotFound, outputPin.Node, outputPin.Pin, 0, "ShaderGraph.InvalidInput" };

			const auto* outPinSchema = registry.FindPin(producer->Type, outputPin.Pin);
			if (!outPinSchema || outPinSchema->Direction != EGraphPinDirection::Output || outPinSchema->Type != expectedType)
				return { EGraphError::UnsupportedCompilation, outputPin.Node, outputPin.Pin, 0, "ShaderGraph.UnsupportedNode" };

			const auto& type = producer->Type;
			if (type == "Core.Float" || type == "Core.Float3" || type == "Core.Float4")
			{
				if (outputPin.Pin != "result")
					return { EGraphError::UnsupportedCompilation, outputPin.Node, outputPin.Pin, 0, "ShaderGraph.UnsupportedNode" };
				return ResolveInputExpression(graph, registry, { outputPin.Node, "value" }, expectedType, callStack, outExpression);
			}

			if (type == "Core.Reroute.Float" || type == "Core.Reroute.Float3" || type == "Core.Reroute.Float4")
			{
				if (outputPin.Pin != "Out")
					return { EGraphError::UnsupportedCompilation, outputPin.Node, outputPin.Pin, 0, "ShaderGraph.UnsupportedNode" };
				return ResolveInputExpression(graph, registry, { outputPin.Node, "In" }, expectedType, callStack, outExpression);
			}

			if (type == "Math.MakeFloat3")
			{
				if (outputPin.Pin != "result" || expectedType != EGraphPinType::Float3)
					return { EGraphError::UnsupportedCompilation, outputPin.Node, outputPin.Pin, 0, "ShaderGraph.UnsupportedNode" };

				std::string x, y, z;
				auto res = ResolveInputExpression(graph, registry, { outputPin.Node, "x" }, EGraphPinType::Float, callStack, x);
				if (!res.Succeeded()) return res;
				res = ResolveInputExpression(graph, registry, { outputPin.Node, "y" }, EGraphPinType::Float, callStack, y);
				if (!res.Succeeded()) return res;
				res = ResolveInputExpression(graph, registry, { outputPin.Node, "z" }, EGraphPinType::Float, callStack, z);
				if (!res.Succeeded()) return res;

				outExpression = "float3(" + x + ", " + y + ", " + z + ")";
				return {};
			}

			if (type == "Math.MakeFloat4")
			{
				if (outputPin.Pin != "result" || expectedType != EGraphPinType::Float4)
					return { EGraphError::UnsupportedCompilation, outputPin.Node, outputPin.Pin, 0, "ShaderGraph.UnsupportedNode" };

				std::string x, y, z, w;
				auto res = ResolveInputExpression(graph, registry, { outputPin.Node, "x" }, EGraphPinType::Float, callStack, x);
				if (!res.Succeeded()) return res;
				res = ResolveInputExpression(graph, registry, { outputPin.Node, "y" }, EGraphPinType::Float, callStack, y);
				if (!res.Succeeded()) return res;
				res = ResolveInputExpression(graph, registry, { outputPin.Node, "z" }, EGraphPinType::Float, callStack, z);
				if (!res.Succeeded()) return res;
				res = ResolveInputExpression(graph, registry, { outputPin.Node, "w" }, EGraphPinType::Float, callStack, w);
				if (!res.Succeeded()) return res;

				outExpression = "float4(" + x + ", " + y + ", " + z + ", " + w + ")";
				return {};
			}

			if (type == "Math.BreakFloat3")
			{
				if (expectedType != EGraphPinType::Float)
					return { EGraphError::UnsupportedCompilation, outputPin.Node, outputPin.Pin, 0, "ShaderGraph.UnsupportedNode" };

				const auto links = graph.GetLinks();
				const auto link = std::find_if(links.begin(), links.end(),
					[&](const GraphLink& item) { return item.To == GraphPinReference{ outputPin.Node, "value" }; });

				if (link == links.end())
				{
					const auto overrideValue = producer->InputValues.find("value");
					const auto* pin = registry.FindPin(type, "value");
					const auto& val = overrideValue != producer->InputValues.end() ? overrideValue->second : pin->DefaultValue;
					if (!IsFiniteGraphValue(val) || !GraphValueMatchesType(val, EGraphPinType::Float3))
						return { EGraphError::InvalidDefault, outputPin.Node, "value", 0, "ShaderGraph.InvalidColor" };

					const auto& arr = std::get<std::array<float, 3>>(val);
					float ch = 0.0f;
					if (outputPin.Pin == "x") ch = arr[0];
					else if (outputPin.Pin == "y") ch = arr[1];
					else if (outputPin.Pin == "z") ch = arr[2];
					else return { EGraphError::PinNotFound, outputPin.Node, outputPin.Pin, 0, "ShaderGraph.InvalidInput" };

					outExpression = FormatFloat(ch);
					return {};
				}

				std::string vecExpr;
				const auto res = ResolveInputExpression(
					graph, registry, { outputPin.Node, "value" }, EGraphPinType::Float3, callStack, vecExpr);
				if (!res.Succeeded()) return res;

				outExpression = "(" + vecExpr + ")." + outputPin.Pin;
				return {};
			}

			if (type == "Math.BreakFloat4")
			{
				if (expectedType != EGraphPinType::Float)
					return { EGraphError::UnsupportedCompilation, outputPin.Node, outputPin.Pin, 0, "ShaderGraph.UnsupportedNode" };

				const auto links = graph.GetLinks();
				const auto link = std::find_if(links.begin(), links.end(),
					[&](const GraphLink& item) { return item.To == GraphPinReference{ outputPin.Node, "value" }; });

				if (link == links.end())
				{
					const auto overrideValue = producer->InputValues.find("value");
					const auto* pin = registry.FindPin(type, "value");
					const auto& val = overrideValue != producer->InputValues.end() ? overrideValue->second : pin->DefaultValue;
					if (!IsFiniteGraphValue(val) || !GraphValueMatchesType(val, EGraphPinType::Float4))
						return { EGraphError::InvalidDefault, outputPin.Node, "value", 0, "ShaderGraph.InvalidColor" };

					const auto& arr = std::get<std::array<float, 4>>(val);
					float ch = 0.0f;
					if (outputPin.Pin == "x") ch = arr[0];
					else if (outputPin.Pin == "y") ch = arr[1];
					else if (outputPin.Pin == "z") ch = arr[2];
					else if (outputPin.Pin == "w") ch = arr[3];
					else return { EGraphError::PinNotFound, outputPin.Node, outputPin.Pin, 0, "ShaderGraph.InvalidInput" };

					outExpression = FormatFloat(ch);
					return {};
				}

				std::string vecExpr;
				const auto res = ResolveInputExpression(
					graph, registry, { outputPin.Node, "value" }, EGraphPinType::Float4, callStack, vecExpr);
				if (!res.Succeeded()) return res;

				outExpression = "(" + vecExpr + ")." + outputPin.Pin;
				return {};
			}

			return { EGraphError::UnsupportedCompilation, outputPin.Node, outputPin.Pin, 0, "ShaderGraph.UnsupportedNode" };
		}
	}

	GraphDiagnostic ShaderGraphCompiler::GenerateColorHlsl(
		const Graph& graph, const GraphRegistry& registry, std::string& outSource)
	{
		const GraphNode* outputNode = nullptr;
		const auto diag = ValidateShaderGraph(graph, registry, outputNode);
		if (!diag.Succeeded()) return diag;

		std::string colorLiteral;
		std::vector<GraphPinReference> callStack;

		if (outputNode->Type == "Shader.ColorOutput")
		{
			const auto res = ResolveInputExpression(
				graph, registry, { outputNode->Id, "color" }, EGraphPinType::Float4, callStack, colorLiteral);
			if (!res.Succeeded()) return res;
		}
		else if (outputNode->Type == "Shader.MaterialOutput")
		{
			const auto res = ResolveInputExpression(
				graph, registry, { outputNode->Id, "baseColor" }, EGraphPinType::Float4, callStack, colorLiteral);
			if (!res.Succeeded()) return res;
		}

		std::string source = "float4 main() : SV_Target0\n{\n\treturn " + colorLiteral + ";\n}\n";
		outSource.swap(source);
		return {};
	}

	GraphDiagnostic ShaderGraphCompiler::GenerateUnlitMaterialHlsl(
		const Graph& graph, const GraphRegistry& registry, std::string& outSource)
	{
		const GraphNode* outputNode = nullptr;
		const auto diag = ValidateShaderGraph(graph, registry, outputNode);
		if (!diag.Succeeded()) return diag;

		std::string colorLiteral;
		std::vector<GraphPinReference> callStack;

		if (outputNode->Type == "Shader.ColorOutput")
		{
			const auto res = ResolveInputExpression(
				graph, registry, { outputNode->Id, "color" }, EGraphPinType::Float4, callStack, colorLiteral);
			if (!res.Succeeded()) return res;
		}
		else if (outputNode->Type == "Shader.MaterialOutput")
		{
			const auto res = ResolveInputExpression(
				graph, registry, { outputNode->Id, "baseColor" }, EGraphPinType::Float4, callStack, colorLiteral);
			if (!res.Succeeded()) return res;
		}

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

		source += colorLiteral;
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

	GraphDiagnostic ShaderGraphCompiler::GenerateGBufferMaterialHlsl(
		const Graph& graph, const GraphRegistry& registry, std::string& outSource)
	{
		const GraphNode* outputNode = nullptr;
		const auto diag = ValidateShaderGraph(graph, registry, outputNode);
		if (!diag.Succeeded()) return diag;

		std::string baseColor = "float4(1.00000000e+00f, 1.00000000e+00f, 1.00000000e+00f, 1.00000000e+00f)";
		std::string roughness = "2.00000000e-01f";
		std::string metallic = "0.00000000e+00f";
		std::string cavity = "5.00000000e-01f";
		std::string ao = "1.00000000e+00f";
		std::string emissive = "float3(0.00000000e+00f, 0.00000000e+00f, 0.00000000e+00f)";

		std::vector<GraphPinReference> callStack;

		if (outputNode->Type == "Shader.ColorOutput")
		{
			const auto res = ResolveInputExpression(
				graph, registry, { outputNode->Id, "color" }, EGraphPinType::Float4, callStack, baseColor);
			if (!res.Succeeded()) return res;
		}
		else if (outputNode->Type == "Shader.MaterialOutput")
		{
			auto res = ResolveInputExpression(
				graph, registry, { outputNode->Id, "baseColor" }, EGraphPinType::Float4, callStack, baseColor);
			if (!res.Succeeded()) return res;

			res = ResolveInputExpression(
				graph, registry, { outputNode->Id, "roughness" }, EGraphPinType::Float, callStack, roughness);
			if (!res.Succeeded()) return res;

			res = ResolveInputExpression(
				graph, registry, { outputNode->Id, "metallic" }, EGraphPinType::Float, callStack, metallic);
			if (!res.Succeeded()) return res;

			res = ResolveInputExpression(
				graph, registry, { outputNode->Id, "cavity" }, EGraphPinType::Float, callStack, cavity);
			if (!res.Succeeded()) return res;

			res = ResolveInputExpression(
				graph, registry, { outputNode->Id, "ao" }, EGraphPinType::Float, callStack, ao);
			if (!res.Succeeded()) return res;

			res = ResolveInputExpression(
				graph, registry, { outputNode->Id, "emissive" }, EGraphPinType::Float3, callStack, emissive);
			if (!res.Succeeded()) return res;
		}

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
	float4 diffuse = ()" + baseColor + R"() * input.Color;
	if (diffuse.a < 0.01f) discard;

	float3 N = normalize(input.Normal);
	float roughness = clamp()" + roughness + R"(, 0.04f, 1.0f);
	float metallic = saturate()" + metallic + R"();
	float cavity = clamp()" + cavity + R"(, 0.0f, 1.0f);
	float ao = clamp()" + ao + R"(, 0.0f, 1.0f);
	float3 emission = )" + emissive + R"(;

	uint defaultMeshFlags = FLAG_RECEIVE_SHADOWS | FLAG_RECEIVE_DECALS | FLAG_APPLY_SSAO;

	output.Diffuse = diffuse;
	output.Normal = float4(N * 0.5f + 0.5f, PackShadingData(SHADING_MODEL_DEFAULT_LIT, defaultMeshFlags));
	output.PBR = float4(roughness, metallic, cavity, ao);
	output.Emission = float4(emission, 1.0f);
	output.Position = float4(input.WorldPos, input.PickingId);
	return output;
}
)";

		outSource.swap(source);
		return {};
	}
}