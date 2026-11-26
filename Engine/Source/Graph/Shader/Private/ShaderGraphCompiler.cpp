#include "ShaderGraphCompiler.h"
#include "GraphValue.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <optional>
#include <sstream>
#include <unordered_set>
#include <vector>

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

	std::string FormatFloat2(const std::array<float, 2>& value)
	{
		return "float2(" + FormatFloat(value[0]) + ", " + FormatFloat(value[1]) + ")";
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

	std::string SanitizeCustomMemberName(const std::string& rawName)
	{
		std::string out = "custom_";
		out.reserve(out.size() + rawName.size());
		for (unsigned char ch : rawName)
		{
			if (std::isalnum(ch) || ch == '_')
				out.push_back(static_cast<char>(ch));
			else
				out.push_back('_');
		}
		return out;
	}

	const char* HlslTypeForParameter(EMaterialParameterType type)
	{
		switch (type)
		{
		case EMaterialParameterType::Float: return "float";
		case EMaterialParameterType::Float2: return "float2";
		case EMaterialParameterType::Float3: return "float3";
		case EMaterialParameterType::Float4: return "float4";
		case EMaterialParameterType::Int: return "int";
		case EMaterialParameterType::Bool: return "uint";
		default: return "float";
		}
	}

	std::string BuildMaterialConstantsHlsl(const MaterialBufferLayout& layout)
	{
		std::ostringstream out;
		out << "// -- Material Constant Buffer (Space 3, b0 / RHI Slot 2) --\n";
		out << "struct MaterialConstants\n";
		out << "{\n";
		out << "uint   materialFlags;\n";
		out << "float  roughness;\n";
		out << "float  metallic;\n";
		out << "float  cavity;\n";
		out << "float4 baseColor;\n";
		out << "float3 emissiveColor;\n";
		out << "float  ao;\n";

		uint32 cursor = sizeof(MaterialConstants);
		uint32 padIndex = 0;

		for (const auto& entry : layout.Entries)
		{
			// 명시적 4바이트 패딩 삽입으로 C++ 레이아웃 오프셋과 HLSL 실제 오프셋 100% 일치 보장
			while (cursor < entry.Offset)
			{
				out << "float  _pad" << padIndex++ << ";\n";
				cursor += 4u;
			}
			out << HlslTypeForParameter(entry.Type) << " " << SanitizeCustomMemberName(entry.Name) << ";\n";
			cursor = entry.Offset + entry.Size;
		}

		while (cursor < layout.TotalBufferSize)
		{
			out << "float  _pad" << padIndex++ << ";\n";
			cursor += 4u;
		}

		out << "};\n\n";
		out << "ConstantBuffer<MaterialConstants> gMaterial : register(b0, space3);\n";
		return out.str();
	}

	constexpr int GraphTextureSlotCount = 4;
	constexpr std::string_view DiffuseTextureDeclaration =
		"Texture2D diffuseMap : register(t0, space1);\nSamplerState materialSampler : register(s0, space1);\n";

	// 지형 도메인 생성 중인지. Terrain.* 노드는 terrainPixel(TerrainBeginPixel 결과)을 읽으므로 그 생성기 밖에서는 컴파일하지 않는다.
	thread_local bool t_TerrainDomain = false;

	struct TerrainDomainScope
	{
		TerrainDomainScope() { t_TerrainDomain = true; }
		~TerrainDomainScope() { t_TerrainDomain = false; }
	};

	// ForwardTransparent 패스 생성 중인지. Shader.ScreenUV / Shader.SceneColor 노드는 SceneColorCopy 버퍼를 읽으므로 그 생성기 밖에서는 컴파일하지 않는다.
	thread_local bool t_ForwardTransparentDomain = false;

	struct ForwardTransparentScope
	{
		ForwardTransparentScope() { t_ForwardTransparentDomain = true; }
		~ForwardTransparentScope() { t_ForwardTransparentDomain = false; }
	};

	// Terrain.* 노드의 layer 상수(0~7). 범위 밖이면 -1.
	int TerrainLayerIndex(const GraphNode& node)
	{
		const auto value = node.InputValues.find("layer");
		if (value == node.InputValues.end()) return 0;
		const auto* layer = std::get_if<std::int64_t>(&value->second);
		return layer && *layer >= 0 && *layer < 8 ? static_cast<int>(*layer) : -1;
	}

	int GraphTextureSlot(const GraphNode& node)
	{
		const auto value = node.InputValues.find("slot");
		if (value == node.InputValues.end()) return 0;
		const auto* slot = std::get_if<std::int64_t>(&value->second);
		return slot && *slot >= 0 && *slot < GraphTextureSlotCount ? static_cast<int>(*slot) : -1;
	}

	std::string BuildGraphTextureHlsl(const Graph& graph, bool declareDiffuse = false)
	{
		std::array<bool, GraphTextureSlotCount> slots{};
		for (const auto& node : graph.GetNodes())
		{
			if (node.Type != "Shader.TextureSample") continue;
			const int slot = GraphTextureSlot(node);
			if (slot >= 0) slots[slot] = true;
		}

		std::ostringstream out;
		if (declareDiffuse || slots[0]) out << DiffuseTextureDeclaration;
		for (int slot = 0; slot < GraphTextureSlotCount; ++slot)
		{
			if (!slots[slot]) continue;
			const auto suffix = std::to_string(slot);
			const auto texture = slot == 0 ? "diffuseMap" : "graphTexture" + suffix;
			const auto sampler = slot == 0 ? "materialSampler" : "graphSampler" + suffix;
			if (slot != 0)
			{
				out << "Texture2D " << texture << " : register(t" << slot << ", space1);\n";
				out << "SamplerState " << sampler << " : register(s" << slot << ", space1);\n";
			}
			out << "float4 SampleGraphTexture" << slot << "(float2 uv)\n{\n"
				<< "\tif ((gMaterial.materialFlags & " << (1u << slot) << "u) == 0u) return float4(1, 1, 1, 1);\n"
				<< "\treturn " << texture << ".Sample(" << sampler << ", uv);\n}\n";
		}
		return out.str();
	}

	enum class EShaderStage
	{
		Vertex,
		Pixel
	};

	bool HasInputLink(const Graph& graph, GraphNodeId nodeId, const std::string& pinName)
	{
		const auto links = graph.GetLinks();
		return std::any_of(links.begin(), links.end(), [&](const GraphLink& link)
		{
			return link.To.Node == nodeId && link.To.Pin == pinName;
		});
	}

	std::string BuildNormalCalculationHlsl(const std::string& normalExpr, bool hasNormalLink)
	{
		if (!hasNormalLink)
			return "float3 N = normalize(input.Normal);";

		return "float3 baseN = normalize(input.Normal);\n"
			"float3 tangentSpaceNormal = normalize(" + normalExpr + ");\n"
			"float3 orthoT = input.Tangent - dot(input.Tangent, baseN) * baseN;\n"
			"float3 T = dot(orthoT, orthoT) > 1e-6f ? normalize(orthoT) : float3(1.0f, 0.0f, 0.0f);\n"
			"float3 B = cross(baseN, T);\n"
			"float3x3 TBN = float3x3(T, B, baseN);\n"
			"float3 N = normalize(mul(tangentSpaceNormal, TBN));";
	}

	bool IsSupportedShaderNode(const std::string& type)
	{
		if (type.rfind("Variable.Get.", 0) == 0 && type.size() > 13)
			return true;

		static const std::unordered_set<std::string> supported{
			"Shader.ColorOutput", "Shader.MaterialOutput", "Shader.TextureSample",
			"Shader.ScreenUV", "Shader.SceneColor",
			"Shader.TexCoord0", "Shader.TexCoord1",
			"Shader.Time", "Shader.WorldPosition", "Shader.WorldNormal", "Shader.ViewDirection", "Shader.Fresnel",
			"Core.Float", "Core.Float2", "Core.Float3", "Core.Float4",
			"Core.Reroute.Float", "Core.Reroute.Float2", "Core.Reroute.Float3", "Core.Reroute.Float4", "Core.Comment",
			"Math.AddFloat", "Math.SubtractFloat", "Math.MultiplyFloat", "Math.DivideFloat",
			"Math.LerpFloat", "Math.ClampFloat", "Math.SaturateFloat", "Math.PowFloat",
			"Math.AbsFloat", "Math.MinFloat", "Math.MaxFloat",
			"Math.SinFloat", "Math.CosFloat", "Math.StepFloat", "Math.SmoothStepFloat",
			"Math.AddFloat2", "Math.SubtractFloat2", "Math.MultiplyFloat2",
			"Math.DotFloat2", "Math.NormalizeFloat2", "Math.LerpFloat2",
			"Math.AddFloat3", "Math.SubtractFloat3", "Math.MultiplyFloat3", "Math.ScaleFloat3",
			"Math.DotFloat3", "Math.NormalizeFloat3", "Math.LerpFloat3",
			"Math.AddFloat4", "Math.SubtractFloat4", "Math.MultiplyFloat4", "Math.ScaleFloat4",
			"Math.DotFloat4", "Math.NormalizeFloat4", "Math.LerpFloat4",
			"Math.MakeFloat3", "Math.BreakFloat3",
			"Math.MakeFloat4", "Math.BreakFloat4",
			"Terrain.LayerWeight", "Terrain.LayerSample", "Terrain.LayerBlend", "Terrain.Slope", "Terrain.Height"
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
		GraphPinReference outputPin, EGraphPinType expectedType, EShaderStage stage,
		std::vector<GraphPinReference>& callStack, std::string& outExpression);

	GraphDiagnostic ResolveInputExpression(
		const Graph& graph, const GraphRegistry& registry,
		GraphPinReference inputPin, EGraphPinType expectedType, EShaderStage stage,
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
				graph, registry, link->From, expectedType, stage, callStack, outExpression);
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
		if (expectedType == EGraphPinType::Float2)
		{
			outExpression = FormatFloat2(std::get<std::array<float, 2>>(value));
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
		if (expectedType == EGraphPinType::Int)
		{
			outExpression = std::to_string(std::get<std::int64_t>(value));
			return {};
		}
		if (expectedType == EGraphPinType::Bool)
		{
			outExpression = std::get<bool>(value) ? "1u" : "0u";
			return {};
		}

		return { EGraphError::UnsupportedCompilation, inputPin.Node, inputPin.Pin, 0, "ShaderGraph.UnsupportedNode" };
	}

	GraphDiagnostic ResolveProducerOutput(
		const Graph& graph, const GraphRegistry& registry,
		GraphPinReference outputPin, EGraphPinType expectedType, EShaderStage stage,
		std::vector<GraphPinReference>& callStack, std::string& outExpression)
	{
		const auto* producer = graph.FindNode(outputPin.Node);
		if (!producer)
			return { EGraphError::PinNotFound, outputPin.Node, outputPin.Pin, 0, "ShaderGraph.InvalidInput" };

		const auto* outPinSchema = registry.FindPin(producer->Type, outputPin.Pin);
		if (!outPinSchema || outPinSchema->Direction != EGraphPinDirection::Output || outPinSchema->Type != expectedType)
			return { EGraphError::UnsupportedCompilation, outputPin.Node, outputPin.Pin, 0, "ShaderGraph.UnsupportedNode" };

		const auto& type = producer->Type;
		const auto resolvePin = [&](const char* pinKey, EGraphPinType pinType, std::string& dst)
		{
			return ResolveInputExpression(graph, registry, { outputPin.Node, pinKey }, pinType, stage, callStack, dst);
		};

		if (type.rfind("Variable.Get.", 0) == 0 && type.size() > 13)
		{
			if (outputPin.Pin != "value")
				return { EGraphError::UnsupportedCompilation, outputPin.Node, outputPin.Pin, 0, "ShaderGraph.UnsupportedNode" };
			const std::string varName = type.substr(13);
			if (!graph.FindVariable(varName))
				return { EGraphError::PinNotFound, outputPin.Node, outputPin.Pin, 0, "ShaderGraph.InvalidInput" };
			outExpression = "gMaterial." + SanitizeCustomMemberName(varName);
			return {};
		}

		if (type == "Core.Float" || type == "Core.Float2" || type == "Core.Float3" || type == "Core.Float4")
		{
			if (outputPin.Pin != "result")
				return { EGraphError::UnsupportedCompilation, outputPin.Node, outputPin.Pin, 0, "ShaderGraph.UnsupportedNode" };
			return resolvePin("value", expectedType, outExpression);
		}

		if (type == "Core.Reroute.Float" || type == "Core.Reroute.Float2" || type == "Core.Reroute.Float3" || type == "Core.Reroute.Float4")
		{
			if (outputPin.Pin != "Out")
				return { EGraphError::UnsupportedCompilation, outputPin.Node, outputPin.Pin, 0, "ShaderGraph.UnsupportedNode" };
			return resolvePin("In", expectedType, outExpression);
		}

		if (type.rfind("Terrain.", 0) == 0)
		{
			if (!t_TerrainDomain)
				return { EGraphError::UnsupportedCompilation, producer->Id, outputPin.Pin, 0, "ShaderGraph.TerrainDomainOnly" };
			if (stage == EShaderStage::Vertex)
				return { EGraphError::UnsupportedCompilation, producer->Id, outputPin.Pin, 0, "ShaderGraph.TerrainPixelOnly" };

			if (type == "Terrain.Slope") { outExpression = "terrainPixel.Slope"; return {}; }
			if (type == "Terrain.Height") { outExpression = "terrainPixel.Height"; return {}; }

			std::string color;
			if (type == "Terrain.LayerBlend")
				color = "TerrainBlendLayers(terrainPixel)";
			else
			{
				const int layer = TerrainLayerIndex(*producer);
				if (layer < 0)
					return { EGraphError::InvalidDefault, producer->Id, "layer", 0, "ShaderGraph.TerrainLayerRange" };
				if (type == "Terrain.LayerWeight")
				{
					outExpression = "TerrainLayerWeight(terrainPixel, " + std::to_string(layer) + "u)";
					return {};
				}
				color = "TerrainSampleLayer(terrainPixel, " + std::to_string(layer) + "u)";
			}
			outExpression = outputPin.Pin == "rgb" ? "(" + color + ").rgb" : color;
			return {};
		}

		if (type == "Shader.TexCoord0" || type == "Shader.TexCoord1")
		{
			// 지형 정점 셰이더는 두 번째 UV를 내지 않는다.
			if (t_TerrainDomain && type == "Shader.TexCoord1")
				return { EGraphError::UnsupportedCompilation, producer->Id, outputPin.Pin, 0, "ShaderGraph.TerrainNoUV1" };
			if (outputPin.Pin != "uv" || expectedType != EGraphPinType::Float2)
				return { EGraphError::UnsupportedCompilation, outputPin.Node, outputPin.Pin, 0, "ShaderGraph.UnsupportedNode" };
			outExpression = type == "Shader.TexCoord0" ? "input.TexCoord" : "input.TexCoord1";
			return {};
		}

		if (type == "Shader.Time")
		{
			if (outputPin.Pin != "time" || expectedType != EGraphPinType::Float)
				return { EGraphError::UnsupportedCompilation, outputPin.Node, outputPin.Pin, 0, "ShaderGraph.UnsupportedNode" };
			outExpression = "commonData.Time";
			return {};
		}

		if (type == "Shader.WorldPosition")
		{
			if (outputPin.Pin != "position" || expectedType != EGraphPinType::Float3)
				return { EGraphError::UnsupportedCompilation, outputPin.Node, outputPin.Pin, 0, "ShaderGraph.UnsupportedNode" };
			outExpression = stage == EShaderStage::Vertex ? "worldPosition.xyz" : "input.WorldPos";
			return {};
		}

		if (type == "Shader.WorldNormal")
		{
			if (outputPin.Pin != "normal" || expectedType != EGraphPinType::Float3)
				return { EGraphError::UnsupportedCompilation, outputPin.Node, outputPin.Pin, 0, "ShaderGraph.UnsupportedNode" };
			outExpression = stage == EShaderStage::Vertex ? "worldNormal" : "normalize(input.Normal)";
			return {};
		}

		if (type == "Shader.ViewDirection")
		{
			if (outputPin.Pin != "direction" || expectedType != EGraphPinType::Float3)
				return { EGraphError::UnsupportedCompilation, outputPin.Node, outputPin.Pin, 0, "ShaderGraph.UnsupportedNode" };
			outExpression = stage == EShaderStage::Vertex
				? "normalize(commonData.CameraPos - worldPosition.xyz)"
				: "normalize(commonData.CameraPos - input.WorldPos)";
			return {};
		}

		if (type == "Shader.Fresnel")
		{
			if (outputPin.Pin != "result" || expectedType != EGraphPinType::Float)
				return { EGraphError::UnsupportedCompilation, outputPin.Node, outputPin.Pin, 0, "ShaderGraph.UnsupportedNode" };

			std::string normalExpr = stage == EShaderStage::Vertex ? "worldNormal" : "normalize(input.Normal)";
			if (HasInputLink(graph, producer->Id, "normal"))
			{
				auto res = resolvePin("normal", EGraphPinType::Float3, normalExpr);
				if (!res.Succeeded()) return res;
			}

			std::string viewExpr = stage == EShaderStage::Vertex
				? "normalize(commonData.CameraPos - worldPosition.xyz)"
				: "normalize(commonData.CameraPos - input.WorldPos)";
			if (HasInputLink(graph, producer->Id, "viewDir"))
			{
				auto res = resolvePin("viewDir", EGraphPinType::Float3, viewExpr);
				if (!res.Succeeded()) return res;
			}

			std::string powerExpr;
			auto res = resolvePin("power", EGraphPinType::Float, powerExpr);
			if (!res.Succeeded()) return res;

			outExpression = "pow(1.0f - saturate(dot(normalize(" + normalExpr + "), normalize(" + viewExpr + "))), " + powerExpr + ")";
			return {};
		}

		if (type == "Shader.TextureSample")
		{
			if (stage == EShaderStage::Vertex)
				return { EGraphError::UnsupportedCompilation, producer->Id, outputPin.Pin, 0, "ShaderGraph.TextureSamplePixelOnly" };

			const int slot = GraphTextureSlot(*producer);
			if (slot < 0)
				return { EGraphError::InvalidDefault, producer->Id, "slot", 0, "ShaderGraph.TextureSlotRange" };

			std::string uv = "input.TexCoord";
			if (HasInputLink(graph, producer->Id, "uv"))
			{
				auto result = resolvePin("uv", EGraphPinType::Float2, uv);
				if (!result.Succeeded()) return result;
			}

			const std::string sampleCall = "SampleGraphTexture" + std::to_string(slot) + "(" + uv + ")";
			if (outputPin.Pin == "rgba")
			{
				if (expectedType != EGraphPinType::Float4)
					return { EGraphError::UnsupportedCompilation, outputPin.Node, outputPin.Pin, 0, "ShaderGraph.UnsupportedNode" };
				outExpression = sampleCall;
			}
			else if (outputPin.Pin == "rgb")
			{
				if (expectedType != EGraphPinType::Float3)
					return { EGraphError::UnsupportedCompilation, outputPin.Node, outputPin.Pin, 0, "ShaderGraph.UnsupportedNode" };
				outExpression = "(" + sampleCall + ").rgb";
			}
			else if (outputPin.Pin == "r")
			{
				if (expectedType != EGraphPinType::Float)
					return { EGraphError::UnsupportedCompilation, outputPin.Node, outputPin.Pin, 0, "ShaderGraph.UnsupportedNode" };
				outExpression = "(" + sampleCall + ").r";
			}
			else if (outputPin.Pin == "g")
			{
				if (expectedType != EGraphPinType::Float)
					return { EGraphError::UnsupportedCompilation, outputPin.Node, outputPin.Pin, 0, "ShaderGraph.UnsupportedNode" };
				outExpression = "(" + sampleCall + ").g";
			}
			else if (outputPin.Pin == "b")
			{
				if (expectedType != EGraphPinType::Float)
					return { EGraphError::UnsupportedCompilation, outputPin.Node, outputPin.Pin, 0, "ShaderGraph.UnsupportedNode" };
				outExpression = "(" + sampleCall + ").b";
			}
			else if (outputPin.Pin == "a")
			{
				if (expectedType != EGraphPinType::Float)
					return { EGraphError::UnsupportedCompilation, outputPin.Node, outputPin.Pin, 0, "ShaderGraph.UnsupportedNode" };
				outExpression = "(" + sampleCall + ").a";
			}
			else
			{
				return { EGraphError::UnsupportedCompilation, outputPin.Node, outputPin.Pin, 0, "ShaderGraph.UnsupportedNode" };
			}
			return {};
		}

		if (type == "Shader.ScreenUV")
		{
			if (stage == EShaderStage::Vertex)
				return { EGraphError::UnsupportedCompilation, producer->Id, outputPin.Pin, 0, "ShaderGraph.ScreenUVPixelOnly" };
			if (!t_ForwardTransparentDomain)
				return { EGraphError::UnsupportedCompilation, producer->Id, outputPin.Pin, 0, "ShaderGraph.ForwardTransparentOnly" };
			if (outputPin.Pin != "uv" || expectedType != EGraphPinType::Float2)
				return { EGraphError::UnsupportedCompilation, outputPin.Node, outputPin.Pin, 0, "ShaderGraph.UnsupportedNode" };

			outExpression = "screenUV";
			return {};
		}

		if (type == "Shader.SceneColor")
		{
			if (stage == EShaderStage::Vertex)
				return { EGraphError::UnsupportedCompilation, producer->Id, outputPin.Pin, 0, "ShaderGraph.SceneColorPixelOnly" };
			if (!t_ForwardTransparentDomain)
				return { EGraphError::UnsupportedCompilation, producer->Id, outputPin.Pin, 0, "ShaderGraph.ForwardTransparentOnly" };

			std::string uv = "screenUV";
			if (HasInputLink(graph, producer->Id, "uv"))
			{
				auto result = resolvePin("uv", EGraphPinType::Float2, uv);
				if (!result.Succeeded()) return result;
			}

			const std::string sampleCall = "SampleSceneColor(" + uv + ", screenUV)";
			if (outputPin.Pin == "rgba")
			{
				if (expectedType != EGraphPinType::Float4)
					return { EGraphError::UnsupportedCompilation, outputPin.Node, outputPin.Pin, 0, "ShaderGraph.UnsupportedNode" };
				outExpression = sampleCall;
			}
			else if (outputPin.Pin == "rgb")
			{
				if (expectedType != EGraphPinType::Float3)
					return { EGraphError::UnsupportedCompilation, outputPin.Node, outputPin.Pin, 0, "ShaderGraph.UnsupportedNode" };
				outExpression = "(" + sampleCall + ").rgb";
			}
			else if (outputPin.Pin == "r")
			{
				if (expectedType != EGraphPinType::Float)
					return { EGraphError::UnsupportedCompilation, outputPin.Node, outputPin.Pin, 0, "ShaderGraph.UnsupportedNode" };
				outExpression = "(" + sampleCall + ").r";
			}
			else if (outputPin.Pin == "g")
			{
				if (expectedType != EGraphPinType::Float)
					return { EGraphError::UnsupportedCompilation, outputPin.Node, outputPin.Pin, 0, "ShaderGraph.UnsupportedNode" };
				outExpression = "(" + sampleCall + ").g";
			}
			else if (outputPin.Pin == "b")
			{
				if (expectedType != EGraphPinType::Float)
					return { EGraphError::UnsupportedCompilation, outputPin.Node, outputPin.Pin, 0, "ShaderGraph.UnsupportedNode" };
				outExpression = "(" + sampleCall + ").b";
			}
			else if (outputPin.Pin == "a")
			{
				if (expectedType != EGraphPinType::Float)
					return { EGraphError::UnsupportedCompilation, outputPin.Node, outputPin.Pin, 0, "ShaderGraph.UnsupportedNode" };
				outExpression = "(" + sampleCall + ").a";
			}
			else
			{
				return { EGraphError::UnsupportedCompilation, outputPin.Node, outputPin.Pin, 0, "ShaderGraph.UnsupportedNode" };
			}
			return {};
		}

		// 이항 산술 연산 (+, -, *, /)
		auto tryBinaryOp = [&](const char* op, EGraphPinType operandType) -> std::optional<GraphDiagnostic>
		{
			if (outputPin.Pin != "result" || expectedType != operandType)
				return GraphDiagnostic{ EGraphError::UnsupportedCompilation, outputPin.Node, outputPin.Pin, 0, "ShaderGraph.UnsupportedNode" };

			std::string a, b;
			auto res = resolvePin("a", operandType, a);
			if (!res.Succeeded()) return res;
			res = resolvePin("b", operandType, b);
			if (!res.Succeeded()) return res;

			outExpression = "(" + a + op + b + ")";
			return GraphDiagnostic{};
		};

		if (type == "Math.AddFloat")      return *tryBinaryOp(" + ", EGraphPinType::Float);
		if (type == "Math.SubtractFloat") return *tryBinaryOp(" - ", EGraphPinType::Float);
		if (type == "Math.MultiplyFloat") return *tryBinaryOp(" * ", EGraphPinType::Float);
		if (type == "Math.DivideFloat")   return *tryBinaryOp(" / ", EGraphPinType::Float);
		if (type == "Math.AddFloat2")      return *tryBinaryOp(" + ", EGraphPinType::Float2);
		if (type == "Math.SubtractFloat2") return *tryBinaryOp(" - ", EGraphPinType::Float2);
		if (type == "Math.MultiplyFloat2") return *tryBinaryOp(" * ", EGraphPinType::Float2);
		if (type == "Math.AddFloat3")      return *tryBinaryOp(" + ", EGraphPinType::Float3);
		if (type == "Math.SubtractFloat3") return *tryBinaryOp(" - ", EGraphPinType::Float3);
		if (type == "Math.MultiplyFloat3") return *tryBinaryOp(" * ", EGraphPinType::Float3);
		if (type == "Math.AddFloat4")      return *tryBinaryOp(" + ", EGraphPinType::Float4);
		if (type == "Math.SubtractFloat4") return *tryBinaryOp(" - ", EGraphPinType::Float4);
		if (type == "Math.MultiplyFloat4") return *tryBinaryOp(" * ", EGraphPinType::Float4);

		if (type == "Math.ScaleFloat3" || type == "Math.ScaleFloat4")
		{
			const auto vecType = (type == "Math.ScaleFloat3") ? EGraphPinType::Float3 : EGraphPinType::Float4;
			if (outputPin.Pin != "result" || expectedType != vecType)
				return { EGraphError::UnsupportedCompilation, outputPin.Node, outputPin.Pin, 0, "ShaderGraph.UnsupportedNode" };

			std::string value, scale;
			auto res = resolvePin("value", vecType, value);
			if (!res.Succeeded()) return res;
			res = resolvePin("scale", EGraphPinType::Float, scale);
			if (!res.Succeeded()) return res;

			outExpression = "(" + value + " * " + scale + ")";
			return {};
		}

		// 단항 HLSL 내장 함수
		auto tryUnaryFn = [&](const char* fn, EGraphPinType pinType) -> GraphDiagnostic
		{
			if (outputPin.Pin != "result" || expectedType != pinType)
				return { EGraphError::UnsupportedCompilation, outputPin.Node, outputPin.Pin, 0, "ShaderGraph.UnsupportedNode" };

			std::string val;
			auto res = resolvePin("value", pinType, val);
			if (!res.Succeeded()) return res;

			outExpression = std::string(fn) + "(" + val + ")";
			return {};
		};

		if (type == "Math.SaturateFloat")   return tryUnaryFn("saturate", EGraphPinType::Float);
		if (type == "Math.AbsFloat")        return tryUnaryFn("abs", EGraphPinType::Float);
		if (type == "Math.SinFloat")        return tryUnaryFn("sin", EGraphPinType::Float);
		if (type == "Math.CosFloat")        return tryUnaryFn("cos", EGraphPinType::Float);
		if (type == "Math.NormalizeFloat2") return tryUnaryFn("normalize", EGraphPinType::Float2);
		if (type == "Math.NormalizeFloat3") return tryUnaryFn("normalize", EGraphPinType::Float3);
		if (type == "Math.NormalizeFloat4") return tryUnaryFn("normalize", EGraphPinType::Float4);

		// 이항 HLSL 내장 함수
		auto tryBinaryFn = [&](const char* fn, const char* pin0, const char* pin1,
			EGraphPinType argType, EGraphPinType retType) -> GraphDiagnostic
		{
			if (outputPin.Pin != "result" || expectedType != retType)
				return { EGraphError::UnsupportedCompilation, outputPin.Node, outputPin.Pin, 0, "ShaderGraph.UnsupportedNode" };

			std::string p0, p1;
			auto res = resolvePin(pin0, argType, p0);
			if (!res.Succeeded()) return res;
			res = resolvePin(pin1, argType, p1);
			if (!res.Succeeded()) return res;

			outExpression = std::string(fn) + "(" + p0 + ", " + p1 + ")";
			return {};
		};

		if (type == "Math.MinFloat")  return tryBinaryFn("min", "a", "b", EGraphPinType::Float, EGraphPinType::Float);
		if (type == "Math.MaxFloat")  return tryBinaryFn("max", "a", "b", EGraphPinType::Float, EGraphPinType::Float);
		if (type == "Math.PowFloat")  return tryBinaryFn("pow", "base", "exponent", EGraphPinType::Float, EGraphPinType::Float);
		if (type == "Math.StepFloat") return tryBinaryFn("step", "edge", "value", EGraphPinType::Float, EGraphPinType::Float);
		if (type == "Math.DotFloat2") return tryBinaryFn("dot", "a", "b", EGraphPinType::Float2, EGraphPinType::Float);
		if (type == "Math.DotFloat3") return tryBinaryFn("dot", "a", "b", EGraphPinType::Float3, EGraphPinType::Float);
		if (type == "Math.DotFloat4") return tryBinaryFn("dot", "a", "b", EGraphPinType::Float4, EGraphPinType::Float);

		// 삼항 HLSL 내장 함수
		auto tryTernaryFn = [&](const char* fn, const char* pin0, EGraphPinType type0,
			const char* pin1, EGraphPinType type1, const char* pin2, EGraphPinType type2,
			EGraphPinType retType) -> GraphDiagnostic
		{
			if (outputPin.Pin != "result" || expectedType != retType)
				return { EGraphError::UnsupportedCompilation, outputPin.Node, outputPin.Pin, 0, "ShaderGraph.UnsupportedNode" };

			std::string p0, p1, p2;
			auto res = resolvePin(pin0, type0, p0);
			if (!res.Succeeded()) return res;
			res = resolvePin(pin1, type1, p1);
			if (!res.Succeeded()) return res;
			res = resolvePin(pin2, type2, p2);
			if (!res.Succeeded()) return res;

			outExpression = std::string(fn) + "(" + p0 + ", " + p1 + ", " + p2 + ")";
			return {};
		};

		if (type == "Math.LerpFloat")
			return tryTernaryFn("lerp", "a", EGraphPinType::Float, "b", EGraphPinType::Float, "t", EGraphPinType::Float, EGraphPinType::Float);
		if (type == "Math.LerpFloat2")
			return tryTernaryFn("lerp", "a", EGraphPinType::Float2, "b", EGraphPinType::Float2, "t", EGraphPinType::Float, EGraphPinType::Float2);
		if (type == "Math.LerpFloat3")
			return tryTernaryFn("lerp", "a", EGraphPinType::Float3, "b", EGraphPinType::Float3, "t", EGraphPinType::Float, EGraphPinType::Float3);
		if (type == "Math.LerpFloat4")
			return tryTernaryFn("lerp", "a", EGraphPinType::Float4, "b", EGraphPinType::Float4, "t", EGraphPinType::Float, EGraphPinType::Float4);
		if (type == "Math.ClampFloat")
			return tryTernaryFn("clamp", "value", EGraphPinType::Float, "minimum", EGraphPinType::Float, "maximum", EGraphPinType::Float, EGraphPinType::Float);
		if (type == "Math.SmoothStepFloat")
			return tryTernaryFn("smoothstep", "edge0", EGraphPinType::Float, "edge1", EGraphPinType::Float, "value", EGraphPinType::Float, EGraphPinType::Float);

		if (type == "Math.MakeFloat3")
		{
			if (outputPin.Pin != "result" || expectedType != EGraphPinType::Float3)
				return { EGraphError::UnsupportedCompilation, outputPin.Node, outputPin.Pin, 0, "ShaderGraph.UnsupportedNode" };

			std::string x, y, z;
			auto res = resolvePin("x", EGraphPinType::Float, x);
			if (!res.Succeeded()) return res;
			res = resolvePin("y", EGraphPinType::Float, y);
			if (!res.Succeeded()) return res;
			res = resolvePin("z", EGraphPinType::Float, z);
			if (!res.Succeeded()) return res;

			outExpression = "float3(" + x + ", " + y + ", " + z + ")";
			return {};
		}

		if (type == "Math.MakeFloat4")
		{
			if (outputPin.Pin != "result" || expectedType != EGraphPinType::Float4)
				return { EGraphError::UnsupportedCompilation, outputPin.Node, outputPin.Pin, 0, "ShaderGraph.UnsupportedNode" };

			std::string x, y, z, w;
			auto res = resolvePin("x", EGraphPinType::Float, x);
			if (!res.Succeeded()) return res;
			res = resolvePin("y", EGraphPinType::Float, y);
			if (!res.Succeeded()) return res;
			res = resolvePin("z", EGraphPinType::Float, z);
			if (!res.Succeeded()) return res;
			res = resolvePin("w", EGraphPinType::Float, w);
			if (!res.Succeeded()) return res;

			outExpression = "float4(" + x + ", " + y + ", " + z + ", " + w + ")";
			return {};
		}

		if (type == "Math.BreakFloat3")
		{
			if (expectedType != EGraphPinType::Float)
				return { EGraphError::UnsupportedCompilation, outputPin.Node, outputPin.Pin, 0, "ShaderGraph.UnsupportedNode" };

			if (!HasInputLink(graph, outputPin.Node, "value"))
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
			const auto res = resolvePin("value", EGraphPinType::Float3, vecExpr);
			if (!res.Succeeded()) return res;

			outExpression = "(" + vecExpr + ")." + outputPin.Pin;
			return {};
		}

		if (type == "Math.BreakFloat4")
		{
			if (expectedType != EGraphPinType::Float)
				return { EGraphError::UnsupportedCompilation, outputPin.Node, outputPin.Pin, 0, "ShaderGraph.UnsupportedNode" };

			if (!HasInputLink(graph, outputPin.Node, "value"))
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
			const auto res = resolvePin("value", EGraphPinType::Float4, vecExpr);
			if (!res.Succeeded()) return res;

			outExpression = "(" + vecExpr + ")." + outputPin.Pin;
			return {};
		}

		return { EGraphError::UnsupportedCompilation, outputPin.Node, outputPin.Pin, 0, "ShaderGraph.UnsupportedNode" };
	}

	GraphDiagnostic ResolveShaderOutputColor(const Graph& graph, const GraphRegistry& registry,
		const GraphNode*& outputNode, std::vector<GraphPinReference>& callStack, std::string& outColor)
	{
		const auto diagnostic = ValidateShaderGraph(graph, registry, outputNode);
		if (!diagnostic.Succeeded()) return diagnostic;

		const char* colorPin = outputNode->Type == "Shader.MaterialOutput" ? "baseColor" : "color";
		return ResolveInputExpression(graph, registry, { outputNode->Id, colorPin },
			EGraphPinType::Float4, EShaderStage::Pixel, callStack, outColor);
	}

	std::string BuildShadowDepthMaterialVSHlsl(const std::string& materialConstantsBlock, const std::string& wpoExpression)
	{
		return R"(#include "common.hlsli"

)" + materialConstantsBlock + R"(
struct VS_INPUT
{
float3 Position : POSITION;
float3 Normal : NORMAL;
float2 TexCoord : TEXCOORD0;
float3 Tangent : TANGENT;
float3 Binormal : BINORMAL;
float4 Color : COLOR;
float2 TexCoord1 : TEXCOORD1;
};

struct InstanceData
{
float4x4 WorldMatrix;
float4 PickingData;
};

StructuredBuffer<InstanceData> g_Instances : register(t0, space2);

float4 main(VS_INPUT input, uint instanceId : SV_InstanceID) : SV_POSITION
{
float4x4 world = g_Instances[instanceId].WorldMatrix;
float4 worldPosition = mul(world, float4(input.Position, 1.0f));

float3 axisX = float3(world[0][0], world[1][0], world[2][0]);
float3 axisY = float3(world[0][1], world[1][1], world[2][1]);
float3 axisZ = float3(world[0][2], world[1][2], world[2][2]);

float3 normalWorld = input.Normal.x * cross(axisY, axisZ) +
                    input.Normal.y * cross(axisZ, axisX) +
                    input.Normal.z * cross(axisX, axisY);

float determinantSign = dot(axisX, cross(axisY, axisZ)) < 0.0f ? -1.0f : 1.0f;
float3 worldNormal = normalize(normalWorld * determinantSign);

// [World Position Offset (WPO) 그래프 표현식 주입]
float3 worldPosOffset = )" + wpoExpression + R"(;
worldPosition.xyz += worldPosOffset;

return mul(commonData.ViewProjection, worldPosition);
}
)";
	}
}

