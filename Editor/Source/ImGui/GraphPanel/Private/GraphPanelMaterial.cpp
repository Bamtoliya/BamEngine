#include "GraphPanel.h"
#include "Renderer.h"
#include "ResourceManager.h"
#include "RenderPass.h"
#include "Mesh.h"
#include "Texture.h"
#include "SamplerManager.h"
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace
{
	struct GraphMaterialResourceDeleter
	{
		void operator()(Engine::Resource* resource) const
		{
			if (!resource) return;
			resource->Free();
			delete resource;
		}
	};

	struct GraphMaterialRegistrationGuard
	{
		Engine::ResourceHandle<Engine::Shader> PixelHandle;
		Engine::ResourceHandle<Engine::Material> MaterialHandle;
		bool Committed = false;

		~GraphMaterialRegistrationGuard()
		{
			if (Committed || !Engine::ResourceManager::IsCreated()) return;

			auto& manager = Engine::ResourceManager::Get();
			if (MaterialHandle) manager.DestroyResource(MaterialHandle.GetRawHandle());
			if (PixelHandle) manager.DestroyResource(PixelHandle.GetRawHandle());
		}
	};
}

void GraphPanel::CreateRuntimeMaterial()
{
	using namespace Engine;

	if (m_Domain != EGraphDomain::Shader) return;
	auto& build = m_Shader.ShaderBuild;

	try
	{
		if (!Renderer::IsCreated() || !ResourceManager::IsCreated() || !SamplerManager::IsCreated())
			throw std::runtime_error("GraphPanel.RuntimeMaterialUnavailable");

		auto& renderer = Renderer::Get();
		if (!renderer.GetRHI() || renderer.GetRHIType() != ERHIType::DirectX12)
			throw std::runtime_error("GraphPanel.RuntimeMaterialUnavailable");

		if (!build.MaterialCurrent) CompileMaterialShader();
		if (!build.MaterialCurrent || !build.MaterialLastGood.Success) return;

		if (build.RuntimeMaterial && build.RuntimeMaterialArtifact == build.MaterialLastGood.OutputPath)
		{
			build.StatusKey = "GraphPanel.RuntimeMaterialReady";
			build.Details.clear();
			return;
		}

		auto& manager = ResourceManager::Get();
		auto mesh = manager.GetResourceHandle<Mesh>(L"CubeMesh");
		auto vertex = manager.GetResourceHandle<Shader>(RESOURCE_PATH L"Shader/GBufferVS");
		auto shadowVertex = manager.GetResourceHandle<Shader>(RESOURCE_PATH L"Shader/ShadowDepthVS");
		auto shadowPixel = manager.GetResourceHandle<Shader>(RESOURCE_PATH L"Shader/ShadowDepthPS");
		auto white = manager.GetResourceHandle<Texture>(RESOURCE_PATH L"Texture/white1x1.png");

		if (!mesh || !vertex || !shadowVertex || !shadowPixel || !white ||
			!vertex->GetRHIShader() || !shadowVertex->GetRHIShader() || !shadowPixel->GetRHIShader() ||
			!white->GetRHITexture() || !SamplerManager::Get().GetDefaultSampler())
		{
			throw std::runtime_error("GraphPanel.RuntimeMaterialDependencies");
		}

		std::vector<RenderPass*> passes;
		bool hasGeometry = false;

		for (const auto& viewport : renderer.GetActiveViewportCameras())
		{
			auto* pass = viewport.renderPass;
			if (!pass) continue;

			const bool geometry = viewport.hasCamera &&
				pass->GetPassType() == ERenderPassType::Geometry && pass->GetRenderTargetCount() == 5;

			if (geometry || pass->GetPassType() == ERenderPassType::Shadow)
				passes.push_back(pass);

			hasGeometry |= geometry;
		}

		if (!hasGeometry)
			throw std::runtime_error("GraphPanel.RuntimeMaterialPassMissing");

		auto artifact = build.MaterialLastGood.OutputPath;
		const auto revision = artifact.parent_path().parent_path().parent_path().filename().wstring();
		if (artifact.empty() || revision.empty())
			throw std::runtime_error("GraphPanel.RuntimeMaterialResourceFailed");

		const auto pixelKey = L"GraphRuntime/Shader/" + revision;
		const auto materialKey = L"GraphRuntime/Material/" + revision;

		if (manager.FindHandleByKey(pixelKey).IsValid() || manager.FindHandleByKey(materialKey).IsValid())
			throw std::runtime_error("GraphPanel.RuntimeMaterialCollision");

		std::string readyStatus = "GraphPanel.RuntimeMaterialReady";
		GraphMaterialRegistrationGuard registration;

		ShaderDesc pixelDesc{};
		pixelDesc.Key = pixelKey;
		pixelDesc.Path = artifact.wstring();
		pixelDesc.shaderType = EShaderType::Fragment;
		pixelDesc.entryPoint = "main";

		std::unique_ptr<Shader, GraphMaterialResourceDeleter> pixel(Shader::Create(&pixelDesc));
		if (!pixel || !pixel->GetRHIShader() ||
			pixel->GetRHIShader()->GetRawShaderBytecode() != build.MaterialLastGood.Bytecode)
		{
			throw std::runtime_error("GraphPanel.RuntimeMaterialResourceFailed");
		}

		const auto* expectedPixel = pixel.get();
		auto pixelHandle = manager.AddResource<Shader>(pixelKey, pixel.get());
		pixel.release();

		if (manager.GetResource(pixelHandle.GetRawHandle()) != expectedPixel)
			throw std::runtime_error("GraphPanel.RuntimeMaterialCollision");

		registration.PixelHandle = std::move(pixelHandle);

		MaterialDesc materialDesc{};
		materialDesc.Key = materialKey;
		materialDesc.VertexShaderHandle = vertex;
		materialDesc.PixelShaderHandle = registration.PixelHandle;
		materialDesc.BlendMode = EBlendMode::Opaque;
		materialDesc.CullMode = ECullMode::Back;
		materialDesc.DepthMode = EDepthMode::ReadWrite;
		materialDesc.PassShaders.push_back({ ERenderPassType::Shadow, shadowVertex, shadowPixel });

		std::unique_ptr<Material, GraphMaterialResourceDeleter> material(Material::Create(&materialDesc));
		if (!material)
			throw std::runtime_error("GraphPanel.RuntimeMaterialResourceFailed");

		material->SetTextureBinding("Diffuse", 0, white);

		for (auto* pass : passes)
		{
			if (IsFailure(renderer.ValidateMeshMaterial(mesh.Get(), material.get(), pass)))
				throw std::runtime_error("GraphPanel.RuntimeMaterialPipelineFailed");
		}

		const auto* expectedMaterial = material.get();
		auto materialHandle = manager.AddResource<Material>(materialKey, material.get());
		material.release();

		if (manager.GetResource(materialHandle.GetRawHandle()) != expectedMaterial)
			throw std::runtime_error("GraphPanel.RuntimeMaterialCollision");

		registration.MaterialHandle = std::move(materialHandle);

		build.RuntimeMaterial = std::move(registration.MaterialHandle);
		build.RuntimeMaterialArtifact.swap(artifact);
		build.StatusKey.swap(readyStatus);
		build.Details.clear();
		registration.Committed = true;
	}
	catch (const std::exception& error)
	{
		build.StatusKey = "GraphPanel.RuntimeMaterialFailed";
		build.Details = error.what();
	}
}

void GraphPanel::DrawRuntimeMaterialDetails()
{
	auto& build = m_Shader.ShaderBuild;
	if (!build.RuntimeMaterial) return;

	ImGui::TextWrapped("%s", LOCAL_CSTR("GraphPanel.RuntimeMaterialScope"));

	const auto key = WStrToStr(build.RuntimeMaterial->GetKey());
	const auto text = LOCAL_FMT("GraphPanel.RuntimeMaterialKey", key);
	ImGui::TextWrapped("%s", text.c_str());

	if (!build.MaterialCurrent || build.RuntimeMaterialArtifact != build.MaterialLastGood.OutputPath)
		ImGui::TextWrapped("%s", LOCAL_CSTR("GraphPanel.RuntimeMaterialOld"));
}