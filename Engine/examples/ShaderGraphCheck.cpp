#include "ShaderGraphCompiler.h"
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
			&ShaderGraphCompiler::GenerateColorHlsl, &ShaderGraphCompiler::GenerateUnlitMaterialHlsl
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

		LocaleScope()
			: Previous(std::locale::global(std::locale(std::locale::classic(), new CommaPunctuation))) {
		}

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

		const auto unsupported = Add(graph, registry, "Core.Float");
		Reject(graph, registry, source, EGraphError::UnsupportedCompilation);
		Accept(graph.RemoveNode(unsupported));

		const auto extraOutput = Add(graph, registry, "Shader.ColorOutput");
		Reject(graph, registry, source, EGraphError::InvalidDocument);
		Accept(graph.RemoveNode(extraOutput));

		Graph empty{ EGraphDomain::Shader };
		Reject(empty, registry, source, EGraphError::InvalidDocument);

		Graph logic{ EGraphDomain::Logic };
		Reject(logic, registry, source, EGraphError::UnsupportedDomain);

		std::cout << "Shader graph checks passed.\n" << source;
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}