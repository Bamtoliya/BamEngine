#include "ShaderGraphCompiler.h"
#include "MaterialInterface.h"
#include <cstring>
#include <iostream>
#include <locale>
#include <stdexcept>
#include <initializer_list>

namespace
{
	using namespace Engine;

	void Expect(bool condition, const char* message)
	{
		if (!condition) throw std::runtime_error(message);
	}

	void Accept(const GraphDiagnostic& result)
	{
		if (!result.Succeeded()) throw std::runtime_error(result.Message);
	}

	GraphNodeId Add(Graph& graph, const GraphRegistry& registry, const char* type)
	{
		const auto result = graph.AddNode(registry, type);
		Accept(result);
		return result.Node;
	}

	void Reject(const Graph& graph, const GraphRegistry& registry, std::string& source, EGraphError code)
	{
		const auto previous = source;
		const auto generators = {
			&ShaderGraphCompiler::GenerateColorHlsl,
			&ShaderGraphCompiler::GenerateUnlitMaterialHlsl,
			&ShaderGraphCompiler::GenerateGBufferMaterialHlsl
		};

		for (const auto generate : generators)
		{
			const auto result = generate(graph, registry, source);
			Expect(!result.Succeeded() && result.Code == code, "Unexpected shader generation result.");
			Expect(source == previous, "Failed generation replaced the previous source.");
		}
	}

	struct CommaPunctuation final : std::numpunct<char>
	{
		char do_decimal_point() const override { return ','; }
	};

	struct LocaleScope
	{
		std::locale Previous;

#pragma push_macro("new")
#undef new
		LocaleScope()
			: Previous(std::locale::global(std::locale(std::locale::classic(), new CommaPunctuation))) {
		}
#pragma pop_macro("new")

		~LocaleScope() { std::locale::global(Previous); }
	};
}

