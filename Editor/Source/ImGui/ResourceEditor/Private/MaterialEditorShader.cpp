#include "MaterialEditor.h"
#include "IconsFontAwesome7.h"
#include "GraphMessageLocalization.h"
#include "ShaderGraphCompiler.h"
#include "LocalizationManager.h"
#include <imgui_stdlib.h>
#include "GraphShaderPreview.h"
#include "ImGuiManager.h"
#include "PlatformPath.h"
#include "Renderer.h"
#include "ResourceManager.h"
#include "RenderPass.h"
#include "Mesh.h"
#include "Texture.h"
#include "Shader.h"
#include "SamplerManager.h"
#include "Logger.h"
#include <fstream>
#include <chrono>

namespace
{
	std::filesystem::path ReserveMaterialShaderSourcePath()
	{
		namespace fs = std::filesystem;
		const auto root = Engine::PlatformPath::ResolveProjectRelative("Temp/MaterialShader");
		fs::create_directories(root);

		const auto stamp = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
		for (unsigned attempt = 0; attempt < 64; ++attempt)
		{
			const auto directory = root / (stamp + "-" + std::to_string(attempt));
			std::error_code error;

			if (fs::create_directory(directory, error))
			{
				// 생성 HLSL은 #include "shading_flags.hlsli"를 쓰고, 컴파일러는 소스 폴더 기준으로 찾는다.
				// 기존 GraphPanel과 같이 엔진 원본을 같은 폴더에 복사해 둔다(없으면 컴파일이 항상 실패한다).
				const auto hlslDirectory = directory / "hlsl";
				fs::create_directory(hlslDirectory);
				fs::copy_file(Engine::PlatformPath::ResolveUri(L"Engine://Shader/hlsl/shading_flags.hlsli"),
					hlslDirectory / "shading_flags.hlsli", fs::copy_options::overwrite_existing);
				return hlslDirectory / "MaterialPS.hlsl";
			}

			if (error && error != std::errc::file_exists)
				throw fs::filesystem_error("create_directory", directory, error);
		}

		throw fs::filesystem_error("create_directory", root, std::make_error_code(std::errc::file_exists));
	}
}

namespace Editor
{
	void MaterialEditor::InvalidateShaderBuild()
	{
		auto& build = m_ShaderGraph.ShaderBuild;
		build.SourceCurrent = false;
		build.MaterialCurrent = false;
		build.StatusKey = "GraphPanel.ShaderChanged";
		build.Details.clear();
	}

	bool MaterialEditor::GenerateShaderSource()
	{
		auto& build = m_ShaderGraph.ShaderBuild;
		std::string candidate;

		m_LastGraphDiagnostic = Engine::ShaderGraphCompiler::GenerateColorHlsl(m_ShaderGraph.Data, m_GraphRegistry, candidate);
		build.Details.clear();

		if (!m_LastGraphDiagnostic.Succeeded())
		{
			build.SourceCurrent = false;
			build.StatusKey = "MaterialEditor.CompileFailed";
			return false;
		}

		build.Source.swap(candidate);
		build.SourceCurrent = true;
		build.StatusKey = "MaterialEditor.SourceGenerated";
		return true;
	}

	bool MaterialEditor::CompileShaderGraph(Engine::ERHIType target)
	{
		if (!GenerateShaderSource()) return false;

		auto& build = m_ShaderGraph.ShaderBuild;
		try
		{
			const auto sourcePath = ReserveMaterialShaderSourcePath();

			std::ofstream file;
			file.exceptions(std::ios::badbit | std::ios::failbit);
			file.open(sourcePath, std::ios::binary | std::ios::trunc);
			file.write(build.Source.data(), static_cast<std::streamsize>(build.Source.size()));
			file.close();

			auto result = ShaderCompiler::Compile(sourcePath, EShaderType::Fragment, target, "main");
			build.Details = result.ErrorMessage;

			if (!result.Success || result.Bytecode.empty())
			{
				build.StatusKey = "MaterialEditor.CompileFailed";
				return false;
			}

			build.LastGood = std::move(result);
			build.LastTarget = target;
			build.StatusKey = "MaterialEditor.CompilePassed";
			return true;
		}
		catch (const std::exception& error)
		{
			build.StatusKey = "MaterialEditor.CompileFailed";
			build.Details = error.what();
			return false;
		}
	}

