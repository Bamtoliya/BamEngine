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
	namespace
	{
		std::filesystem::path ReserveMaterialShaderDirectory()
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
					const auto hlslDirectory = directory / "hlsl";
					fs::create_directory(hlslDirectory);
					// PS용 shading_flags.hlsli 복사
					fs::copy_file(Engine::PlatformPath::ResolveUri(L"Engine://Shader/hlsl/shading_flags.hlsli"),
						hlslDirectory / "shading_flags.hlsli", fs::copy_options::overwrite_existing);
					// VS용 common.hlsli 복사
					fs::copy_file(Engine::PlatformPath::ResolveUri(L"Engine://Shader/hlsl/common.hlsli"),
						hlslDirectory / "common.hlsli", fs::copy_options::overwrite_existing);
					return hlslDirectory;
				}

				if (error && error != std::errc::file_exists)
					throw fs::filesystem_error("create_directory", directory, error);
			}

			throw fs::filesystem_error("create_directory", root, std::make_error_code(std::errc::file_exists));
		}
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
			const auto sourcePath = ReserveMaterialShaderDirectory() / "MaterialPS.hlsl";

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
		std::string candidateVs;
		std::string candidatePs;

		m_LastGraphDiagnostic = Engine::ShaderGraphCompiler::GenerateGBufferMaterialVS(m_ShaderGraph.Data, m_GraphRegistry, candidateVs);
		if (!m_LastGraphDiagnostic.Succeeded())
		{
			build.MaterialCurrent = false;
			build.StatusKey = "MaterialEditor.CompileFailed";
			build.Details = m_LastGraphDiagnostic.Message;
			return;
		}

		m_LastGraphDiagnostic = Engine::ShaderGraphCompiler::GenerateGBufferMaterialHlsl(m_ShaderGraph.Data, m_GraphRegistry, candidatePs);
		build.Details.clear();

		if (!m_LastGraphDiagnostic.Succeeded())
		{
			build.MaterialCurrent = false;
			build.StatusKey = "MaterialEditor.CompileFailed";
			build.Details = m_LastGraphDiagnostic.Message;
			return;
		}

		try
		{
			const auto hlslDir = ReserveMaterialShaderDirectory();
			const auto vsPath = hlslDir / "MaterialVS.hlsl";
			const auto psPath = hlslDir / "MaterialPS.hlsl";

			{
				std::ofstream file(vsPath, std::ios::binary | std::ios::trunc);
				file.write(candidateVs.data(), static_cast<std::streamsize>(candidateVs.size()));
			}
			{
				std::ofstream file(psPath, std::ios::binary | std::ios::trunc);
				file.write(candidatePs.data(), static_cast<std::streamsize>(candidatePs.size()));
			}

			auto vsResult = ShaderCompiler::Compile(vsPath, EShaderType::Vertex, Engine::ERHIType::DirectX12, "main");
			if (!vsResult.Success || vsResult.Bytecode.empty())
			{
				build.MaterialCurrent = false;
				build.StatusKey = "MaterialEditor.CompileFailed";
				build.Details = "[VS Compile Error] " + vsResult.ErrorMessage;
				return;
			}

			auto psResult = ShaderCompiler::Compile(psPath, EShaderType::Fragment, Engine::ERHIType::DirectX12, "main");
			build.Details = psResult.ErrorMessage;

			if (!psResult.Success || psResult.Bytecode.empty())
			{
				build.MaterialCurrent = false;
				build.StatusKey = "MaterialEditor.CompileFailed";
				build.Details = "[PS Compile Error] " + psResult.ErrorMessage;
				return;
			}

			build.MaterialLastGood = std::move(psResult);
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

		// 1. VS 및 PS HLSL 소스 코드 동시 생성
		std::string candidateVsHlsl;
		auto diagVs = Engine::ShaderGraphCompiler::GenerateGBufferMaterialVS(m_ShaderGraph.Data, m_GraphRegistry, candidateVsHlsl);
		if (!diagVs.Succeeded())
		{
			build.StatusKey = "MaterialEditor.CompileFailed";
			build.Details = diagVs.Message;
			return;
		}

		std::string candidatePsHlsl;
		auto diagPs = Engine::ShaderGraphCompiler::GenerateGBufferMaterialHlsl(m_ShaderGraph.Data, m_GraphRegistry, candidatePsHlsl);
		if (!diagPs.Succeeded())
		{
			build.StatusKey = "MaterialEditor.CompileFailed";
			build.Details = diagPs.Message;
			return;
		}

		try
		{
			const auto hlslDir = ReserveMaterialShaderDirectory();
			const auto vsPath = hlslDir / "MaterialVS.hlsl";
			const auto psPath = hlslDir / "MaterialPS.hlsl";

			// 파일 쓰기
			{
				std::ofstream file(vsPath, std::ios::binary | std::ios::trunc);
				file.write(candidateVsHlsl.data(), static_cast<std::streamsize>(candidateVsHlsl.size()));
			}
			{
				std::ofstream file(psPath, std::ios::binary | std::ios::trunc);
				file.write(candidatePsHlsl.data(), static_cast<std::streamsize>(candidatePsHlsl.size()));
			}

			const auto rhiType = Engine::Renderer::IsCreated() ? Engine::Renderer::Get().GetRHIType() : Engine::ERHIType::DirectX12;

			// 2. Vertex Shader 컴파일
			auto vsResult = ShaderCompiler::Compile(vsPath, EShaderType::Vertex, rhiType, "main");
			if (!vsResult.Success || vsResult.Bytecode.empty())
			{
				build.StatusKey = "MaterialEditor.CompileFailed";
				build.Details = "[VS Compile Error] " + vsResult.ErrorMessage;
				return;
			}

			// 3. Pixel Shader 컴파일
			auto psResult = ShaderCompiler::Compile(psPath, EShaderType::Fragment, rhiType, "main");
			if (!psResult.Success || psResult.Bytecode.empty())
			{
				build.StatusKey = "MaterialEditor.CompileFailed";
				build.Details = "[PS Compile Error] " + psResult.ErrorMessage;
				return;
			}

			build.MaterialLastGood = psResult;
			build.MaterialCurrent = true;
			build.StatusKey = "MaterialEditor.CompilePassed";
			build.Details.clear();

			// 4. 런타임 셰이더 리소스 갱신 및 머티리얼 바인딩
			auto& rm = Engine::ResourceManager::Get();
			std::string baseName = m_TargetResourcePath.stem().string();
			const std::string bammat = ".bammat";
			if (baseName.size() > bammat.size() && baseName.ends_with(bammat))
				baseName = baseName.substr(0, baseName.size() - bammat.size());

			const std::wstring vsKey = L"LiveShader/" + StrToWStr(baseName) + L"_VS";
			const std::wstring psKey = L"LiveShader/" + StrToWStr(baseName) + L"_PS";

			// VS 핸들 생성 또는 바이트코드 갱신
			auto vsHandle = rm.GetResourceHandle<Engine::Shader>(vsKey);
			if (vsHandle.IsValid() && vsHandle.Get())
			{
				vsHandle->UpdateBytecode(vsResult.Bytecode);
			}
			else
			{
				ShaderDesc desc{};
				desc.Key = vsKey;
				desc.Path = vsResult.OutputPath.wstring();
				if (rhiType == Engine::ERHIType::DirectX12) desc.dxilPath = vsResult.OutputPath.wstring();
				else if (rhiType == Engine::ERHIType::WebGPU) desc.wgslPath = vsResult.OutputPath.wstring();
				else desc.spirvPath = vsResult.OutputPath.wstring();
				desc.shaderType = EShaderType::Vertex;
				desc.entryPoint = "main";
				auto* newShader = Engine::Shader::Create(&desc);
				if (newShader) vsHandle = rm.AddResource<Engine::Shader>(vsKey, newShader);
			}

			// PS 핸들 생성 또는 바이트코드 갱신
			auto psHandle = rm.GetResourceHandle<Engine::Shader>(psKey);
			if (psHandle.IsValid() && psHandle.Get())
			{
				psHandle->UpdateBytecode(psResult.Bytecode);
			}
			else
			{
				ShaderDesc desc{};
				desc.Key = psKey;
				desc.Path = psResult.OutputPath.wstring();
				if (rhiType == Engine::ERHIType::DirectX12) desc.dxilPath = psResult.OutputPath.wstring();
				else if (rhiType == Engine::ERHIType::WebGPU) desc.wgslPath = psResult.OutputPath.wstring();
				else desc.spirvPath = psResult.OutputPath.wstring();
				desc.shaderType = EShaderType::Fragment;
				desc.entryPoint = "main";
				auto* newShader = Engine::Shader::Create(&desc);
				if (newShader) psHandle = rm.AddResource<Engine::Shader>(psKey, newShader);
			}

			if (vsHandle.IsValid() && psHandle.IsValid())
			{
				m_LiveVertexShader = vsHandle;
				m_LivePixelShader = psHandle;

				m_Material->SetVertexShaderHandle(m_LiveVertexShader);
				m_Material->SetPixelShaderHandle(m_LivePixelShader);
				m_Material->SetPassShader(Engine::ERenderPassType::Geometry, m_LiveVertexShader, m_LivePixelShader);

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
