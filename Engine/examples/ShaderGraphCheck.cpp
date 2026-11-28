#include "ShaderGraphCompiler.h"
#include "MaterialInterface.h"
#include "Vertex.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <locale>
#include <stdexcept>
#include <initializer_list>
#include <source_location>

namespace
{
	using namespace Engine;

	void Expect(bool condition, const char* message,
		const std::source_location& location = std::source_location::current())
	{
		std::cerr << "[EXPECT] Line " << location.line()
			<< ": " << message << std::endl;

		if (!condition)
			throw std::runtime_error(message);
	}

	void Accept(const GraphDiagnostic& result,
		const std::source_location& location = std::source_location::current())
	{
		std::cerr << "[ACCEPT] Line " << location.line() << std::endl;

		if (!result.Succeeded())
			throw std::runtime_error(result.Message);
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
		Graph uvFallbackGraph{ EGraphDomain::Shader };
		Add(uvFallbackGraph, registry, "Shader.MaterialOutput");
		const auto fallbackReconciliation = uvFallbackGraph.Reconcile(registry);
		Expect(fallbackReconciliation.InjectedDefaults > 0, "Graph schema defaults were not reconciled.");
		Accept(ShaderGraphCompiler::GenerateGBufferMaterialVS(uvFallbackGraph, registry, vsSource));
		Expect(vsSource.find("output.TexCoord = input.TexCoord;") != std::string::npos,
			"Unconnected customUV0 must preserve the mesh UV after schema defaults are reconciled.");
		Expect(vsSource.find("output.TexCoord1 = input.TexCoord1;") != std::string::npos,
			"Unconnected customUV1 must preserve mesh UV1 after schema defaults are reconciled.");
		const auto& vertexElements = VertexMaterial::Layout.elements;
		Expect(std::any_of(vertexElements.begin(), vertexElements.end(), [](const auto& element) {
			return element.semanticName == "TEXCOORD" && element.semanticIndex == 1 &&
				element.offset == offsetof(VertexMaterial, texCoord1);
		}), "Material vertex layout must expose a distinct mesh UV1.");

		const auto wpoConst = Add(matGraph, registry, "Core.Float3");
		Accept(matGraph.SetInputValue(registry, { wpoConst, "value" }, std::array<float, 3>{ 0.0f, 1.0f, 0.0f }));
		Accept(matGraph.Connect(registry, { wpoConst, "result" }, { matOutput, "worldPositionOffset" }));
		Accept(ShaderGraphCompiler::GenerateGBufferMaterialVS(matGraph, registry, vsSource));
		Expect(vsSource.find("float3(0.00000000e+00f, 1.00000000e+00f, 0.00000000e+00f)") != std::string::npos,
			"Connected worldPositionOffset expression was incorrect.");

		const auto meshUv = Add(matGraph, registry, "Shader.TexCoord0");
		const auto uvScale = Add(matGraph, registry, "Math.MultiplyFloat2");
		const auto uvOffset = Add(matGraph, registry, "Math.AddFloat2");
		Accept(matGraph.SetInputValue(registry, { uvScale, "b" }, std::array<float, 2>{ 2.0f, 3.0f }));
		Accept(matGraph.SetInputValue(registry, { uvOffset, "b" }, std::array<float, 2>{ 0.25f, 0.5f }));
		Accept(matGraph.Connect(registry, { meshUv, "uv" }, { uvScale, "a" }));
		Accept(matGraph.Connect(registry, { uvScale, "result" }, { uvOffset, "a" }));
		Accept(matGraph.Connect(registry, { uvOffset, "result" }, { matOutput, "customUV0" }));
		Accept(ShaderGraphCompiler::GenerateGBufferMaterialVS(matGraph, registry, vsSource));
		Expect(vsSource.find(
			"output.TexCoord = ((input.TexCoord * float2(2.00000000e+00f, 3.00000000e+00f)) + "
			"float2(2.50000000e-01f, 5.00000000e-01f));") != std::string::npos,
			"Vertex-stage UV scale and offset were not forwarded through TEXCOORD0.");

		std::string matPsSource;
		Accept(ShaderGraphCompiler::GenerateGBufferMaterialHlsl(matGraph, registry, matPsSource));
		Expect(matPsSource.find("Texture2D diffuseMap : register(t0, space1);") != std::string::npos &&
			matPsSource.find("diffuseMap.Sample(materialSampler, input.TexCoord)") != std::string::npos,
			"Generated GBuffer PS must sample the material diffuse texture using interpolated UV0.");
		std::string transparentPsSource;
		Accept(ShaderGraphCompiler::GenerateForwardTransparentMaterialHlsl(matGraph, registry, transparentPsSource));
		Expect(transparentPsSource.find("diffuseMap.Sample(materialSampler, input.TexCoord)") != std::string::npos,
			"Generated ForwardTransparent PS must sample the material diffuse texture using interpolated UV0.");
		const auto meshUv1 = Add(matGraph, registry, "Shader.TexCoord1");
		const auto uv1Scale = Add(matGraph, registry, "Math.MultiplyFloat2");
		Accept(matGraph.SetInputValue(registry, { uv1Scale, "b" }, std::array<float, 2>{ 4.0f, 5.0f }));
		Accept(matGraph.Connect(registry, { meshUv1, "uv" }, { uv1Scale, "a" }));
		Accept(matGraph.Connect(registry, { uv1Scale, "result" }, { matOutput, "customUV1" }));
		Accept(ShaderGraphCompiler::GenerateGBufferMaterialVS(matGraph, registry, vsSource));
		Accept(ShaderGraphCompiler::GenerateGBufferMaterialHlsl(matGraph, registry, matPsSource));
		Accept(ShaderGraphCompiler::GenerateForwardTransparentMaterialHlsl(matGraph, registry, transparentPsSource));
		Expect(vsSource.find("output.TexCoord1 = (input.TexCoord1 * float2(4.00000000e+00f, 5.00000000e+00f));") != std::string::npos,
			"Connected customUV1 must transform mesh UV1.");
		Expect(vsSource.find("output.TexCoord = ((input.TexCoord *") != std::string::npos,
			"Custom UV1 must not replace the custom UV0 expression.");
		for (const auto* shader : { &vsSource, &matPsSource, &transparentPsSource })
			Expect(shader->find("float2 TexCoord1 : TEXCOORD1;") != std::string::npos,
				"Generated material stages must declare the TEXCOORD1 interface.");
		const auto firstUv1 = vsSource.find("float2 TexCoord1 : TEXCOORD1;");
		Expect(vsSource.find("float2 TexCoord1 : TEXCOORD1;", firstUv1 + 1) != std::string::npos,
			"Generated vertex shader must expose UV1 in both input and output.");

		Graph textureGraph{ EGraphDomain::Shader };
		const auto textureOutput = Add(textureGraph, registry, "Shader.MaterialOutput");
		const auto sample = Add(textureGraph, registry, "Shader.TextureSample");
		Accept(textureGraph.Connect(registry, { sample, "rgba" }, { textureOutput, "baseColor" }));
		Accept(ShaderGraphCompiler::GenerateGBufferMaterialHlsl(textureGraph, registry, matPsSource));
		Expect(matPsSource.find("SampleGraphTexture0(input.TexCoord)") != std::string::npos &&
			matPsSource.find("diffuse *= diffuseMap.Sample") == std::string::npos,
			"Explicit diffuse sampling must default to UV0 and must not multiply by slot 0 twice.");

		const auto textureUv1 = Add(textureGraph, registry, "Shader.TexCoord1");
		Accept(textureGraph.Connect(registry, { textureUv1, "uv" }, { sample, "uv" }));
		Accept(textureGraph.SetInputValue(registry, { sample, "slot" }, std::int64_t{ 2 }));
		Accept(ShaderGraphCompiler::GenerateGBufferMaterialHlsl(textureGraph, registry, matPsSource));
		Accept(ShaderGraphCompiler::GenerateForwardTransparentMaterialHlsl(textureGraph, registry, transparentPsSource));
		for (const auto* shader : { &matPsSource, &transparentPsSource })
			Expect(shader->find("register(t2, space1)") != std::string::npos &&
				shader->find("register(s2, space1)") != std::string::npos &&
				shader->find("SampleGraphTexture2(input.TexCoord1)") != std::string::npos,
				"Texture sample must use the selected material slot and connected UV in both pixel shaders.");
		Accept(textureGraph.SetInputValue(registry, { sample, "slot" }, std::int64_t{ 4 }));
		Expect(!ShaderGraphCompiler::GenerateGBufferMaterialHlsl(textureGraph, registry, matPsSource).Succeeded(),
			"Texture sample accepted a slot outside the supported material flags.");
		Accept(textureGraph.SetInputValue(registry, { sample, "slot" }, std::int64_t{ 0 }));
		const auto sampleChannels = Add(textureGraph, registry, "Math.BreakFloat4");
		const auto position = Add(textureGraph, registry, "Math.MakeFloat3");
		Accept(textureGraph.Connect(registry, { sample, "rgba" }, { sampleChannels, "value" }));
		Accept(textureGraph.Connect(registry, { sampleChannels, "x" }, { position, "x" }));
		Accept(textureGraph.Connect(registry, { position, "result" }, { textureOutput, "worldPositionOffset" }));
		Expect(!ShaderGraphCompiler::GenerateGBufferMaterialVS(textureGraph, registry, vsSource).Succeeded(),
			"Pixel-only texture sampling must be rejected through vertex-stage intermediate nodes.");

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
		const auto getTiling = Add(dynGraph, registry, "Variable.Get.UVTiling");
		const auto getTint = Add(dynGraph, registry, "Variable.Get.TintRgb");
		const auto dynMeshUv = Add(dynGraph, registry, "Shader.TexCoord0");
		const auto dynUvScale = Add(dynGraph, registry, "Math.MultiplyFloat2");
		Accept(dynGraph.Connect(registry, { getDissolve, "value" }, { dynOutput, "roughness" }));
		Accept(dynGraph.Connect(registry, { dynMeshUv, "uv" }, { dynUvScale, "a" }));
		Accept(dynGraph.Connect(registry, { getTiling, "value" }, { dynUvScale, "b" }));
		Accept(dynGraph.Connect(registry, { dynUvScale, "result" }, { dynOutput, "customUV0" }));
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
		Expect(dynVsSource.find("output.TexCoord = (input.TexCoord * gMaterial.custom_UVTiling);") != std::string::npos,
			"Variable.Get.UVTiling must be available to the custom UV vertex stage.");

		// ── 6. ForwardTransparent 단일 렌더 타깃 PS 생성 검증 ──
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

		std::cerr << "[RUN] TestMaterial construction" << std::endl;

		TestMaterial testMat;

		std::cerr << "[PASS] TestMaterial construction" << std::endl;
		Expect(testMat.GetPrimaryPassType() == ERenderPassType::Geometry, "Default PrimaryPassType must be Geometry.");
		testMat.SetPrimaryPassType(ERenderPassType::ForwardTransparent);
		Expect(testMat.GetPrimaryPassType() == ERenderPassType::ForwardTransparent, "SetPrimaryPassType failed.");
		Expect(testMat.GetPixelShader(ERenderPassType::Geometry) == nullptr, "Non-primary pass without override must return nullptr.");

		testMat.SetBufferLayout(dynLayout);
		testMat.SetFloat("DissolveProgress", 0.75f);
		testMat.SetFloat2("UVTiling", vec2(4.0f, 5.0f));
		testMat.SetFloat3("TintRgb", vec3(0.1f, 0.2f, 0.3f));

		vector<uint8> blob;

		std::cerr << "[RUN] BuildConstantBufferBlob" << std::endl;

		testMat.BuildConstantBufferBlob(blob);

		std::cerr << "[PASS] BuildConstantBufferBlob" << std::endl;
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

		// ── 8. 확장 Math / Shader 입력 노드(Time, WorldPosition, WorldNormal, ViewDirection, Fresnel) 검증 ──
		{
			Graph mathGraph{ EGraphDomain::Shader };
			const auto out = Add(mathGraph, registry, "Shader.MaterialOutput");
			const auto timeNode = Add(mathGraph, registry, "Shader.Time");
			const auto sinNode = Add(mathGraph, registry, "Math.SinFloat");
			const auto cosNode = Add(mathGraph, registry, "Math.CosFloat");
			const auto absNode = Add(mathGraph, registry, "Math.AbsFloat");
			const auto subNode = Add(mathGraph, registry, "Math.SubtractFloat");
			const auto divNode = Add(mathGraph, registry, "Math.DivideFloat");
			const auto powNode = Add(mathGraph, registry, "Math.PowFloat");
			const auto minNode = Add(mathGraph, registry, "Math.MinFloat");
			const auto maxNode = Add(mathGraph, registry, "Math.MaxFloat");
			const auto clampNode = Add(mathGraph, registry, "Math.ClampFloat");
			const auto satNode = Add(mathGraph, registry, "Math.SaturateFloat");
			const auto lerpNode = Add(mathGraph, registry, "Math.LerpFloat");
			const auto stepNode = Add(mathGraph, registry, "Math.StepFloat");
			const auto smoothNode = Add(mathGraph, registry, "Math.SmoothStepFloat");

			Accept(mathGraph.Connect(registry, { timeNode, "time" }, { sinNode, "value" }));
			Accept(mathGraph.Connect(registry, { timeNode, "time" }, { cosNode, "value" }));
			Accept(mathGraph.Connect(registry, { sinNode, "result" }, { absNode, "value" }));
			Accept(mathGraph.Connect(registry, { absNode, "result" }, { subNode, "a" }));
			Accept(mathGraph.Connect(registry, { cosNode, "result" }, { subNode, "b" }));
			Accept(mathGraph.Connect(registry, { subNode, "result" }, { divNode, "a" }));
			Accept(mathGraph.SetInputValue(registry, { divNode, "b" }, 2.0f));
			Accept(mathGraph.Connect(registry, { divNode, "result" }, { powNode, "base" }));
			Accept(mathGraph.SetInputValue(registry, { powNode, "exponent" }, 3.0f));
			Accept(mathGraph.Connect(registry, { powNode, "result" }, { minNode, "a" }));
			Accept(mathGraph.SetInputValue(registry, { minNode, "b" }, 0.9f));
			Accept(mathGraph.Connect(registry, { minNode, "result" }, { maxNode, "a" }));
			Accept(mathGraph.SetInputValue(registry, { maxNode, "b" }, 0.1f));
			Accept(mathGraph.Connect(registry, { maxNode, "result" }, { clampNode, "value" }));
			Accept(mathGraph.Connect(registry, { clampNode, "result" }, { satNode, "value" }));
			Accept(mathGraph.Connect(registry, { satNode, "result" }, { out, "roughness" }));

			Accept(mathGraph.Connect(registry, { timeNode, "time" }, { stepNode, "value" }));
			Accept(mathGraph.SetInputValue(registry, { stepNode, "edge" }, 0.5f));
			Accept(mathGraph.Connect(registry, { stepNode, "result" }, { lerpNode, "a" }));
			Accept(mathGraph.Connect(registry, { timeNode, "time" }, { smoothNode, "value" }));
			Accept(mathGraph.Connect(registry, { smoothNode, "result" }, { lerpNode, "b" }));
			Accept(mathGraph.Connect(registry, { lerpNode, "result" }, { out, "metallic" }));

			const auto worldPos = Add(mathGraph, registry, "Shader.WorldPosition");
			const auto worldNrm = Add(mathGraph, registry, "Shader.WorldNormal");
			const auto viewDir = Add(mathGraph, registry, "Shader.ViewDirection");
			const auto normPos = Add(mathGraph, registry, "Math.NormalizeFloat3");
			const auto dotNode = Add(mathGraph, registry, "Math.DotFloat3");
			const auto fresnelNode = Add(mathGraph, registry, "Shader.Fresnel");
			const auto scaleEmissive = Add(mathGraph, registry, "Math.ScaleFloat3");

			Accept(mathGraph.Connect(registry, { worldPos, "position" }, { normPos, "value" }));
			Accept(mathGraph.Connect(registry, { normPos, "result" }, { dotNode, "a" }));
			Accept(mathGraph.Connect(registry, { worldNrm, "normal" }, { dotNode, "b" }));
			Accept(mathGraph.Connect(registry, { dotNode, "result" }, { out, "ao" }));

			Accept(mathGraph.Connect(registry, { worldNrm, "normal" }, { fresnelNode, "normal" }));
			Accept(mathGraph.Connect(registry, { viewDir, "direction" }, { fresnelNode, "viewDir" }));
			Accept(mathGraph.SetInputValue(registry, { fresnelNode, "power" }, 4.0f));
			Accept(mathGraph.Connect(registry, { worldNrm, "normal" }, { scaleEmissive, "value" }));
			Accept(mathGraph.Connect(registry, { fresnelNode, "result" }, { scaleEmissive, "scale" }));
			Accept(mathGraph.Connect(registry, { scaleEmissive, "result" }, { out, "emissive" }));

			// VS 스테이지에서 WorldNormal / Time / ScaleFloat3 사용 검증
			const auto vsScale = Add(mathGraph, registry, "Math.ScaleFloat3");
			Accept(mathGraph.Connect(registry, { worldNrm, "normal" }, { vsScale, "value" }));
			Accept(mathGraph.Connect(registry, { sinNode, "result" }, { vsScale, "scale" }));
			Accept(mathGraph.Connect(registry, { vsScale, "result" }, { out, "worldPositionOffset" }));

			std::string mathPs;
			std::string mathVs;
			Accept(ShaderGraphCompiler::GenerateGBufferMaterialHlsl(mathGraph, registry, mathPs));
			Accept(ShaderGraphCompiler::GenerateGBufferMaterialVS(mathGraph, registry, mathVs));

			Expect(mathPs.find("#include \"common.hlsli\"") != std::string::npos, "GBuffer PS must include common.hlsli.");
			Expect(mathPs.find("commonData.Time") != std::string::npos, "Shader.Time failed to emit commonData.Time.");
			Expect(mathPs.find("sin(commonData.Time)") != std::string::npos, "Math.SinFloat HLSL mismatch.");
			Expect(mathPs.find("cos(commonData.Time)") != std::string::npos, "Math.CosFloat HLSL mismatch.");
			Expect(mathPs.find("abs(sin(commonData.Time))") != std::string::npos, "Math.AbsFloat HLSL mismatch.");
			Expect(mathPs.find("pow(") != std::string::npos, "Math.PowFloat HLSL mismatch.");
			Expect(mathPs.find("min(") != std::string::npos && mathPs.find("max(") != std::string::npos,
				"Math.MinFloat / MaxFloat HLSL mismatch.");
			Expect(mathPs.find("clamp(") != std::string::npos && mathPs.find("saturate(") != std::string::npos,
				"Math.ClampFloat / SaturateFloat HLSL mismatch.");
			Expect(mathPs.find("step(") != std::string::npos && mathPs.find("smoothstep(") != std::string::npos &&
				mathPs.find("lerp(") != std::string::npos, "Step / SmoothStep / Lerp HLSL mismatch.");
			Expect(mathPs.find("dot(normalize(input.WorldPos), normalize(input.Normal))") != std::string::npos,
				"Dot / Normalize / WorldPosition / WorldNormal HLSL mismatch.");
			Expect(mathPs.find("normalize(commonData.CameraPos - input.WorldPos)") != std::string::npos,
				"Shader.ViewDirection PS HLSL mismatch.");
			Expect(mathPs.find("pow(1.0f - saturate(dot(") != std::string::npos,
				"Shader.Fresnel HLSL mismatch.");
			Expect(mathVs.find("float3 worldNormal = normalize(normalWorld * determinantSign);") != std::string::npos &&
				mathVs.find("(worldNormal * sin(commonData.Time))") != std::string::npos,
				"VS stage WorldNormal + Time WPO HLSL mismatch.");
		}

		// ── 9. MaterialOutput Normal(TBN) 및 OpacityMask(clip) 핀 검증 ──
		{
			Graph masterPinGraph{ EGraphDomain::Shader };
			const auto out = Add(masterPinGraph, registry, "Shader.MaterialOutput");

			// 미연결 시 기본 Fallback 유지 확인
			std::string fallbackGb;
			std::string fallbackFt;
			Accept(ShaderGraphCompiler::GenerateGBufferMaterialHlsl(masterPinGraph, registry, fallbackGb));
			Accept(ShaderGraphCompiler::GenerateForwardTransparentMaterialHlsl(masterPinGraph, registry, fallbackFt));
			for (const auto* shader : { &fallbackGb, &fallbackFt })
			{
				Expect(shader->find("float3 N = normalize(input.Normal);") != std::string::npos,
					"Unconnected normal pin must preserve normalize(input.Normal) fallback.");
				Expect(shader->find("float3x3 TBN") == std::string::npos,
					"Unconnected normal pin must not emit TBN matrix.");
				Expect(shader->find("clip(") == std::string::npos,
					"Unconnected opacityMask pin must not emit clip().");
			}

			// Normal + OpacityMask 연결 시 TBN 변환 및 clip(opacityMask - 0.5f) 생성 확인
			const auto normalConst = Add(masterPinGraph, registry, "Core.Float3");
			const auto maskConst = Add(masterPinGraph, registry, "Core.Float");
			Accept(masterPinGraph.SetInputValue(registry, { normalConst, "value" }, std::array<float, 3>{ 0.0f, 0.25f, 1.0f }));
			Accept(masterPinGraph.SetInputValue(registry, { maskConst, "value" }, 0.75f));
			Accept(masterPinGraph.Connect(registry, { normalConst, "result" }, { out, "normal" }));
			Accept(masterPinGraph.Connect(registry, { maskConst, "result" }, { out, "opacityMask" }));

			std::string connectedGb;
			std::string connectedFt;
			Accept(ShaderGraphCompiler::GenerateGBufferMaterialHlsl(masterPinGraph, registry, connectedGb));
			Accept(ShaderGraphCompiler::GenerateForwardTransparentMaterialHlsl(masterPinGraph, registry, connectedFt));
			for (const auto* shader : { &connectedGb, &connectedFt })
			{
				Expect(shader->find("float3 tangentSpaceNormal = normalize(float3(") != std::string::npos &&
					shader->find("float3x3 TBN = float3x3(T, B, baseN);") != std::string::npos &&
					shader->find("float3 N = normalize(mul(tangentSpaceNormal, TBN));") != std::string::npos,
					"Connected normal pin must emit TBN tangent-space normal perturbation.");
				Expect(shader->find("float opacityMask = 7.50000000e-01f;") != std::string::npos &&
					shader->find("clip(opacityMask - 0.5f);") != std::string::npos,
					"Connected opacityMask pin must emit clip(opacityMask - 0.5f).");
			}
		}

		// ── 10. Shader.TextureSample 다중 노드, 채널 분리(RGBA/RGB/R/G/B/A), 커스텀 UV 및 메타데이터 수집/동기화 검증 ──
		{
			Graph multiTexGraph{ EGraphDomain::Shader };
			const auto out = Add(multiTexGraph, registry, "Shader.MaterialOutput");

			// 1) Albedo (Slot 0, Default UV0, RGBA 출력)
			const auto albedoSample = Add(multiTexGraph, registry, "Shader.TextureSample");
			Accept(multiTexGraph.SetInputValue(registry, { albedoSample, "slot" }, std::int64_t{ 0 }));
			Accept(multiTexGraph.SetInputValue(registry, { albedoSample, "name" }, std::string("BaseAlbedo")));
			Accept(multiTexGraph.Connect(registry, { albedoSample, "rgba" }, { out, "baseColor" }));

			// 2) Normal (Slot 1, Connected Custom Scaled UV, RGB 출력)
			const auto normalSample = Add(multiTexGraph, registry, "Shader.TextureSample");
			Accept(multiTexGraph.SetInputValue(registry, { normalSample, "slot" }, std::int64_t{ 1 }));
			Accept(multiTexGraph.SetInputValue(registry, { normalSample, "name" }, std::string("NormalTexture")));
			const auto uv0Node = Add(multiTexGraph, registry, "Shader.TexCoord0");
			const auto uvScaleNode = Add(multiTexGraph, registry, "Math.MultiplyFloat2");
			Accept(multiTexGraph.SetInputValue(registry, { uvScaleNode, "b" }, std::array<float, 2>{ 2.0f, 2.0f }));
			Accept(multiTexGraph.Connect(registry, { uv0Node, "uv" }, { uvScaleNode, "a" }));
			Accept(multiTexGraph.Connect(registry, { uvScaleNode, "result" }, { normalSample, "uv" }));
			Accept(multiTexGraph.Connect(registry, { normalSample, "rgb" }, { out, "normal" }));

			// 3) PBR (Slot 2, Connected UV1, R/G/B/A 개별 채널 분리 출력)
			const auto pbrSample = Add(multiTexGraph, registry, "Shader.TextureSample");
			Accept(multiTexGraph.SetInputValue(registry, { pbrSample, "slot" }, std::int64_t{ 2 }));
			Accept(multiTexGraph.SetInputValue(registry, { pbrSample, "name" }, std::string("PbrPacked")));
			const auto uv1Node = Add(multiTexGraph, registry, "Shader.TexCoord1");
			Accept(multiTexGraph.Connect(registry, { uv1Node, "uv" }, { pbrSample, "uv" }));
			Accept(multiTexGraph.Connect(registry, { pbrSample, "r" }, { out, "roughness" }));
			Accept(multiTexGraph.Connect(registry, { pbrSample, "g" }, { out, "metallic" }));
			Accept(multiTexGraph.Connect(registry, { pbrSample, "b" }, { out, "cavity" }));
			Accept(multiTexGraph.Connect(registry, { pbrSample, "a" }, { out, "opacityMask" }));

			// 4) Emissive (Slot 3, Default UV0, RGB 출력)
			const auto emissiveSample = Add(multiTexGraph, registry, "Shader.TextureSample");
			Accept(multiTexGraph.SetInputValue(registry, { emissiveSample, "slot" }, std::int64_t{ 3 }));
			Accept(multiTexGraph.SetInputValue(registry, { emissiveSample, "name" }, std::string("EmissiveMap")));
			Accept(multiTexGraph.Connect(registry, { emissiveSample, "rgb" }, { out, "emissive" }));

			// 5) 동일 슬롯(Slot 0) 다중 노드 참조 검증용 Detail Albedo 노드
			const auto detailAlbedoSample = Add(multiTexGraph, registry, "Shader.TextureSample");
			Accept(multiTexGraph.SetInputValue(registry, { detailAlbedoSample, "slot" }, std::int64_t{ 0 }));
			Accept(multiTexGraph.SetInputValue(registry, { detailAlbedoSample, "name" }, std::string("DetailAlbedo")));

			// HLSL 코드 생성 검증
			std::string multiPs;
			std::string multiFt;
			Accept(ShaderGraphCompiler::GenerateGBufferMaterialHlsl(multiTexGraph, registry, multiPs));
			Accept(ShaderGraphCompiler::GenerateForwardTransparentMaterialHlsl(multiTexGraph, registry, multiFt));

			// Space 1 및 t0~t3, s0~s3 바인딩 레지스터 검증
			for (const auto* shader : { &multiPs, &multiFt })
			{
				Expect(shader->find("register(t0, space1)") != std::string::npos &&
					shader->find("register(s0, space1)") != std::string::npos, "Slot 0 binding mismatch.");
				Expect(shader->find("register(t1, space1)") != std::string::npos &&
					shader->find("register(s1, space1)") != std::string::npos, "Slot 1 binding mismatch.");
				Expect(shader->find("register(t2, space1)") != std::string::npos &&
					shader->find("register(s2, space1)") != std::string::npos, "Slot 2 binding mismatch.");
				Expect(shader->find("register(t3, space1)") != std::string::npos &&
					shader->find("register(s3, space1)") != std::string::npos, "Slot 3 binding mismatch.");

				Expect(shader->find("SampleGraphTexture0(float2 uv)") != std::string::npos, "Missing SampleGraphTexture0 helper.");
				Expect(shader->find("SampleGraphTexture1(float2 uv)") != std::string::npos, "Missing SampleGraphTexture1 helper.");
				Expect(shader->find("SampleGraphTexture2(float2 uv)") != std::string::npos, "Missing SampleGraphTexture2 helper.");
				Expect(shader->find("SampleGraphTexture3(float2 uv)") != std::string::npos, "Missing SampleGraphTexture3 helper.");
			}

			// GBuffer PS 표현식 검증
			Expect(multiPs.find("SampleGraphTexture0(input.TexCoord)") != std::string::npos,
				"Albedo sample must default to input.TexCoord (UV0).");
			Expect(multiPs.find("(SampleGraphTexture1((input.TexCoord * float2(2.00000000e+00f, 2.00000000e+00f)))).rgb") != std::string::npos,
				"Normal sample must use connected custom scaled UV and .rgb swizzle.");
			Expect(multiPs.find("(SampleGraphTexture2(input.TexCoord1)).r") != std::string::npos,
				"Roughness must use SampleGraphTexture2(input.TexCoord1).r.");
			Expect(multiPs.find("(SampleGraphTexture2(input.TexCoord1)).g") != std::string::npos,
				"Metallic must use SampleGraphTexture2(input.TexCoord1).g.");
			Expect(multiPs.find("(SampleGraphTexture2(input.TexCoord1)).b") != std::string::npos,
				"Cavity must use SampleGraphTexture2(input.TexCoord1).b.");
			Expect(multiPs.find("float opacityMask = (SampleGraphTexture2(input.TexCoord1)).a;") != std::string::npos &&
				multiPs.find("clip(opacityMask - 0.5f);") != std::string::npos,
				"OpacityMask must use SampleGraphTexture2(input.TexCoord1).a in clip expression.");
			Expect(multiPs.find("(SampleGraphTexture3(input.TexCoord)).rgb") != std::string::npos,
				"Emissive must use SampleGraphTexture3(input.TexCoord).rgb.");

			// 텍스처 파라미터 / 슬롯 메타데이터 수집(CollectTextureDescriptors / CollectTextureParameters) 검증
			const auto descriptors = ShaderGraphCompiler::CollectTextureDescriptors(multiTexGraph);
			const auto params = ShaderGraphCompiler::CollectTextureParameters(multiTexGraph);
			Expect(descriptors.size() == 4, "Must collect exactly 4 deduplicated texture slot descriptors.");
			Expect(descriptors == params, "CollectTextureDescriptors and CollectTextureParameters must match.");

			Expect(descriptors[0].Slot == 0 && descriptors[0].Name == "BaseAlbedo", "Slot 0 descriptor mismatch.");
			Expect(descriptors[1].Slot == 1 && descriptors[1].Name == "NormalTexture", "Slot 1 descriptor mismatch.");
			Expect(descriptors[2].Slot == 2 && descriptors[2].Name == "PbrPacked", "Slot 2 descriptor mismatch.");
			Expect(descriptors[3].Slot == 3 && descriptors[3].Name == "EmissiveMap", "Slot 3 descriptor mismatch.");

			// MaterialInterface 동기화 검증
			TestMaterial syncMat;
			syncMat.SyncTextureBindings(descriptors);
			Expect(syncMat.GetTextureBindings().size() == 4, "Material must contain 4 texture bindings.");
			Expect(syncMat.HasTextureBindingBySlot(0), "Material missing slot 0 binding.");
			Expect(syncMat.HasTextureBindingBySlot(1), "Material missing slot 1 binding.");
			Expect(syncMat.HasTextureBindingBySlot(2), "Material missing slot 2 binding.");
			Expect(syncMat.HasTextureBindingBySlot(3), "Material missing slot 3 binding.");
			Expect(syncMat.HasTextureBinding("BaseAlbedo"), "Material missing BaseAlbedo binding by name.");
			Expect(syncMat.HasTextureBinding("NormalTexture"), "Material missing NormalTexture binding by name.");
			Expect(syncMat.HasTextureBinding("PbrPacked"), "Material missing PbrPacked binding by name.");
			Expect(syncMat.HasTextureBinding("EmissiveMap"), "Material missing EmissiveMap binding by name.");
			Expect(syncMat.GetTextureDescriptors().size() == 4, "GetTextureDescriptors mismatch.");
		}

		// ── 11. MaterialInterface 단일 순회 상수 해석 우선순위 및 상수 버퍼 캐시 무효화 검증 ──
		{
			TestMaterial cachedMat;
			cachedMat.SetFloat4("color", Engine::vec4(0.1f, 0.2f, 0.3f, 1.0f));
			cachedMat.SetFloat4("BaseColor", Engine::vec4(0.8f, 0.6f, 0.4f, 1.0f));
			cachedMat.SetFloat3("emissive", Engine::vec3(0.1f, 0.1f, 0.1f));
			cachedMat.SetFloat3("EmissiveColor", Engine::vec3(1.0f, 0.5f, 0.25f));
			cachedMat.SetFloat("roughness", 0.35f);
			cachedMat.SetFloat("METALLIC", 0.75f);
			cachedMat.SetFloat("Cavity", 0.6f);
			cachedMat.SetFloat("ao", 0.9f);

			const auto& blob1 = cachedMat.GetOrBuildConstantBufferBlob(0u);
			Expect(blob1.size() >= sizeof(Engine::MaterialConstants), "Constant buffer blob must hold MaterialConstants.");
			Engine::MaterialConstants mc1{};
			std::memcpy(&mc1, blob1.data(), sizeof(mc1));
			Expect(std::abs(mc1.baseColor.r - 0.8f) < 1e-5f, "BaseColor must take precedence over Color.");
			Expect(std::abs(mc1.emissiveColor.r - 1.0f) < 1e-5f && std::abs(mc1.emissiveColor.g - 0.5f) < 1e-5f,
				"EmissiveColor must take precedence over Emissive.");
			Expect(std::abs(mc1.roughness - 0.35f) < 1e-5f && std::abs(mc1.metallic - 0.75f) < 1e-5f &&
				std::abs(mc1.cavity - 0.6f) < 1e-5f && std::abs(mc1.ao - 0.9f) < 1e-5f,
				"Single-pass case-insensitive scalar parameters must populate MaterialConstants accurately.");

			// 동일 버전/마스크 재호출 시 캐시된 버퍼를 그대로 반환해야 함
			const uint8_t* cachedPtr = blob1.data();
			const auto& blobHit = cachedMat.GetOrBuildConstantBufferBlob(0u);
			Expect(blobHit.data() == cachedPtr, "Cache hit must reuse the same constant buffer allocation.");

			// 파라미터 및 블렌드 모드 변경 시 캐시 무효화 및 재반영 검증
			cachedMat.SetFloat("roughness", 0.85f);
			cachedMat.SetBlendMode(Engine::EBlendMode::Masked);
			cachedMat.SetAlphaCutoff(0.4f);
			const auto& blob2 = cachedMat.GetOrBuildConstantBufferBlob(0u);
			Engine::MaterialConstants mc2{};
			std::memcpy(&mc2, blob2.data(), sizeof(mc2));
			Expect(std::abs(mc2.roughness - 0.85f) < 1e-5f, "Parameter update must invalidate constant buffer cache.");
			Expect((mc2.materialFlags >> Engine::MATERIAL_ALPHA_CUTOFF_SHIFT) > 0u,
				"Masked blend mode and alpha cutoff update must invalidate constant buffer cache.");
		}

		// ── 지형 도메인(T3-b): 지형 노드·기본 레이어 합성·정점 셰이더 입력 일치·다른 생성기에서의 거부 ──
		{
			// 출력만 있는 지형 그래프: baseColor 기본값이 레이어 합성이고 공용 헤더를 포함한다.
			Graph terrainGraph{ EGraphDomain::Shader };
			const auto terrainOutput = Add(terrainGraph, registry, "Shader.MaterialOutput");
			std::string terrainSource;
			Accept(ShaderGraphCompiler::GenerateTerrainMaterialHlsl(terrainGraph, registry, terrainSource));
			Expect(terrainSource.find("#include \"terrainLayers.hlsli\"") != std::string::npos, "Terrain shader must include the shared terrain header.");
			Expect(terrainSource.find("TerrainBlendLayers(terrainPixel)") != std::string::npos, "Unlinked terrain baseColor must blend painted layers.");
			Expect(terrainSource.find("TexCoord1") == std::string::npos, "Terrain pixel input must match terrainVS (no second UV).");
			Expect(terrainSource.find("#define TERRAIN_SAMPLER") == std::string::npos, "Terrain shader must not alias the sampler without a graph texture.");

			// 레이어 3 샘플을 baseColor로, 경사로 거칠기를 정한다.
			const auto layerSample = Add(terrainGraph, registry, "Terrain.LayerSample");
			Accept(terrainGraph.SetInputValue(registry, { layerSample, "layer" }, std::int64_t{ 3 }));
			Accept(terrainGraph.Connect(registry, { layerSample, "rgba" }, { terrainOutput, "baseColor" }));
			const auto slope = Add(terrainGraph, registry, "Terrain.Slope");
			Accept(terrainGraph.Connect(registry, { slope, "slope" }, { terrainOutput, "roughness" }));
			Accept(ShaderGraphCompiler::GenerateTerrainMaterialHlsl(terrainGraph, registry, terrainSource));
			Expect(terrainSource.find("TerrainSampleLayer(terrainPixel, 3u)") != std::string::npos, "LayerSample must sample the chosen layer.");
			Expect(terrainSource.find("terrainPixel.Slope") != std::string::npos, "Slope node must read the terrain pixel slope.");

			// 지형 노드는 일반 G-Buffer 생성기에서 컴파일하지 않는다(terrainPixel이 없다).
			std::string gbufferWithTerrain = "previous";
			const auto rejected = ShaderGraphCompiler::GenerateGBufferMaterialHlsl(terrainGraph, registry, gbufferWithTerrain);
			Expect(!rejected.Succeeded() && rejected.Code == EGraphError::UnsupportedCompilation && gbufferWithTerrain == "previous",
				"Terrain nodes must be rejected outside the terrain domain without replacing the source.");

			// 범위 밖 레이어 번호는 거부한다.
			Accept(terrainGraph.SetInputValue(registry, { layerSample, "layer" }, std::int64_t{ 9 }));
			const auto badLayer = ShaderGraphCompiler::GenerateTerrainMaterialHlsl(terrainGraph, registry, terrainSource);
			Expect(!badLayer.Succeeded() && badLayer.Code == EGraphError::InvalidDefault, "Terrain layer outside 0-7 must be rejected.");
			Accept(terrainGraph.SetInputValue(registry, { layerSample, "layer" }, std::int64_t{ 7 }));

			// 두 번째 UV는 지형 정점 셰이더가 내지 않으므로 거부한다.
			Graph uv1Graph{ EGraphDomain::Shader };
			const auto uv1Output = Add(uv1Graph, registry, "Shader.MaterialOutput");
			const auto uv1 = Add(uv1Graph, registry, "Shader.TexCoord1");
			const auto uv1Sample = Add(uv1Graph, registry, "Shader.TextureSample");
			Accept(uv1Graph.Connect(registry, { uv1, "uv" }, { uv1Sample, "uv" }));
			Accept(uv1Graph.Connect(registry, { uv1Sample, "rgba" }, { uv1Output, "baseColor" }));
			std::string uv1Source;
			const auto uv1Result = ShaderGraphCompiler::GenerateTerrainMaterialHlsl(uv1Graph, registry, uv1Source);
			Expect(!uv1Result.Succeeded() && uv1Result.Code == EGraphError::UnsupportedCompilation, "Terrain domain must reject the second UV set.");

			// 그래프가 텍스처 슬롯 0(materialSampler)을 쓰면 지형 헤더가 같은 샘플러를 쓴다.
			Graph samplerGraph{ EGraphDomain::Shader };
			const auto samplerOutput = Add(samplerGraph, registry, "Shader.MaterialOutput");
			const auto slot0 = Add(samplerGraph, registry, "Shader.TextureSample");
			Accept(samplerGraph.Connect(registry, { slot0, "rgba" }, { samplerOutput, "baseColor" }));
			std::string samplerSource;
			Accept(ShaderGraphCompiler::GenerateTerrainMaterialHlsl(samplerGraph, registry, samplerSource));
			const auto aliasPos = samplerSource.find("#define TERRAIN_SAMPLER materialSampler");
			const auto includePos = samplerSource.find("#include \"terrainLayers.hlsli\"");
			Expect(aliasPos != std::string::npos && includePos != std::string::npos && aliasPos < includePos,
				"Terrain header must reuse materialSampler when the graph declares slot 0.");
		}

		// ── 기본 지형 그래프(T3-c): terrainPS.hlsl과 같은 식을 만든다 ──
		{
			Graph defaultTerrain;
			Accept(ShaderGraphCompiler::BuildDefaultTerrainGraph(registry, defaultTerrain));
			Expect(defaultTerrain.GetDomain() == EGraphDomain::Shader, "Default terrain graph must be a shader graph.");
			std::string defaultSource;
			Accept(ShaderGraphCompiler::GenerateTerrainMaterialHlsl(defaultTerrain, registry, defaultSource));
			const auto contains = [&](const char* text) { return defaultSource.find(text) != std::string::npos; };
			Expect(contains("TerrainBlendLayers(terrainPixel)"), "Default terrain graph must blend painted layers.");
			Expect(contains("TerrainSampleLayer(terrainPixel, 1u)"), "Default terrain graph must use layer 1 for cliffs.");
			Expect(contains("terrainPixel.Slope"), "Default terrain graph must read the slope.");
			Expect(contains("smoothstep("), "Default terrain graph must use smoothstep for the cliff transition.");
			Expect(!contains("gMaterial.metallic") && !contains("gMaterial.cavity") && !contains("gMaterial.ao") && !contains("gMaterial.roughness"),
				"Default terrain graph must fix PBR values like terrainPS instead of reading material constants.");

			// 같은 그래프는 일반 G-Buffer 머티리얼로는 컴파일되지 않는다(지형 노드 사용).
			std::string notTerrain;
			const auto rejected = ShaderGraphCompiler::GenerateGBufferMaterialHlsl(defaultTerrain, registry, notTerrain);
			Expect(!rejected.Succeeded() && rejected.Code == EGraphError::UnsupportedCompilation,
				"Default terrain graph must not compile as a surface material.");
		}

		// ── 11. 디스토션 V1 (Shader.ScreenUV 및 Shader.SceneColor) 검증 ──
		{
			// (1) ScreenUV 및 SceneColor 기본 연결 (강도 0: 왜곡 없음)
			Graph distortGraph{ EGraphDomain::Shader };
			const auto matOut = Add(distortGraph, registry, "Shader.MaterialOutput");
			const auto screenUv = Add(distortGraph, registry, "Shader.ScreenUV");
			const auto sceneColor = Add(distortGraph, registry, "Shader.SceneColor");

			const auto directUvLink = distortGraph.Connect(registry, { screenUv, "uv" }, { sceneColor, "uv" });
			Accept(directUvLink);
			Accept(distortGraph.Connect(registry, { sceneColor, "rgba" }, { matOut, "baseColor" }));

			std::string ftSource;
			Accept(ShaderGraphCompiler::GenerateForwardTransparentMaterialHlsl(distortGraph, registry, ftSource));

			// SceneColorCopy 바인딩 선언 및 안전 샘플러 함수 검증
			Expect(ftSource.find("Texture2D g_SceneColor : register(t0, space3);") != std::string::npos,
				"ForwardTransparent PS must declare g_SceneColor on t0, space3.");
			Expect(ftSource.find("SamplerState g_SceneColorSampler : register(s0, space3);") != std::string::npos,
				"ForwardTransparent PS must declare g_SceneColorSampler on s0, space3.");
			Expect(ftSource.find("float4 SampleSceneColor(float2 uv, float2 baseScreenUV)") != std::string::npos,
				"ForwardTransparent PS must define SampleSceneColor helper.");
			Expect(ftSource.find("bool isOffScreen = any(uv < 0.0f) || any(uv > 1.0f);") != std::string::npos &&
				ftSource.find("float2 finalUV = isOffScreen ? baseScreenUV : uv;") != std::string::npos &&
				ftSource.find("clamp(finalUV, float2(0.0001f, 0.0001f), float2(0.9999f, 0.9999f))") != std::string::npos,
				"SampleSceneColor must fall back to baseScreenUV when off-screen and clamp finalUV.");
			Expect(ftSource.find("float2 screenUV = input.Position.xy / float2(max(_sceneWidth, 1.0f), max(_sceneHeight, 1.0f));") != std::string::npos,
				"ForwardTransparent PS must compute screenUV from SV_Position and g_SceneColor dimensions.");

			// 강도 0 연결식: SampleSceneColor(screenUV, screenUV)
			Expect(ftSource.find("SampleSceneColor(screenUV, screenUV)") != std::string::npos,
				"Direct connection from ScreenUV to SceneColor must generate SampleSceneColor(screenUV, screenUV).");

			// (2) 왜곡 오프셋 적용 검증 (uv = screenUV + offset)
			const auto addNode = Add(distortGraph, registry, "Math.AddFloat2");
			const auto offsetConst = Add(distortGraph, registry, "Core.Float2");
			Accept(distortGraph.SetInputValue(registry, { offsetConst, "value" }, std::array<float, 2>{ 0.0625f, -0.125f })); // 2진수로 정확히 표현되는 값(0.05f는 출력이 5.00000007e-02f가 되어 문자열 비교가 깨진다)
			Accept(distortGraph.Connect(registry, { screenUv, "uv" }, { addNode, "a" }));
			Accept(distortGraph.Connect(registry, { offsetConst, "result" }, { addNode, "b" }));
			// 데이터 입력은 연결을 하나만 받으므로 (1)의 직접 연결을 먼저 끊고 오프셋 결과로 바꾼다.
			Accept(distortGraph.Disconnect(directUvLink.Link));
			Accept(distortGraph.Connect(registry, { addNode, "result" }, { sceneColor, "uv" }));

			std::string offsetFtSource;
			Accept(ShaderGraphCompiler::GenerateForwardTransparentMaterialHlsl(distortGraph, registry, offsetFtSource));
			Expect(offsetFtSource.find("SampleSceneColor((screenUV + float2(6.25000000e-02f, -1.25000000e-01f)), screenUV)") != std::string::npos,
				"Distorted UV must pass perturbed UV as first arg and clean screenUV as second arg.");

			// (3) 미연결 uv 입력 시 기본 screenUV 폴백 검증
			Graph fallbackGraph{ EGraphDomain::Shader };
			const auto fbMatOut = Add(fallbackGraph, registry, "Shader.MaterialOutput");
			const auto fbSceneColor = Add(fallbackGraph, registry, "Shader.SceneColor");
			Accept(fallbackGraph.Connect(registry, { fbSceneColor, "rgb" }, { fbMatOut, "emissive" }));

			std::string fbFtSource;
			Accept(ShaderGraphCompiler::GenerateForwardTransparentMaterialHlsl(fallbackGraph, registry, fbFtSource));
			Expect(fbFtSource.find("(SampleSceneColor(screenUV, screenUV)).rgb") != std::string::npos,
				"Unconnected uv input on SceneColor must fall back to screenUV and support rgb swizzle.");

			// (4) GBuffer 셰이더 생성기 및 정점 단계에서의 사용 거부 검증
			std::string rejectedGbSource;
			const auto gbReject = ShaderGraphCompiler::GenerateGBufferMaterialHlsl(distortGraph, registry, rejectedGbSource);
			Expect(!gbReject.Succeeded() && gbReject.Code == EGraphError::UnsupportedCompilation,
				"Distortion nodes must be rejected when compiling GBuffer material PS.");

			// 정점 생성기는 worldPositionOffset·customUV에 연결된 노드만 읽으므로, 거부되는지 보려면 거기에 연결해야 한다.
			Accept(distortGraph.Connect(registry, { sceneColor, "rgb" }, { matOut, "worldPositionOffset" }));
			std::string rejectedVsSource;
			const auto vsReject = ShaderGraphCompiler::GenerateGBufferMaterialVS(distortGraph, registry, rejectedVsSource);
			Expect(!vsReject.Succeeded() && vsReject.Code == EGraphError::UnsupportedCompilation,
				"Distortion nodes must be rejected when compiling vertex shader stages.");
		}

		std::cout << "Shader graph checks passed.\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