MaterialBufferLayout ShaderGraphCompiler::BuildMaterialBufferLayout(const Graph& graph)
{
	MaterialBufferLayout layout;
	layout.TotalBufferSize = sizeof(MaterialConstants);
	uint32 cursor = sizeof(MaterialConstants);

	for (const auto& var : graph.GetVariables())
	{
		EMaterialParameterType paramType = EMaterialParameterType::Float;
		uint32 size = 0;

		switch (var.Type)
		{
		case EGraphPinType::Float:  paramType = EMaterialParameterType::Float;  size = 4u;  break;
		case EGraphPinType::Float2: paramType = EMaterialParameterType::Float2; size = 8u;  break;
		case EGraphPinType::Float3: paramType = EMaterialParameterType::Float3; size = 12u; break;
		case EGraphPinType::Float4: paramType = EMaterialParameterType::Float4; size = 16u; break;
		case EGraphPinType::Int:    paramType = EMaterialParameterType::Int;    size = 4u;  break;
		case EGraphPinType::Bool:   paramType = EMaterialParameterType::Bool;   size = 4u;  break;
		default: continue;
		}

		// HLSL cbuffer 패킹 규칙: 4바이트 정렬 후 16바이트 경계를 넘치면 다음 16바이트 경계로 점프
		uint32 offset = (cursor + 3u) & ~3u;
		if ((offset / 16u) != ((offset + size - 1u) / 16u))
			offset = (offset + 15u) & ~15u;

		layout.Entries.push_back({ var.Name, paramType, offset, size });
		cursor = offset + size;
	}

	layout.TotalBufferSize = (cursor + 15u) & ~15u;
	return layout;
}

