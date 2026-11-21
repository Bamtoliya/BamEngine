#include "ShaderGraphCompiler.h"
#include "MaterialInterface.h"
#include "Vertex.h"
#include <algorithm>
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

		std::cout << "Shader graph checks passed.\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