int main()
{
	try
	{
		using namespace Engine;

		GraphRegistry registry;
		Accept(registry.RegisterBuiltins());

		// ── 1. ColorOutput 및 기존 Float4 연동 검증 ──
		Graph graph{ EGraphDomain::Shader };
		const auto output = Add(graph, registry, "Shader.ColorOutput");
		std::string source;

		Accept(ShaderGraphCompiler::GenerateColorHlsl(graph, registry, source));
		Expect(source.find("float4 main() : SV_Target0") != std::string::npos, "Missing pixel shader entry.");
		Expect(source.find(
			"return float4(1.00000000e+00f, 1.00000000e+00f, "
			"1.00000000e+00f, 1.00000000e+00f);") != std::string::npos, "Default color was incorrect.");

		const auto first = Add(graph, registry, "Core.Float4");
		const auto second = Add(graph, registry, "Core.Float4");

		Accept(graph.SetInputValue(registry, { first, "value" }, std::array<float, 4>{ 0.25f, -0.5f, 2.0f, 1.0f }));
		Accept(graph.SetInputValue(registry, { second, "value" }, std::array<float, 4>{ 7, 7, 7, 7 }));
		Accept(graph.SetInputValue(registry, { output, "color" }, std::array<float, 4>{ 9, 9, 9, 9 }));
		Accept(graph.Connect(registry, { first, "result" }, { second, "value" }));
		Accept(graph.Connect(registry, { second, "result" }, { output, "color" }));

		{
			LocaleScope locale;
			Accept(ShaderGraphCompiler::GenerateColorHlsl(graph, registry, source));
		}

		Expect(source.find(
			"return float4(2.50000000e-01f, -5.00000000e-01f, "
			"2.00000000e+00f, 1.00000000e+00f);") != std::string::npos,
			"Connection priority, color channels or decimal formatting was incorrect.");

		std::string materialSource;
		{
			LocaleScope locale;
			Accept(ShaderGraphCompiler::GenerateUnlitMaterialHlsl(graph, registry, materialSource));
		}

		Expect(materialSource.find(
			"float4(2.50000000e-01f, -5.00000000e-01f, 2.00000000e+00f, 1.00000000e+00f)") != std::string::npos,
			"Material generation changed the resolved color or decimal formatting.");

		// ── 2. 미지원 노드 및 다중 출력 노드 거부 검증 ──
		const auto unsupported = Add(graph, registry, "Core.Int");
		Reject(graph, registry, source, EGraphError::UnsupportedCompilation);
		Accept(graph.RemoveNode(unsupported));

		const auto extraOutput = Add(graph, registry, "Shader.ColorOutput");
		Reject(graph, registry, source, EGraphError::InvalidDocument);
		Accept(graph.RemoveNode(extraOutput));

		const auto extraMatOutput = Add(graph, registry, "Shader.MaterialOutput");
		Reject(graph, registry, source, EGraphError::InvalidDocument);
		Accept(graph.RemoveNode(extraMatOutput));

		Graph empty{ EGraphDomain::Shader };
		Reject(empty, registry, source, EGraphError::InvalidDocument);

		Graph logic{ EGraphDomain::Logic };
		Reject(logic, registry, source, EGraphError::UnsupportedDomain);

		// ── 3. MaterialOutput 및 채널 분리/결합 PBR G-Buffer 검증 ──
		Graph matGraph{ EGraphDomain::Shader };
		const auto matOutput = Add(matGraph, registry, "Shader.MaterialOutput");
		std::string gbufferSource;
		Accept(ShaderGraphCompiler::GenerateGBufferMaterialHlsl(matGraph, registry, gbufferSource));
		Expect(gbufferSource.find("SHADING_MODEL_DEFAULT_LIT") != std::string::npos, "Missing DEFAULT_LIT shading model.");
		Expect(gbufferSource.find("ConstantBuffer<MaterialConstants> gMaterial : register(b0, space3);") != std::string::npos,
			"Missing gMaterial ConstantBuffer declaration.");
		Expect(gbufferSource.find("gMaterial.baseColor") != std::string::npos, "Default baseColor Fast-Path was incorrect.");
		Expect(gbufferSource.find("gMaterial.roughness") != std::string::npos, "Default roughness Fast-Path was incorrect.");
		Expect(gbufferSource.find("gMaterial.metallic") != std::string::npos, "Default metallic Fast-Path was incorrect.");
		Expect(gbufferSource.find("gMaterial.cavity") != std::string::npos, "Default cavity Fast-Path was incorrect.");
		Expect(gbufferSource.find("gMaterial.ao") != std::string::npos, "Default ao Fast-Path was incorrect.");
		const auto makeEmissive = Add(matGraph, registry, "Math.MakeFloat3");
		const auto floatX = Add(matGraph, registry, "Core.Float");
		const auto floatY = Add(matGraph, registry, "Core.Float");
		const auto floatZ = Add(matGraph, registry, "Core.Float");
		Accept(matGraph.SetInputValue(registry, { floatX, "value" }, 0.125f));
		Accept(matGraph.SetInputValue(registry, { floatY, "value" }, 0.25f));
		Accept(matGraph.SetInputValue(registry, { floatZ, "value" }, 0.5f));
		Accept(matGraph.Connect(registry, { floatX, "result" }, { makeEmissive, "x" }));
		Accept(matGraph.Connect(registry, { floatY, "result" }, { makeEmissive, "y" }));
		Accept(matGraph.Connect(registry, { floatZ, "result" }, { makeEmissive, "z" }));
		Accept(matGraph.Connect(registry, { makeEmissive, "result" }, { matOutput, "emissive" }));
		const auto pbrConst = Add(matGraph, registry, "Core.Float4");
		const auto breakPbr = Add(matGraph, registry, "Math.BreakFloat4");
		Accept(matGraph.SetInputValue(registry, { pbrConst, "value" }, std::array<float, 4>{ 0.25f, 0.75f, 0.5f, 1.0f }));
		Accept(matGraph.Connect(registry, { pbrConst, "result" }, { breakPbr, "value" }));
		Accept(matGraph.Connect(registry, { breakPbr, "x" }, { matOutput, "roughness" }));
		Accept(matGraph.Connect(registry, { breakPbr, "y" }, { matOutput, "metallic" }));
		{
			LocaleScope locale;
			Accept(ShaderGraphCompiler::GenerateGBufferMaterialHlsl(matGraph, registry, gbufferSource));
		}
		Expect(gbufferSource.find("float3(1.25000000e-01f, 2.50000000e-01f, 5.00000000e-01f)") != std::string::npos,
			"MakeFloat3 emissive connection failed.");
		Expect(gbufferSource.find("2.50000000e-01f") != std::string::npos, "BreakFloat4 x roughness failed.");
		Expect(gbufferSource.find("7.50000000e-01f") != std::string::npos, "BreakFloat4 y metallic failed.");

		// ── 4. MaterialOutput VS (WorldPositionOffset) 검증 ──
		std::string vsSource;
		Accept(ShaderGraphCompiler::GenerateGBufferMaterialVS(matGraph, registry, vsSource));
		Expect(vsSource.find("float3 worldPosOffset = float3(0.0f, 0.0f, 0.0f);") != std::string::npos &&
			vsSource.find("worldPosition.xyz += worldPosOffset;") != std::string::npos,
			"Default worldPositionOffset expression was incorrect.");

		const auto wpoConst = Add(matGraph, registry, "Core.Float3");
		Accept(matGraph.SetInputValue(registry, { wpoConst, "value" }, std::array<float, 3>{ 0.0f, 1.0f, 0.0f }));
		Accept(matGraph.Connect(registry, { wpoConst, "result" }, { matOutput, "worldPositionOffset" }));
		Accept(ShaderGraphCompiler::GenerateGBufferMaterialVS(matGraph, registry, vsSource));
		Expect(vsSource.find("float3(0.00000000e+00f, 1.00000000e+00f, 0.00000000e+00f)") != std::string::npos,
			"Connected worldPositionOffset expression was incorrect.");

		// ── 5. 동적 MaterialBufferLayout (16바이트 패킹) 및 커스텀 파라미터(Variable.Get.*) 검증 ──
		Graph dynGraph{ EGraphDomain::Shader };
		const auto dynOutput = Add(dynGraph, registry, "Shader.MaterialOutput");
		Accept(dynGraph.AddVariable(registry, { "DissolveProgress", EGraphPinType::Float, 0.35f }));
		Accept(dynGraph.AddVariable(registry, { "UVTiling", EGraphPinType::Float2, std::array<float, 2>{ 2.0f, 3.0f } }));
		Accept(dynGraph.AddVariable(registry, { "TintRgb", EGraphPinType::Float3, std::array<float, 3>{ 0.2f, 0.4f, 0.8f } }));

		const MaterialBufferLayout dynLayout = ShaderGraphCompiler::BuildMaterialBufferLayout(dynGraph);
		Expect(dynLayout.Entries.size() == 3, "Dynamic layout entry count mismatch.");
		Expect(dynLayout.TotalBufferSize == 80, "Dynamic layout total size must be 80 bytes (16-byte aligned).");
		const auto* eDissolve = dynLayout.Find("dissolveprogress");
		const auto* eTiling = dynLayout.Find("UVTiling");
		const auto* eTint = dynLayout.Find("TintRgb");
		Expect(eDissolve && eDissolve->Offset == 48 && eDissolve->Size == 4, "DissolveProgress offset/size mismatch.");
		Expect(eTiling && eTiling->Offset == 52 && eTiling->Size == 8, "UVTiling offset/size mismatch.");
		Expect(eTint && eTint->Offset == 64 && eTint->Size == 12, "TintRgb 16-byte boundary jump offset mismatch.");

		const auto getDissolve = Add(dynGraph, registry, "Variable.Get.DissolveProgress");
		const auto getTint = Add(dynGraph, registry, "Variable.Get.TintRgb");
		Accept(dynGraph.Connect(registry, { getDissolve, "value" }, { dynOutput, "roughness" }));
		Accept(dynGraph.Connect(registry, { getTint, "value" }, { dynOutput, "worldPositionOffset" }));

		std::string dynPsSource;
		std::string dynVsSource;
		Accept(ShaderGraphCompiler::GenerateGBufferMaterialHlsl(dynGraph, registry, dynPsSource));
		Accept(ShaderGraphCompiler::GenerateGBufferMaterialVS(dynGraph, registry, dynVsSource));
		Expect(dynPsSource.find("float custom_DissolveProgress;") != std::string::npos, "Missing custom_DissolveProgress in PS cbuffer.");
		Expect(dynPsSource.find("float2 custom_UVTiling;") != std::string::npos, "Missing custom_UVTiling in PS cbuffer.");
		Expect(dynPsSource.find("float3 custom_TintRgb;") != std::string::npos, "Missing custom_TintRgb in PS cbuffer.");
		Expect(dynPsSource.find("float  _pad0;") != std::string::npos, "Missing explicit 16-byte alignment padding in PS cbuffer.");
		Expect(dynPsSource.find("gMaterial.custom_DissolveProgress") != std::string::npos, "Variable.Get.DissolveProgress PS reference missing.");
		Expect(dynVsSource.find("gMaterial.custom_TintRgb") != std::string::npos, "Variable.Get.TintRgb VS reference missing.");

		// ── 6. ForwardTransparent 단일 렌더 타깃 PS 생성 검증 ──
		std::string transparentPsSource;
		Accept(ShaderGraphCompiler::GenerateForwardTransparentMaterialHlsl(dynGraph, registry, transparentPsSource));
		Expect(transparentPsSource.find("float4 main(PS_INPUT input) : SV_TARGET0") != std::string::npos,
			"ForwardTransparent PS must output single SV_TARGET0.");
		Expect(transparentPsSource.find("SHADING_MODEL_DEFAULT_LIT") == std::string::npos,
			"ForwardTransparent PS must not emit GBuffer shading model.");
		Expect(transparentPsSource.find("ConstantBuffer<MaterialConstants> gMaterial : register(b0, space3);") != std::string::npos,
			"ForwardTransparent PS missing dynamic gMaterial cbuffer.");

		// ── 7. MaterialInterface PrimaryPassType Fallback 정합성 및 동적 바이트 블롭 검증 ──
		class TestMaterial final : public MaterialInterface
		{
		public:
			TestMaterial() : MaterialInterface() {}
		};

		TestMaterial testMat;
		Expect(testMat.GetPrimaryPassType() == ERenderPassType::Geometry, "Default PrimaryPassType must be Geometry.");
		testMat.SetPrimaryPassType(ERenderPassType::ForwardTransparent);
		Expect(testMat.GetPrimaryPassType() == ERenderPassType::ForwardTransparent, "SetPrimaryPassType failed.");
		Expect(testMat.GetPixelShader(ERenderPassType::Geometry) == nullptr, "Non-primary pass without override must return nullptr.");

		testMat.SetBufferLayout(dynLayout);
		testMat.SetFloat("DissolveProgress", 0.75f);
		testMat.SetFloat2("UVTiling", vec2(4.0f, 5.0f));
		testMat.SetFloat3("TintRgb", vec3(0.1f, 0.2f, 0.3f));

		vector<uint8> blob;
		testMat.BuildConstantBufferBlob(blob);
		Expect(blob.size() == 80, "Constant buffer blob size must match dynamic layout (80 bytes).");

		float readDissolve = 0.0f;
		vec2 readTiling(0.0f);
		vec3 readTint(0.0f);
		std::memcpy(&readDissolve, blob.data() + 48, sizeof(float));
		std::memcpy(&readTiling, blob.data() + 52, sizeof(vec2));
		std::memcpy(&readTint, blob.data() + 64, sizeof(vec3));
		Expect(readDissolve == 0.75f, "Blob DissolveProgress value mismatch.");
		Expect(readTiling.x == 4.0f && readTiling.y == 5.0f, "Blob UVTiling value mismatch.");
		Expect(readTint.x == 0.1f && readTint.y == 0.2f && readTint.z == 0.3f, "Blob TintRgb value mismatch.");

		std::cout << "Shader graph checks passed.\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}