std::vector<MaterialTextureDescriptor> ShaderGraphCompiler::CollectTextureDescriptors(const Graph& graph)
{
	std::vector<MaterialTextureDescriptor> descriptors;

	for (const auto& node : graph.GetNodes())
	{
		if (node.Type != "Shader.TextureSample") continue;

		const int slot = GraphTextureSlot(node);
		if (slot < 0 || slot >= GraphTextureSlotCount) continue;

		const uint32 uSlot = static_cast<uint32>(slot);

		std::string name;
		const auto itName = node.InputValues.find("name");
		if (itName != node.InputValues.end())
		{
			if (const auto* s = std::get_if<std::string>(&itName->second); s && !s->empty())
				name = *s;
		}
		if (name.empty())
		{
			const auto itSlotName = node.InputValues.find("slotName");
			if (itSlotName != node.InputValues.end())
			{
				if (const auto* s = std::get_if<std::string>(&itSlotName->second); s && !s->empty())
					name = *s;
			}
		}
		if (name.empty())
		{
			name = (uSlot == 0) ? "diffuseMap" : ("graphTexture" + std::to_string(uSlot));
		}

		auto existing = std::find_if(descriptors.begin(), descriptors.end(),
			[uSlot](const MaterialTextureDescriptor& d) { return d.Slot == uSlot; });

		if (existing != descriptors.end())
		{
			const std::string defaultName = (uSlot == 0) ? "diffuseMap" : ("graphTexture" + std::to_string(uSlot));
			if (existing->Name == defaultName && name != defaultName)
			{
				existing->Name = name;
				existing->NodeId = node.Id;
			}
		}
		else
		{
			descriptors.push_back({ uSlot, name, node.Id });
		}
	}

	std::sort(descriptors.begin(), descriptors.end(),
		[](const MaterialTextureDescriptor& a, const MaterialTextureDescriptor& b)
		{
			return a.Slot < b.Slot;
		});

	return descriptors;
}

