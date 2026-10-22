#include "GraphPanel.h"
#include "GraphPanel.h"
#include "GraphPanel.h"
#include "GraphMessageLocalization.h"
#include "ShaderGraphCompiler.h"
#include "LocalizationManager.h"
#include <imgui_stdlib.h>
#include <chrono>
#include <fstream>
#include <system_error>
#include <utility>
#include "GraphShaderPreview.h"
#include "ImGuiManager.h"
#include <algorithm>

namespace
{
	std::filesystem::path ReserveGraphShaderSourcePath()
	{
		namespace fs = std::filesystem;

		const auto root = fs::absolute("Temp/GraphShader");
		fs::create_directories(root);

		const auto stamp = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
		for (unsigned attempt = 0; attempt < 64; ++attempt)
		{
			const auto directory = root / (stamp + "-" + std::to_string(attempt));
			std::error_code error;

			if (fs::create_directory(directory, error))
			{
				fs::create_directory(directory / "hlsl");
				return directory / "hlsl" / "GraphPS.hlsl";
			}

			if (error && error != std::errc::file_exists)
				throw fs::filesystem_error("create_directory", directory, error);
		}

		throw fs::filesystem_error("create_directory", root, std::make_error_code(std::errc::file_exists));
	}
}

void GraphPanel::InvalidateShaderBuild()
{
	auto& build = m_Shader.ShaderBuild;
	build.SourceCurrent = false;
	build.StatusKey = "GraphPanel.ShaderChanged";
	build.Details.clear();
}

bool GraphPanel::GenerateShaderSource()
{
	if (m_Domain != EGraphDomain::Shader) return false;

	auto& build = m_Shader.ShaderBuild;
	std::string candidate;

	m_LastDiagnostic = ShaderGraphCompiler::GenerateColorHlsl(m_Shader.Data, m_Registry, candidate);
	build.Details.clear();

	if (!m_LastDiagnostic.Succeeded())
	{
		build.SourceCurrent = false;
		build.StatusKey = "GraphPanel.ShaderGenerationFailed";
		return false;
	}

	build.Source.swap(candidate);
	build.SourceCurrent = true;
	build.StatusKey = "GraphPanel.ShaderSourceReady";
	return true;
}

bool GraphPanel::CompileShaderGraph(ERHIType target)
{
	if (!GenerateShaderSource()) return false;

	auto& build = m_Shader.ShaderBuild;
	try
	{
		const auto sourcePath = ReserveGraphShaderSourcePath();

		std::ofstream file;
		file.exceptions(std::ios::badbit | std::ios::failbit);
		file.open(sourcePath, std::ios::binary | std::ios::trunc);
		file.write(build.Source.data(), static_cast<std::streamsize>(build.Source.size()));
		file.close();

		auto result = ShaderCompiler::Compile(sourcePath, EShaderType::Fragment, target, "main");
		build.Details = result.ErrorMessage;

		if (!result.Success || result.Bytecode.empty())
		{
			build.StatusKey = "GraphPanel.ShaderCompileFailed";
			return false;
		}

		build.LastGood = std::move(result);
		build.LastTarget = target;
		build.StatusKey = "GraphPanel.ShaderCompilePassed";
		return true;
	}
	catch (const std::exception& error)
	{
		build.StatusKey = "GraphPanel.ShaderCompileFailed";
		build.Details = error.what();
		return false;
	}
}