	void MaterialEditor::CompileMaterialShader()
	{
		auto& build = m_ShaderGraph.ShaderBuild;
		std::string candidate;

		m_LastGraphDiagnostic = Engine::ShaderGraphCompiler::GenerateGBufferMaterialHlsl(m_ShaderGraph.Data, m_GraphRegistry, candidate);
		build.Details.clear();

		if (!m_LastGraphDiagnostic.Succeeded())
		{
			build.MaterialCurrent = false;
			build.StatusKey = "MaterialEditor.CompileFailed";
			return;
		}

		try
		{
			const auto sourcePath = ReserveMaterialShaderSourcePath();

			std::ofstream file;
			file.exceptions(std::ios::badbit | std::ios::failbit);
			file.open(sourcePath, std::ios::binary | std::ios::trunc);
			file.write(candidate.data(), static_cast<std::streamsize>(candidate.size()));
			file.close();

			auto result = ShaderCompiler::Compile(sourcePath, EShaderType::Fragment, Engine::ERHIType::DirectX12, "main");
			build.Details = result.ErrorMessage;

			if (!result.Success || result.Bytecode.empty())
			{
				build.MaterialCurrent = false;
				build.StatusKey = "MaterialEditor.CompileFailed";
				return;
			}

			build.MaterialLastGood = std::move(result);
			build.MaterialCurrent = true;
			build.StatusKey = "MaterialEditor.CompilePassed";
		}
		catch (const std::exception& error)
		{
			build.MaterialCurrent = false;
			build.StatusKey = "MaterialEditor.CompileFailed";
			build.Details = error.what();
		}
	}

	void MaterialEditor::LiveHotReloadShader()
	{
		// 그래프가 연결되어 실제로 읽힌 머티리얼에만 컴파일 결과를 적용한다. 읽기 전용 머티리얼의 셰이더는 바꾸지 않는다.
		if (!m_Material || m_IsReadOnly || !m_GraphReady || !HasLinkedShaderGraph()) return;

		auto& build = m_ShaderGraph.ShaderBuild;
		std::string candidateHlsl;
		auto diag = Engine::ShaderGraphCompiler::GenerateGBufferMaterialHlsl(m_ShaderGraph.Data, m_GraphRegistry, candidateHlsl);
		if (!diag.Succeeded())
		{
			build.StatusKey = "MaterialEditor.CompileFailed";
			build.Details = diag.Message;
			return;
		}

		try
		{
			const auto sourcePath = ReserveMaterialShaderSourcePath();

			std::ofstream file(sourcePath, std::ios::binary | std::ios::trunc);
			file.write(candidateHlsl.data(), static_cast<std::streamsize>(candidateHlsl.size()));
			file.close();

			const auto rhiType = Engine::Renderer::IsCreated() ? Engine::Renderer::Get().GetRHIType() : Engine::ERHIType::DirectX12;
			auto result = ShaderCompiler::Compile(sourcePath, EShaderType::Fragment, rhiType, "main");
			if (!result.Success || result.Bytecode.empty())
			{
				build.StatusKey = "MaterialEditor.CompileFailed";
				build.Details = result.ErrorMessage;
				return;
			}

			build.MaterialLastGood = result;
			build.MaterialCurrent = true;
			build.StatusKey = "MaterialEditor.CompilePassed";
			build.Details.clear();

			// 런타임 셰이더 바이트코드 갱신
			auto& rm = Engine::ResourceManager::Get();
			std::string baseName = m_TargetResourcePath.stem().string();
			const std::string bammat = ".bammat";
			if (baseName.size() > bammat.size() && baseName.ends_with(bammat))
				baseName = baseName.substr(0, baseName.size() - bammat.size());
			const std::wstring shaderKey = L"LiveShader/" + StrToWStr(baseName) + L"_PS";

			auto shaderHandle = rm.GetResourceHandle<Engine::Shader>(shaderKey);
			if (shaderHandle.IsValid() && shaderHandle.Get())
			{
				shaderHandle->UpdateBytecode(result.Bytecode);
			}
			else
			{
				ShaderDesc desc{};
				desc.Key = shaderKey;
				desc.Path = result.OutputPath.wstring();
				if (rhiType == Engine::ERHIType::DirectX12) desc.dxilPath = result.OutputPath.wstring();
				else if (rhiType == Engine::ERHIType::WebGPU) desc.wgslPath = result.OutputPath.wstring();
				else desc.spirvPath = result.OutputPath.wstring();
				desc.shaderType = EShaderType::Fragment;
				desc.entryPoint = "main";
				auto* newShader = Engine::Shader::Create(&desc);
				if (newShader)
				{
					shaderHandle = rm.AddResource<Engine::Shader>(shaderKey, newShader);
				}
			}

			if (shaderHandle.IsValid())
			{
				m_LivePixelShader = shaderHandle;
				m_Material->SetPixelShaderHandle(m_LivePixelShader);

				// 머티리얼에 Geometry 패스 셰이더가 등록되어 있다면 함께 갱신하여 렌더링에 즉각 반영
				if (m_Material->HasPassShader(Engine::ERenderPassType::Geometry))
				{
					auto vs = m_Material->GetVertexShaderHandle(Engine::ERenderPassType::Geometry);
					m_Material->SetPassShader(Engine::ERenderPassType::Geometry, vs, m_LivePixelShader);
				}
				m_Material->IncreaseVersion();
				m_PreviewDirtyTimer = 0.5f;
			}
		}
		catch (const std::exception& e)
		{
			build.StatusKey = "MaterialEditor.CompileFailed";
			build.Details = e.what();
		}
	}

	void MaterialEditor::CreateRuntimeMaterial()
	{
		CompileMaterialShader();
		LiveHotReloadShader();
	}
}