std::vector<MaterialTextureDescriptor> ShaderGraphCompiler::CollectTextureParameters(const Graph& graph)
{
	return CollectTextureDescriptors(graph);
}

bool ShaderGraphCompiler::HasWorldPositionOffset(const Graph& graph)
{
	for (const auto& node : graph.GetNodes())
	{
		if (node.Type == "Shader.MaterialOutput")
			return HasInputLink(graph, node.Id, "worldPositionOffset");
	}
	return false;
}

GraphDiagnostic ShaderGraphCompiler::GenerateGBufferMaterialVS(
	const Graph& graph, const GraphRegistry& registry, std::string& outSource)
{
	ShaderGraphCompiledVS compiled;
	const auto diagnostic = GenerateGBufferMaterialVS(graph, registry, compiled);
	if (!diagnostic.Succeeded()) return diagnostic;

	outSource.swap(compiled.VertexShaderSource);
	return {};
}

GraphDiagnostic ShaderGraphCompiler::GenerateGBufferMaterialVS(
	const Graph& graph, const GraphRegistry& registry, ShaderGraphCompiledVS& outCompiled)
{
	const GraphNode* outputNode = nullptr;
	const auto diagnostic = ValidateShaderGraph(graph, registry, outputNode);
	if (!diagnostic.Succeeded()) return diagnostic;

	const MaterialBufferLayout layout = BuildMaterialBufferLayout(graph);
	const std::string materialConstantsBlock = BuildMaterialConstantsHlsl(layout);

	std::vector<GraphPinReference> callStack;
	std::string wpoExpression = "float3(0.0f, 0.0f, 0.0f)";
	std::string uv0Expression = "input.TexCoord";
	std::string uv1Expression = "input.TexCoord1";
	bool hasWpoLink = false;
	const auto links = graph.GetLinks();
	const auto hasInputLink = [&](const char* pinName) {
		return std::any_of(links.begin(), links.end(), [&](const GraphLink& link) {
			return link.To.Node == outputNode->Id && link.To.Pin == pinName;
		});
	};

	if (outputNode->Type == "Shader.MaterialOutput")
	{
		if (hasInputLink("worldPositionOffset"))
		{
			auto result = ResolveInputExpression(
				graph, registry, { outputNode->Id, "worldPositionOffset" }, EGraphPinType::Float3, EShaderStage::Vertex, callStack, wpoExpression);
			if (!result.Succeeded()) return result;
			hasWpoLink = true;
		}

		if (hasInputLink("customUV0"))
		{
			auto result = ResolveInputExpression(
				graph, registry, { outputNode->Id, "customUV0" }, EGraphPinType::Float2, EShaderStage::Vertex, callStack, uv0Expression);
			if (!result.Succeeded()) return result;
		}
		if (hasInputLink("customUV1"))
		{
			auto result = ResolveInputExpression(
				graph, registry, { outputNode->Id, "customUV1" }, EGraphPinType::Float2, EShaderStage::Vertex, callStack, uv1Expression);
			if (!result.Succeeded()) return result;
		}
	}

	std::string source = R"(#include "common.hlsli"

)" + materialConstantsBlock + R"(
struct VS_INPUT
{
float3 Position : POSITION;
float3 Normal : NORMAL;
float2 TexCoord : TEXCOORD0;
float3 Tangent : TANGENT;
float3 Binormal : BINORMAL;
float4 Color : COLOR;
float2 TexCoord1 : TEXCOORD1;
};

struct VS_OUTPUT
{
float4 Position : SV_POSITION;
float3 WorldPos : WORLD_POSITION;
float3 Normal : NORMAL;
float2 TexCoord : TEXCOORD0;
float2 TexCoord1 : TEXCOORD1;
float3 Tangent : TANGENT;
float4 Color : COLOR0;
nointerpolation float PickingId : TEXCOORD5;
nointerpolation float NoDecals : TEXCOORD6;
};

struct InstanceData
{
float4x4 WorldMatrix;
float4 PickingData;
};

StructuredBuffer<InstanceData> g_Instances : register(t0, space2);

VS_OUTPUT main(VS_INPUT input, uint instanceId : SV_InstanceID)
{
VS_OUTPUT output;
float4x4 world = g_Instances[instanceId].WorldMatrix;
float4 worldPosition = mul(world, float4(input.Position, 1.0f));

float3 axisX = float3(world[0][0], world[1][0], world[2][0]);
float3 axisY = float3(world[0][1], world[1][1], world[2][1]);
float3 axisZ = float3(world[0][2], world[1][2], world[2][2]);

float3 normalWorld = input.Normal.x * cross(axisY, axisZ) +
                    input.Normal.y * cross(axisZ, axisX) +
                    input.Normal.z * cross(axisX, axisY);

float determinantSign = dot(axisX, cross(axisY, axisZ)) < 0.0f ? -1.0f : 1.0f;
float3 worldNormal = normalize(normalWorld * determinantSign);

// [World Position Offset (WPO) 그래프 표현식 주입]
float3 worldPosOffset = )" + wpoExpression + R"(;
worldPosition.xyz += worldPosOffset;

output.Position = mul(commonData.ViewProjection, worldPosition);
output.WorldPos = worldPosition.xyz;
output.Normal = worldNormal;
output.TexCoord = )" + uv0Expression + R"(;
output.TexCoord1 = )" + uv1Expression + R"(;
output.Tangent = normalize(mul(world, float4(input.Tangent, 0.0f)).xyz);
output.Color = input.Color;
output.PickingId = g_Instances[instanceId].PickingData.x;
output.NoDecals = g_Instances[instanceId].PickingData.y;

return output;
}
)";

	outCompiled.VertexShaderSource.swap(source);
	if (hasWpoLink)
		outCompiled.ShadowVertexShaderSource = BuildShadowDepthMaterialVSHlsl(materialConstantsBlock, wpoExpression);
	else
		outCompiled.ShadowVertexShaderSource.clear();
	return {};
}