void GraphPanel::DrawShaderToolbar()
{
	if (m_Domain != EGraphDomain::Shader) return;

	if (ImGui::Button(LOCAL_LABEL("GraphPanel.GenerateHlsl").c_str())) GenerateShaderSource();
	ImGui::SameLine();
	if (ImGui::Button(LOCAL_LABEL("GraphPanel.CheckDxil").c_str()))
		CompileShaderGraph(ERHIType::DirectX12);
	ImGui::SameLine();
	if (ImGui::Button(LOCAL_LABEL("GraphPanel.CheckSpirv").c_str()))
		CompileShaderGraph(ERHIType::Vulkan);
	ImGui::SameLine();
	if (ImGui::Button(LOCAL_LABEL("GraphPanel.PreviewShader").c_str()))
		BuildShaderPreview();
	ImGui::SameLine();
	if (ImGui::Button(LOCAL_LABEL("GraphPanel.CheckMaterialDxil").c_str()))
		CompileMaterialShader();
	ImGui::SameLine();
	if (ImGui::Button(LOCAL_LABEL("GraphPanel.CreateRuntimeMaterial").c_str()))
		CreateRuntimeMaterial();

	ImGui::TextDisabled("%s", LOCAL_CSTR("GraphPanel.ShaderCompileOnly"));
}

void GraphPanel::DrawShaderBuildDetails()
{
	if (m_Domain != EGraphDomain::Shader) return;

	const auto title = LOCAL_LABEL("GraphPanel.ShaderBuild");
	if (!ImGui::CollapsingHeader(title.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) return;

	auto& build = m_Shader.ShaderBuild;
	ImGui::TextWrapped("%s", LOCAL_CSTR(build.StatusKey));

	if (build.LastGood.Success)
	{
		const char* format = build.LastTarget == ERHIType::DirectX12 ? "DXIL" : "SPIR-V";
		const auto summary = LOCAL_FMT("GraphPanel.ShaderLastGood", format, build.LastGood.Bytecode.size());
		ImGui::TextWrapped("%s", summary.c_str());

		const auto path = WStrToStr(build.LastGood.OutputPath.wstring());
		const auto artifact = LOCAL_FMT("GraphPanel.ShaderArtifact", path);
		ImGui::TextWrapped("%s", artifact.c_str());
	}

	if (!build.Details.empty())
	{
		const auto details = LocalizeGraphMessage(build.Details);
		ImGui::BeginChild("##GraphShaderLog", ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing() * 6),
			ImGuiChildFlags_Borders);
		ImGui::TextUnformatted(details.c_str());
		ImGui::EndChild();
	}

	DrawShaderPreview();
	DrawMaterialShaderDetails();

	if (build.Source.empty()) return;

	ImGui::TextUnformatted(LOCAL_CSTR("GraphPanel.GeneratedHlsl"));
	if (!build.SourceCurrent)
		ImGui::TextWrapped("%s", LOCAL_CSTR("GraphPanel.ShaderOldSource"));

	ImGui::InputTextMultiline("##GraphShaderSource", &build.Source,
		ImVec2(-1.0f, ImGui::GetTextLineHeightWithSpacing() * 9), ImGuiInputTextFlags_ReadOnly);
}

void GraphPanel::BuildShaderPreview()
{
	if (m_Domain != EGraphDomain::Shader) return;

	auto& build = m_Shader.ShaderBuild;

	if (!Renderer::IsCreated() || Renderer::Get().GetRHIType() != ERHIType::DirectX12)
	{
		build.StatusKey = "GraphPanel.ShaderPreviewFailed";
		build.Details = "GraphPanel.ShaderPreviewUnavailable";
		return;
	}

	if (!CompileShaderGraph(ERHIType::DirectX12)) return;

	try
	{
		std::string errorKey;
		auto candidate = GraphShaderPreview::Create(build.LastGood, build.LastTarget, build.Source, errorKey);

		if (!candidate)
		{
			build.StatusKey = "GraphPanel.ShaderPreviewFailed";
			build.Details = std::move(errorKey);
			return;
		}

		build.Preview.swap(candidate);
		build.StatusKey = "GraphPanel.ShaderPreviewReady";
		build.Details.clear();
	}
	catch (const std::exception& error)
	{
		build.StatusKey = "GraphPanel.ShaderPreviewFailed";
		build.Details = error.what();
	}
}

