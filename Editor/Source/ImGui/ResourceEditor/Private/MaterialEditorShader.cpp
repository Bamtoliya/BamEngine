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
#include <algorithm>

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
	bool MaterialEditor::SupportsGraphPass(Engine::ERenderPassType pass)
	{
		return pass == Engine::ERenderPassType::Geometry || pass == Engine::ERenderPassType::ForwardTransparent;
	}

	bool MaterialEditor::IsGraphShader(const Engine::ResourceHandle<Engine::Shader>& handle)
	{
		const auto* shader = handle.Get();
		return shader && (shader->GetKey().starts_with(L"LiveShader/") ||
			Engine::PlatformPath::ToProjectRelative(Engine::PlatformPath::ResolveUri(shader->GetPath()))
				.generic_wstring().starts_with(L"Temp/MaterialShader/"));
	}

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
			build.Details = LocalizeGraphMessage(m_LastGraphDiagnostic.Message);
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
		const auto primaryPass = m_Material ? m_Material->GetPrimaryPassType() : Engine::ERenderPassType::Geometry;
		if (!SupportsGraphPass(primaryPass))
		{
			build.MaterialCurrent = false;
			build.StatusKey = "MaterialEditor.GraphPassUnsupported";
			build.Details = LOCAL("MaterialEditor.GraphPassUnsupported");
			return;
		}
		std::string candidateVs;
		std::string candidatePs;

		m_LastGraphDiagnostic = Engine::ShaderGraphCompiler::GenerateGBufferMaterialVS(m_ShaderGraph.Data, m_GraphRegistry, candidateVs);
		if (!m_LastGraphDiagnostic.Succeeded())
		{
			build.MaterialCurrent = false;
			build.StatusKey = "MaterialEditor.CompileFailed";
			build.Details = LocalizeGraphMessage(m_LastGraphDiagnostic.Message);
			return;
		}

		m_LastGraphDiagnostic = (primaryPass == Engine::ERenderPassType::ForwardTransparent)
			? Engine::ShaderGraphCompiler::GenerateForwardTransparentMaterialHlsl(m_ShaderGraph.Data, m_GraphRegistry, candidatePs)
			: Engine::ShaderGraphCompiler::GenerateGBufferMaterialHlsl(m_ShaderGraph.Data, m_GraphRegistry, candidatePs);
		build.Details.clear();

		if (!m_LastGraphDiagnostic.Succeeded())
		{
			build.MaterialCurrent = false;
			build.StatusKey = "MaterialEditor.CompileFailed";
			build.Details = LocalizeGraphMessage(m_LastGraphDiagnostic.Message);
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
		if (!CanEditShaderGraph() || !m_GraphReady || !HasLinkedShaderGraph()) return;

		auto& build = m_ShaderGraph.ShaderBuild;

		build.MaterialCurrent = false;
		const auto primaryPass = m_Material->GetPrimaryPassType();
		if (!SupportsGraphPass(primaryPass))
		{
			build.StatusKey = "MaterialEditor.GraphPassUnsupported";
			build.Details = LOCAL("MaterialEditor.GraphPassUnsupported");
			return;
		}

		// 1. VS 및 PS HLSL 소스 코드 동시 생성
		std::string candidateVsHlsl;
		auto diagVs = Engine::ShaderGraphCompiler::GenerateGBufferMaterialVS(m_ShaderGraph.Data, m_GraphRegistry, candidateVsHlsl);
		if (!diagVs.Succeeded())
		{
			build.StatusKey = "MaterialEditor.CompileFailed";
			build.Details = LocalizeGraphMessage(diagVs.Message);
			return;
		}

		std::string candidatePsHlsl;
		auto diagPs = (primaryPass == Engine::ERenderPassType::ForwardTransparent)
			? Engine::ShaderGraphCompiler::GenerateForwardTransparentMaterialHlsl(m_ShaderGraph.Data, m_GraphRegistry, candidatePsHlsl)
			: Engine::ShaderGraphCompiler::GenerateGBufferMaterialHlsl(m_ShaderGraph.Data, m_GraphRegistry, candidatePsHlsl);
		if (!diagPs.Succeeded())
		{
			build.StatusKey = "MaterialEditor.CompileFailed";
			build.Details = LocalizeGraphMessage(diagPs.Message);
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

			// 새 셰이더 쌍을 준비한 뒤에만 현재 바인딩을 교체한다.
			auto& rm = Engine::ResourceManager::Get();
			const auto asset = Engine::PlatformPath::ToVirtualUri(m_TargetResourcePath);
			const auto key = L"LiveShader/" + (asset.empty() ? m_TargetResourcePath.generic_wstring() : asset) +
				L"/" + std::to_wstring(static_cast<unsigned>(primaryPass)) +
				L"/" + std::to_wstring(static_cast<unsigned>(rhiType)) + L"/" + hlslDir.parent_path().filename().wstring();

			const auto create = [&](const auto& result, EShaderType stage, const wchar_t* suffix)
				-> Engine::ResourceHandle<Engine::Shader>
			{
				ShaderDesc desc{};
				desc.Key = key + suffix;
				desc.Path = result.OutputPath.wstring();
				if (rhiType == Engine::ERHIType::DirectX12) desc.dxilPath = desc.Path;
				else if (rhiType == Engine::ERHIType::WebGPU) desc.wgslPath = desc.Path;
				else desc.spirvPath = desc.Path;
				desc.shaderType = stage;
				desc.entryPoint = "main";
				auto* shader = Engine::Shader::Create(&desc);
				return shader ? rm.AddResource<Engine::Shader>(desc.Key, shader) : Engine::ResourceHandle<Engine::Shader>{};
			};
			auto layout = Engine::ShaderGraphCompiler::BuildMaterialBufferLayout(m_ShaderGraph.Data);
			const auto& previousLayout = m_Material->GetBufferLayout();
			const bool layoutChanged = layout.TotalBufferSize != previousLayout.TotalBufferSize ||
				!std::ranges::equal(layout.Entries, previousLayout.Entries,
					[](const auto& a, const auto& b)
					{
						return a.Name == b.Name && a.Type == b.Type && a.Offset == b.Offset && a.Size == b.Size;
					});
			std::vector<Engine::ERenderPassType> stalePasses;
			if (layoutChanged)
			{
				for (const auto& pass : m_Material->GetPassShaders())
					if (pass.passType != primaryPass && IsGraphShader(pass.vertexShader) && IsGraphShader(pass.pixelShader))
						stalePasses.push_back(pass.passType);
			}

			auto vs = create(vsResult, EShaderType::Vertex, L"/VS");
			auto ps = vs.IsValid() ? create(psResult, EShaderType::Fragment, L"/PS") : Engine::ResourceHandle<Engine::Shader>{};
			if (!vs.IsValid() || !ps.IsValid())
			{
				if (vs.IsValid()) (void)rm.DestroyResource(vs.GetRawHandle());
				build.StatusKey = "MaterialEditor.CompileFailed";
				build.Details = LOCAL("MaterialEditor.GraphShaderPublishFailed");
				return;
			}

			m_Material->SetPassShader(primaryPass, vs, ps);
			for (const auto pass : stalePasses)
				m_Material->RemovePassShader(pass);
			m_Material->SetBufferLayout(std::move(layout));
			auto texDescriptors = Engine::ShaderGraphCompiler::CollectTextureDescriptors(m_ShaderGraph.Data);
			m_Material->SyncTextureBindings(texDescriptors);
			m_Material->SetVertexShaderHandle(vs);
			m_Material->SetPixelShaderHandle(ps);
			m_LiveVertexShader = std::move(vs);
			m_LivePixelShader = std::move(ps);
			m_Material->IncreaseVersion();
			m_PreviewDirtyTimer = 0.5f;

			build.MaterialLastGood = std::move(psResult);
			build.MaterialCurrent = true;
			build.StatusKey = "MaterialEditor.CompilePassed";
			build.Details.clear();
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