GraphDiagnostic ShaderGraphCompiler::GenerateShadowDepthMaterialVS(
	const Graph& graph, const GraphRegistry& registry, std::string& outShadowVertexShaderSource)
{
	const GraphNode* outputNode = nullptr;
	const auto diagnostic = ValidateShaderGraph(graph, registry, outputNode);
	if (!diagnostic.Succeeded()) return diagnostic;

	if (outputNode->Type != "Shader.MaterialOutput" || !HasInputLink(graph, outputNode->Id, "worldPositionOffset"))
	{
		outShadowVertexShaderSource.clear();
		return {};
	}

	std::vector<GraphPinReference> callStack;
	std::string wpoExpression;
	const auto result = ResolveInputExpression(
		graph, registry, { outputNode->Id, "worldPositionOffset" }, EGraphPinType::Float3, EShaderStage::Vertex, callStack, wpoExpression);
	if (!result.Succeeded()) return result;

	const MaterialBufferLayout layout = BuildMaterialBufferLayout(graph);
	const std::string materialConstantsBlock = BuildMaterialConstantsHlsl(layout);
	outShadowVertexShaderSource = BuildShadowDepthMaterialVSHlsl(materialConstantsBlock, wpoExpression);
	return {};
}

bool ShaderGraphCompiler::SupportsNode(const std::string& type)
{
	return IsSupportedShaderNode(type);
}