void GraphPanel::DrawShaderPreview()
{
	auto& build = m_Shader.ShaderBuild;
	if (!build.Preview) return;

	ImGui::SeparatorText(LOCAL_CSTR("MaterialEditor.Preview"));

	if (!build.SourceCurrent || build.Preview->GetSource() != build.Source)
		ImGui::TextWrapped("%s", LOCAL_CSTR("GraphPanel.ShaderPreviewOld"));

	if (IsFailure(build.Preview->Render()))
	{
		ImGui::TextWrapped("%s", LOCAL_CSTR("GraphPanel.ShaderPreviewDrawFailed"));
		return;
	}

	const auto textureId = ImGuiManager::Get().GetImGuiTextureID(build.Preview->GetTexture());
	if (!textureId)
	{
		ImGui::TextWrapped("%s", LOCAL_CSTR("GraphPanel.ShaderPreviewDrawFailed"));
		return;
	}

	const float size = std::min(256.0f, std::max(1.0f, ImGui::GetContentRegionAvail().x));
	ImGui::Image(textureId, ImVec2(size, size));
}

void GraphPanel::CompileMaterialShader()
{
	if (m_Domain != EGraphDomain::Shader) return;

	auto& build = m_Shader.ShaderBuild;
	build.MaterialCurrent = false;
	build.Details.clear();

	try
	{
		std::string source;
		m_LastDiagnostic = ShaderGraphCompiler::GenerateUnlitMaterialHlsl(m_Shader.Data, m_Registry, source);

		if (!m_LastDiagnostic.Succeeded())
		{
			build.StatusKey = "GraphPanel.ShaderGenerationFailed";
			return;
		}

		const auto sourcePath = ReserveGraphShaderSourcePath().parent_path() / "GraphMaterialPS.hlsl";

		std::ofstream file;
		file.exceptions(std::ios::badbit | std::ios::failbit);
		file.open(sourcePath, std::ios::binary | std::ios::trunc);
		file.write(source.data(), static_cast<std::streamsize>(source.size()));
		file.close();

		std::filesystem::copy_file(
			std::filesystem::absolute("Resources/Shader/hlsl/shading_flags.hlsli"),
			sourcePath.parent_path() / "shading_flags.hlsli");

		auto compiled = ShaderCompiler::Compile(sourcePath, EShaderType::Fragment, ERHIType::DirectX12, "main");
		build.Details = compiled.ErrorMessage;

		if (!compiled.Success || compiled.Bytecode.empty())
		{
			build.StatusKey = "GraphPanel.MaterialShaderFailed";
			return;
		}

		build.MaterialLastGood = std::move(compiled);
		build.MaterialSource.swap(source);
		build.MaterialCurrent = true;
		build.StatusKey = "GraphPanel.MaterialShaderReady";
	}
	catch (const std::exception& error)
	{
		build.StatusKey = "GraphPanel.MaterialShaderFailed";
		build.Details = error.what();
	}
}

void GraphPanel::DrawMaterialShaderDetails()
{
	auto& build = m_Shader.ShaderBuild;
	if (!build.MaterialLastGood.Success) return;

	const auto title = LOCAL_LABEL("GraphPanel.MaterialShaderBuild");
	if (!ImGui::CollapsingHeader(title.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) return;

	ImGui::TextWrapped("%s", LOCAL_CSTR("GraphPanel.MaterialShaderScope"));

	if (!build.MaterialCurrent)
		ImGui::TextWrapped("%s", LOCAL_CSTR("GraphPanel.MaterialShaderOld"));

	const auto summary = LOCAL_FMT("GraphPanel.ShaderLastGood", "DXIL", build.MaterialLastGood.Bytecode.size());
	ImGui::TextWrapped("%s", summary.c_str());

	const auto path = WStrToStr(build.MaterialLastGood.OutputPath.wstring());
	const auto artifact = LOCAL_FMT("GraphPanel.ShaderArtifact", path);
	ImGui::TextWrapped("%s", artifact.c_str());

	DrawRuntimeMaterialDetails();

	ImGui::InputTextMultiline("##GraphMaterialShaderSource", &build.MaterialSource,
		ImVec2(-1.0f, ImGui::GetTextLineHeightWithSpacing() * 9), ImGuiInputTextFlags_ReadOnly);
}