#include "StartupResourceLoader.h"
#include "ResourceManager.h"
#include "Shader.h"
#include "Renderer.h"
#include "Logger.h"
#include <glaze/glaze.hpp>
#include <fstream>
#include <sstream>
#include <string_view>
#include <vector>

namespace Editor::StartupConfig
{
	struct PipelineStepConfig
	{
		int Step = 0;
		std::string Category;
		std::string Config;
	};

	struct StartupManifestConfig
	{
		std::vector<PipelineStepConfig> Pipeline;
	};

	struct TextureManifestConfig
	{
		std::vector<std::string> Textures;
	};

	struct ShaderItemConfig
	{
		std::string Key;
		std::string DxilPath;
		std::string SpirvPath;
		std::string Type = "Vertex";
		std::string EntryPoint = "main";
		uint32_t Samplers = 0;
		uint32_t StorageBuffers = 0;
		uint32_t UniformBuffers = 0;
	};

	struct ShaderManifestConfig
	{
		std::vector<ShaderItemConfig> Shaders;
	};

	struct MeshManifestConfig
	{
		std::vector<std::string> Meshes;
	};

	struct MaterialManifestConfig
	{
		std::vector<std::string> Materials;
	};

	struct MaterialInstanceManifestConfig
	{
		std::vector<std::string> MaterialInstances;
	};

	static bool ReadJsonFile(const std::filesystem::path& path, std::string& outContent)
	{
		std::ifstream file(path);
		if (!file.is_open()) return false;
		std::stringstream buffer;
		buffer << file.rdbuf();
		outContent = buffer.str();
		return true;
	}

	static bool LoadFiles(const std::vector<std::string>& paths, std::string_view category)
	{
		bool success = true;
		auto& resourceManager = Engine::ResourceManager::Get();

		for (const auto& path : paths)
		{
			if (!resourceManager.LoadFile(std::filesystem::path(path).wstring()).IsValid())
			{
				EDITOR_LOG_ERROR("[StartupLoader] Failed to load {} resource: {}", category, path);
				success = false;
			}
		}

		return success;
	}

	template<typename T>
	static bool ReadManifest(const std::filesystem::path& path, std::string_view category, T& manifest)
	{
		std::string content;
		if (!ReadJsonFile(path, content))
		{
			EDITOR_LOG_ERROR("[StartupLoader] Failed to read {} manifest: {}", category, path.string());
			return false;
		}

		if (glz::read_json(manifest, content))
		{
			EDITOR_LOG_ERROR("[StartupLoader] Failed to parse {} manifest: {}", category, path.string());
			return false;
		}

		return true;
	}
}

using namespace Editor::StartupConfig;

bool Editor::StartupResourceLoader::LoadPipeline(const std::filesystem::path& manifestPath)
{
	StartupManifestConfig manifest;
	if (!ReadManifest(manifestPath, "pipeline", manifest))
		return false;

	EDITOR_LOG_INFO("[StartupLoader] Starting resource preload pipeline...");

	bool success = true;
	for (const auto& step : manifest.Pipeline)
	{
		const std::filesystem::path configPath = step.Config;
		bool stepSuccess = false;

		if (!std::filesystem::exists(configPath))
		{
			EDITOR_LOG_ERROR("[StartupLoader] Config file not found for {}: {}", step.Category, step.Config);
			success = false;
			continue;
		}

		if (step.Category == "Textures") stepSuccess = LoadTextures(configPath);
		else if (step.Category == "Shaders") stepSuccess = LoadShaders(configPath);
		else if (step.Category == "Meshes") stepSuccess = LoadMeshes(configPath);
		else if (step.Category == "Materials") stepSuccess = LoadMaterials(configPath);
		else if (step.Category == "MaterialInstances") stepSuccess = LoadMaterialInstances(configPath);
		else
		{
			EDITOR_LOG_ERROR("[StartupLoader] Unknown pipeline category: {}", step.Category);
			success = false;
			continue;
		}

		if (!stepSuccess)
		{
			EDITOR_LOG_ERROR("[StartupLoader] Pipeline step failed: category={}, config={}",
				step.Category, step.Config);
			success = false;
		}
	}

	if (success)
		EDITOR_LOG_INFO("[StartupLoader] Preload pipeline completed successfully.");
	else
		EDITOR_LOG_ERROR("[StartupLoader] Resource preload pipeline failed.");

	return success;
}

bool Editor::StartupResourceLoader::LoadTextures(const std::filesystem::path& configPath)
{
	TextureManifestConfig manifest;
	if (!ReadManifest(configPath, "Textures", manifest))
		return false;

	return LoadFiles(manifest.Textures, "Textures");
}

bool Editor::StartupResourceLoader::LoadShaders(const std::filesystem::path& configPath)
{
	ShaderManifestConfig manifest;
	if (!ReadManifest(configPath, "Shaders", manifest))
		return false;

	const bool isSdlGpu = Engine::Renderer::IsCreated() &&
		Engine::Renderer::Get().GetRHIType() == Engine::ERHIType::SDLGPU;
	auto& resourceManager = Engine::ResourceManager::Get();
	bool success = true;

	for (const auto& item : manifest.Shaders)
	{
		ShaderDesc desc = {};
		desc.Key = std::filesystem::path(item.Key).wstring();

		std::string path = isSdlGpu ? item.SpirvPath : item.DxilPath;
		if (path.empty()) path = item.DxilPath;
		desc.Path = std::filesystem::path(path).wstring();
		desc.shaderType = (item.Type == "Fragment" || item.Type == "Pixel")
			? EShaderType::Fragment : EShaderType::Vertex;
		desc.entryPoint = item.EntryPoint.empty() ? "main" : item.EntryPoint;
		desc.numSamplers = item.Samplers;
		desc.numStorageBuffers = item.StorageBuffers;
		desc.numUniformBuffers = item.UniformBuffers;

		if (!resourceManager.LoadResource<Engine::Shader>(&desc))
		{
			EDITOR_LOG_ERROR("[StartupLoader] Failed to load shader: key={}, path={}", item.Key, path);
			success = false;
		}
	}

	return success;
}

bool Editor::StartupResourceLoader::LoadMeshes(const std::filesystem::path& configPath)
{
	MeshManifestConfig manifest;
	if (!ReadManifest(configPath, "Meshes", manifest))
		return false;

	return LoadFiles(manifest.Meshes, "Meshes");
}

bool Editor::StartupResourceLoader::LoadMaterials(const std::filesystem::path& configPath)
{
	MaterialManifestConfig manifest;
	if (!ReadManifest(configPath, "Materials", manifest))
		return false;

	return LoadFiles(manifest.Materials, "Materials");
}

bool Editor::StartupResourceLoader::LoadMaterialInstances(const std::filesystem::path& configPath)
{
	MaterialInstanceManifestConfig manifest;
	if (!ReadManifest(configPath, "MaterialInstances", manifest))
		return false;

	return LoadFiles(manifest.MaterialInstances, "MaterialInstances");
}