GraphDiagnostic ShaderGraphCompiler::GenerateColorHlsl(
	const Graph& graph, const GraphRegistry& registry, std::string& outSource)
{
	const GraphNode* outputNode = nullptr;
	std::string colorLiteral;
	std::vector<GraphPinReference> callStack;

	const auto diagnostic = ResolveShaderOutputColor(graph, registry, outputNode, callStack, colorLiteral);
	if (!diagnostic.Succeeded()) return diagnostic;

	std::string source;
	if (colorLiteral.find("commonData.") != std::string::npos)
	{
		source += "#include \"common.hlsli\"\n";
	}
	if (colorLiteral.find("gMaterial.") != std::string::npos ||
		colorLiteral.find("SampleGraphTexture") != std::string::npos)
	{
		source += BuildMaterialConstantsHlsl(BuildMaterialBufferLayout(graph));
		source += BuildGraphTextureHlsl(graph);
	}
	const bool needsInput = colorLiteral.find("input.") != std::string::npos;
	if (needsInput)
	{
		source += "struct PS_INPUT { float3 WorldPos : WORLD_POSITION; float3 Normal : NORMAL; "
			"float2 TexCoord : TEXCOORD0; float2 TexCoord1 : TEXCOORD1; };\n";
	}
	source += needsInput ? "float4 main(PS_INPUT input)" : "float4 main()";
	source += " : SV_Target0\n{\n\treturn " + colorLiteral + ";\n}\n";
	outSource.swap(source);
	return {};
}

GraphDiagnostic ShaderGraphCompiler::GenerateUnlitMaterialHlsl(
	const Graph& graph, const GraphRegistry& registry, std::string& outSource)
{
	const GraphNode* outputNode = nullptr;
	std::string colorLiteral;
	std::vector<GraphPinReference> callStack;

	const auto diagnostic = ResolveShaderOutputColor(graph, registry, outputNode, callStack, colorLiteral);
	if (!diagnostic.Succeeded()) return diagnostic;

	std::string source = R"(#include "common.hlsli"
#include "shading_flags.hlsli"

struct PS_INPUT
{
float4 Position : SV_POSITION;
float3 WorldPos : WORLD_POSITION;
float3 Normal : NORMAL;
float2 TexCoord : TEXCOORD0;
float2 TexCoord1 : TEXCOORD1;
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

)" + BuildMaterialConstantsHlsl(BuildMaterialBufferLayout(graph)) + BuildGraphTextureHlsl(graph) + R"(
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
	std::string baseColor;
	std::vector<GraphPinReference> callStack;

	const auto diagnostic = ValidateShaderGraph(graph, registry, outputNode);
	if (!diagnostic.Succeeded()) return diagnostic;

	const auto links = graph.GetLinks();
	const auto hasInputLink = [&](const std::string& pinName)
		{
			return std::any_of(links.begin(), links.end(), [&](const GraphLink& link)
				{
					return link.To.Node == outputNode->Id && link.To.Pin == pinName;
				});
		};

	// 1. BaseColor: 링크가 있으면 노드 표현식 해결, 없으면 Fast-Path GPU 상수버퍼(gMaterial.baseColor) 참조
	if (hasInputLink("baseColor"))
	{
		auto result = ResolveInputExpression(
			graph, registry, { outputNode->Id, "baseColor" }, EGraphPinType::Float4, EShaderStage::Pixel, callStack, baseColor);
		if (!result.Succeeded()) return result;
	}
	else
	{
		baseColor = "gMaterial.baseColor";
	}

	// 2. PBR 파라미터: 링크가 있으면 노드 표현식 해결, 없으면 gMaterial 상수버퍼 참조 (Fast-Path)
	std::string roughness = "gMaterial.roughness";
	std::string metallic = "gMaterial.metallic";
	std::string cavity = "gMaterial.cavity";
	std::string ao = "gMaterial.ao";
	std::string emissive = "gMaterial.emissiveColor";
	std::string normalExpr;
	std::string opacityMaskExpr;
	bool hasNormalLink = false;
	bool hasOpacityMaskLink = false;

	if (outputNode->Type == "Shader.MaterialOutput")
	{
		if (hasInputLink("roughness"))
		{
			auto result = ResolveInputExpression(
				graph, registry, { outputNode->Id, "roughness" }, EGraphPinType::Float, EShaderStage::Pixel, callStack, roughness);
			if (!result.Succeeded()) return result;
		}
		if (hasInputLink("metallic"))
		{
			auto result = ResolveInputExpression(
				graph, registry, { outputNode->Id, "metallic" }, EGraphPinType::Float, EShaderStage::Pixel, callStack, metallic);
			if (!result.Succeeded()) return result;
		}
		if (hasInputLink("cavity"))
		{
			auto result = ResolveInputExpression(
				graph, registry, { outputNode->Id, "cavity" }, EGraphPinType::Float, EShaderStage::Pixel, callStack, cavity);
			if (!result.Succeeded()) return result;
		}
		if (hasInputLink("ao"))
		{
			auto result = ResolveInputExpression(
				graph, registry, { outputNode->Id, "ao" }, EGraphPinType::Float, EShaderStage::Pixel, callStack, ao);
			if (!result.Succeeded()) return result;
		}
		if (hasInputLink("emissive"))
		{
			auto result = ResolveInputExpression(
				graph, registry, { outputNode->Id, "emissive" }, EGraphPinType::Float3, EShaderStage::Pixel, callStack, emissive);
			if (!result.Succeeded()) return result;
		}
		if (hasInputLink("normal"))
		{
			auto result = ResolveInputExpression(
				graph, registry, { outputNode->Id, "normal" }, EGraphPinType::Float3, EShaderStage::Pixel, callStack, normalExpr);
			if (!result.Succeeded()) return result;
			hasNormalLink = true;
		}
		if (hasInputLink("opacityMask"))
		{
			auto result = ResolveInputExpression(
				graph, registry, { outputNode->Id, "opacityMask" }, EGraphPinType::Float, EShaderStage::Pixel, callStack, opacityMaskExpr);
			if (!result.Succeeded()) return result;
			hasOpacityMaskLink = true;
		}
	}

	const MaterialBufferLayout layout = BuildMaterialBufferLayout(graph);
	const std::string materialConstantsBlock = BuildMaterialConstantsHlsl(layout);
	const bool useMaterialTexture = outputNode->Type == "Shader.MaterialOutput" &&
		baseColor.find("SampleGraphTexture0(") == std::string::npos;
	const char* materialDiffuseSample = useMaterialTexture
		? "if ((gMaterial.materialFlags & FLAG_HAS_DIFFUSE_MAP) != 0u)\n{\n\tdiffuse *= diffuseMap.Sample(materialSampler, input.TexCoord);\n}\n"
		: "";
	const std::string opacityMaskClip = hasOpacityMaskLink
		? "float opacityMask = " + opacityMaskExpr + ";\nclip(opacityMask - 0.5f);\n"
		: "";
	const std::string normalCalcBlock = BuildNormalCalculationHlsl(normalExpr, hasNormalLink);

	std::string source = R"(#include "common.hlsli"
#include "shading_flags.hlsli"

struct PS_INPUT
{
float4 Position : SV_POSITION;
float3 WorldPos : WORLD_POSITION;
float3 Normal : NORMAL;
float2 TexCoord : TEXCOORD0;
float2 TexCoord1 : TEXCOORD1;
float3 Tangent : TANGENT;
float4 Color : COLOR0;
nointerpolation float PickingId : TEXCOORD5;
nointerpolation float NoDecals : TEXCOORD6;
};

struct PS_OUTPUT
{
float4 Diffuse : SV_TARGET0;
float4 Normal : SV_TARGET1;
float4 PBR : SV_TARGET2;
float4 Emission : SV_TARGET3;
float4 Position : SV_TARGET4;
};

)" + materialConstantsBlock + BuildGraphTextureHlsl(graph, useMaterialTexture) + R"(
PS_OUTPUT main(PS_INPUT input)
{
PS_OUTPUT output;
float4 diffuse = ()" + baseColor + R"() * input.Color;
)" + materialDiffuseSample + opacityMaskClip + R"(
if (diffuse.a < 0.01f) discard;

)" + normalCalcBlock + R"(
float roughness = clamp()" + roughness + R"(, 0.04f, 1.0f);
float metallic = saturate()" + metallic + R"();
float cavity = clamp()" + cavity + R"(, 0.0f, 1.0f);
float ao = clamp()" + ao + R"(, 0.0f, 1.0f);
float3 emission = )" + emissive + R"(;

uint defaultMeshFlags = FLAG_RECEIVE_SHADOWS | FLAG_APPLY_SSAO | (input.NoDecals > 0.5f ? 0u : FLAG_RECEIVE_DECALS);

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

GraphDiagnostic ShaderGraphCompiler::GenerateForwardTransparentMaterialHlsl(
	const Graph& graph, const GraphRegistry& registry, std::string& outSource)
{
	const ForwardTransparentScope transparentScope;
	const GraphNode* outputNode = nullptr;
	std::string baseColor;
	std::vector<GraphPinReference> callStack;

	const auto diagnostic = ValidateShaderGraph(graph, registry, outputNode);
	if (!diagnostic.Succeeded()) return diagnostic;

	const auto links = graph.GetLinks();
	const auto hasInputLink = [&](const std::string& pinName)
	{
		return std::any_of(links.begin(), links.end(), [&](const GraphLink& link)
		{
			return link.To.Node == outputNode->Id && link.To.Pin == pinName;
		});
	};

	const char* colorPin = outputNode->Type == "Shader.MaterialOutput" ? "baseColor" : "color";
	if (hasInputLink(colorPin))
	{
		auto result = ResolveInputExpression(
			graph, registry, { outputNode->Id, colorPin }, EGraphPinType::Float4, EShaderStage::Pixel, callStack, baseColor);
		if (!result.Succeeded()) return result;
	}
	else
	{
		baseColor = "gMaterial.baseColor";
	}

	std::string emissive = "gMaterial.emissiveColor";
	std::string ao = "gMaterial.ao";
	std::string normalExpr;
	std::string opacityMaskExpr;
	bool hasNormalLink = false;
	bool hasOpacityMaskLink = false;
	if (outputNode->Type == "Shader.MaterialOutput")
	{
		if (hasInputLink("emissive"))
		{
			auto result = ResolveInputExpression(
				graph, registry, { outputNode->Id, "emissive" }, EGraphPinType::Float3, EShaderStage::Pixel, callStack, emissive);
			if (!result.Succeeded()) return result;
		}
		if (hasInputLink("ao"))
		{
			auto result = ResolveInputExpression(
				graph, registry, { outputNode->Id, "ao" }, EGraphPinType::Float, EShaderStage::Pixel, callStack, ao);
			if (!result.Succeeded()) return result;
		}
		if (hasInputLink("normal"))
		{
			auto result = ResolveInputExpression(
				graph, registry, { outputNode->Id, "normal" }, EGraphPinType::Float3, EShaderStage::Pixel, callStack, normalExpr);
			if (!result.Succeeded()) return result;
			hasNormalLink = true;
		}
		if (hasInputLink("opacityMask"))
		{
			auto result = ResolveInputExpression(
				graph, registry, { outputNode->Id, "opacityMask" }, EGraphPinType::Float, EShaderStage::Pixel, callStack, opacityMaskExpr);
			if (!result.Succeeded()) return result;
			hasOpacityMaskLink = true;
		}
	}

	const MaterialBufferLayout layout = BuildMaterialBufferLayout(graph);
	const std::string materialConstantsBlock = BuildMaterialConstantsHlsl(layout);
	const bool useMaterialTexture = outputNode->Type == "Shader.MaterialOutput" &&
		baseColor.find("SampleGraphTexture0(") == std::string::npos;
	const char* materialDiffuseSample = useMaterialTexture
		? "if ((gMaterial.materialFlags & FLAG_HAS_DIFFUSE_MAP) != 0u)\n{\n\tdiffuse *= diffuseMap.Sample(materialSampler, input.TexCoord);\n}\n"
		: "";
	const std::string opacityMaskClip = hasOpacityMaskLink
		? "float opacityMask = " + opacityMaskExpr + ";\nclip(opacityMask - 0.5f);\n"
		: "";
	const std::string normalCalcBlock = BuildNormalCalculationHlsl(normalExpr, hasNormalLink);

	std::string source = R"(#include "common.hlsli"
#include "shading_flags.hlsli"

struct PS_INPUT
{
float4 Position : SV_POSITION;
float3 WorldPos : WORLD_POSITION;
float3 Normal : NORMAL;
float2 TexCoord : TEXCOORD0;
float2 TexCoord1 : TEXCOORD1;
float3 Tangent : TANGENT;
float4 Color : COLOR0;
nointerpolation float PickingId : TEXCOORD5;
};

// Pass Texture: SceneColorCopy (Space 3, t0 / RHI Slot 16)
Texture2D g_SceneColor : register(t0, space3);
SamplerState g_SceneColorSampler : register(s0, space3);

float4 SampleSceneColor(float2 uv, float2 baseScreenUV)
{
	bool isOffScreen = any(uv < 0.0f) || any(uv > 1.0f);
	float2 finalUV = isOffScreen ? baseScreenUV : uv;
	finalUV = clamp(finalUV, float2(0.0001f, 0.0001f), float2(0.9999f, 0.9999f));
	return g_SceneColor.Sample(g_SceneColorSampler, finalUV);
}

)" + materialConstantsBlock + BuildGraphTextureHlsl(graph, useMaterialTexture) + R"(
float4 main(PS_INPUT input) : SV_TARGET0
{
float _sceneWidth, _sceneHeight;
g_SceneColor.GetDimensions(_sceneWidth, _sceneHeight);
float2 screenUV = input.Position.xy / float2(max(_sceneWidth, 1.0f), max(_sceneHeight, 1.0f));

float4 diffuse = ()" + baseColor + R"() * input.Color;
)" + materialDiffuseSample + opacityMaskClip + R"(
float alpha = saturate(diffuse.a);
if (alpha < 0.001f) discard;

)" + normalCalcBlock + R"(
float3 L = normalize(float3(0.45f, 0.80f, 0.38f));
float ndl = saturate(dot(N, L));
float hemi = 0.35f + 0.65f * (N.y * 0.5f + 0.5f);
float aoVal = clamp()" + ao + R"(, 0.0f, 1.0f);
float3 litColor = diffuse.rgb * ((0.35f * hemi + 0.75f * ndl) * aoVal) + ()" + emissive + R"();
return float4(litColor, alpha);
}
)";

	outSource.swap(source);
	return {};
}
GraphDiagnostic ShaderGraphCompiler::GenerateTerrainMaterialHlsl(
	const Graph& graph, const GraphRegistry& registry, std::string& outSource)
{
	const TerrainDomainScope terrainDomain;
	const GraphNode* outputNode = nullptr;
	std::vector<GraphPinReference> callStack;

	const auto diagnostic = ValidateShaderGraph(graph, registry, outputNode);
	if (!diagnostic.Succeeded()) return diagnostic;

	// 연결된 출력 핀만 그래프 식으로 바꾸고, 나머지는 기본값을 쓴다. 정점 입력 핀은 지형 정점 셰이더가 고정이라 읽지 않는다.
	const bool isMaterialOutput = outputNode->Type == "Shader.MaterialOutput";
	const auto resolveIfLinked = [&](const char* pin, EGraphPinType type, std::string& expression) -> GraphDiagnostic
		{
			if (!HasInputLink(graph, outputNode->Id, pin)) return {};
			return ResolveInputExpression(graph, registry, { outputNode->Id, pin }, type, EShaderStage::Pixel, callStack, expression);
		};

	// baseColor가 없으면 칠한 레이어 합성(지형의 기본 표면)
	std::string baseColor = "TerrainBlendLayers(terrainPixel)";
	if (auto result = resolveIfLinked(isMaterialOutput ? "baseColor" : "color", EGraphPinType::Float4, baseColor); !result.Succeeded())
		return result;

	std::string roughness = "gMaterial.roughness";
	std::string metallic = "gMaterial.metallic";
	std::string cavity = "gMaterial.cavity";
	std::string ao = "gMaterial.ao";
	std::string emissive = "gMaterial.emissiveColor";
	std::string normalExpr;
	bool hasNormalLink = false;
	if (isMaterialOutput)
	{
		const struct { const char* Pin; EGraphPinType Type; std::string* Expression; } pins[] = {
			{ "roughness", EGraphPinType::Float, &roughness }, { "metallic", EGraphPinType::Float, &metallic },
			{ "cavity", EGraphPinType::Float, &cavity }, { "ao", EGraphPinType::Float, &ao },
			{ "emissive", EGraphPinType::Float3, &emissive }, { "normal", EGraphPinType::Float3, &normalExpr } };
		for (const auto& pin : pins)
			if (auto result = resolveIfLinked(pin.Pin, pin.Type, *pin.Expression); !result.Succeeded()) return result;
		hasNormalLink = HasInputLink(graph, outputNode->Id, "normal");
	}

	const MaterialBufferLayout layout = BuildMaterialBufferLayout(graph);
	const std::string textureBlock = BuildGraphTextureHlsl(graph, false);
	// 그래프가 슬롯 0(materialSampler, s0 space1)을 쓰면 지형 공용 헤더가 같은 샘플러를 쓰게 한다(같은 레지스터 중복 선언 방지).
	const std::string samplerAlias = textureBlock.find("materialSampler") != std::string::npos
		? "#define TERRAIN_SAMPLER materialSampler\n" : "";

	std::string source = R"(#include "common.hlsli"
#include "shading_flags.hlsli"

// 지형 정점 셰이더(terrainVS)의 출력과 같아야 한다.
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

)" + BuildMaterialConstantsHlsl(layout) + textureBlock + samplerAlias + R"(#include "terrainLayers.hlsli"

PS_OUTPUT main(PS_INPUT input)
{
PS_OUTPUT output;
const TerrainPixel terrainPixel = TerrainBeginPixel(input.WorldPos, normalize(input.Normal), input.TexCoord);
float4 diffuse = ()" + baseColor + R"() * input.Color;

)" + BuildNormalCalculationHlsl(normalExpr, hasNormalLink) + R"(
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

GraphDiagnostic ShaderGraphCompiler::BuildDefaultTerrainGraph(const GraphRegistry& registry, Graph& outGraph)
{
	Graph graph(EGraphDomain::Shader);
	GraphDiagnostic failure;
	const auto add = [&](const char* type, float x, float y) -> GraphNodeId
		{
			if (!failure.Succeeded()) return 0;
			const auto result = graph.AddNode(registry, type);
			if (!result.Succeeded()) { failure = result; return 0; }
			(void)graph.SetNodePosition(result.Node, x, y);
			return result.Node;
		};
	const auto connect = [&](GraphNodeId from, const char* fromPin, GraphNodeId to, const char* toPin)
		{
			if (!failure.Succeeded()) return;
			if (auto result = graph.Connect(registry, { from, fromPin }, { to, toPin }); !result.Succeeded()) failure = result;
		};
	const auto set = [&](GraphNodeId node, const char* pin, GraphValue value)
		{
			if (!failure.Succeeded()) return;
			if (auto result = graph.SetInputValue(registry, { node, pin }, std::move(value)); !result.Succeeded()) failure = result;
		};
	const auto constant = [&](float value, float x, float y)
		{
			const auto node = add("Core.Float", x, y);
			set(node, "value", value);
			return node;
		};

	const auto output = add("Shader.MaterialOutput", 900.0f, 250.0f);

	// 절벽 가중치 = 1 - smoothstep(0.55, 0.78, 경사)
	const auto slope = add("Terrain.Slope", 0.0f, 420.0f);
	const auto smooth = add("Math.SmoothStepFloat", 200.0f, 420.0f);
	set(smooth, "edge0", 0.55f);
	set(smooth, "edge1", 0.78f);
	connect(slope, "slope", smooth, "value");
	const auto cliff = add("Math.SubtractFloat", 420.0f, 420.0f);
	set(cliff, "a", 1.0f);
	connect(smooth, "result", cliff, "b");

	// 색 = lerp(칠한 레이어 합성, 레이어 1, 절벽 가중치)
	const auto blend = add("Terrain.LayerBlend", 200.0f, 100.0f);
	const auto rock = add("Terrain.LayerSample", 200.0f, 230.0f);
	set(rock, "layer", std::int64_t{ 1 });
	const auto color = add("Math.LerpFloat4", 620.0f, 150.0f);
	connect(blend, "rgba", color, "a");
	connect(rock, "rgba", color, "b");
	connect(cliff, "result", color, "t");
	connect(color, "result", output, "baseColor");

	// 거칠기 = lerp(0.85, 0.65, 절벽 가중치)
	const auto roughness = add("Math.LerpFloat", 620.0f, 420.0f);
	set(roughness, "a", 0.85f);
	set(roughness, "b", 0.65f);
	connect(cliff, "result", roughness, "t");
	connect(roughness, "result", output, "roughness");

	// 연결하지 않은 핀은 머티리얼 상수를 읽으므로 terrainPS와 같은 값을 상수로 넣는다.
	connect(constant(0.0f, 620.0f, 560.0f), "result", output, "metallic");
	connect(constant(0.5f, 620.0f, 620.0f), "result", output, "cavity");
	connect(constant(1.0f, 620.0f, 680.0f), "result", output, "ao");

	if (!failure.Succeeded()) return failure;
	outGraph = std::move(graph);
	return {};
}
