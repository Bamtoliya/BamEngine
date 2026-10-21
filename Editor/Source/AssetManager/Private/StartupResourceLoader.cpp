#include "StartupResourceLoader.h"
#include "ResourceManager.h"
#include "Shader.h"
#include "Renderer.h"
#include "Logger.h"
#include <glaze/glaze.hpp>
#include <fstream>
#include <sstream>

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
}

using namespace Editor::StartupConfig;

bool Editor::StartupResourceLoader::LoadPipeline(const std::filesystem::path& manifestPath)
{
	std::string content;
	if (!ReadJsonFile(manifestPath, content))
	{
		ENGINE_LOG_ERROR("[StartupLoader] Failed to open manifest: {}", manifestPath.string());
		return false;
	}

	StartupManifestConfig manifest;
	if (glz::read_json(manifest, content))
	{
		ENGINE_LOG_ERROR("[StartupLoader] Manifest JSON parse error: {}", manifestPath.string());
		return false;
	}

	ENGINE_LOG_INFO("[StartupLoader] Starting resource preload pipeline...");

	for (const auto& step : manifest.Pipeline)
	{
		std::filesystem::path configPath = step.Config;
		if (!std::filesystem::exists(configPath))
		{
			ENGINE_LOG_WARN("[StartupLoader] Config file not found for {}: {}", step.Category, step.Config);
			continue;
		}

		if (step.Category == "Textures") LoadTextures(configPath);
		else if (step.Category == "Shaders") LoadShaders(configPath);
		else if (step.Category == "Meshes") LoadMeshes(configPath);
		else if (step.Category == "Materials") LoadMaterials(configPath);
		else if (step.Category == "MaterialInstances") LoadMaterialInstances(configPath);
	}

	ENGINE_LOG_INFO("[StartupLoader] Preload pipeline completed successfully.");
	return true;
}

bool Editor::StartupResourceLoader::LoadTextures(const std::filesystem::path& configPath)
{
	std::string content;
	if (!ReadJsonFile(configPath, content)) return false;

	TextureManifestConfig manifest;
	if (glz::read_json(manifest, content)) return false;

	auto& rm = Engine::ResourceManager::Get();
	for (const auto& pathStr : manifest.Textures)
	{
		rm.LoadFile(std::filesystem::path(pathStr).wstring());
	}
	return true;
}

bool Editor::StartupResourceLoader::LoadShaders(const std::filesystem::path& configPath)
{
	std::string content;
	if (!ReadJsonFile(configPath, content)) return false;

	ShaderManifestConfig manifest;
	if (glz::read_json(manifest, content)) return false;

	const bool isSdlGpu = Engine::Renderer::IsCreated() &&
		Engine::Renderer::Get().GetRHIType() == Engine::ERHIType::SDLGPU;

	auto& rm = Engine::ResourceManager::Get();

	for (const auto& item : manifest.Shaders)
	{
		ShaderDesc desc = {};
		desc.Key = std::filesystem::path(item.Key).wstring();

		std::string pathStr = isSdlGpu ? item.SpirvPath : item.DxilPath;
		if (pathStr.empty()) pathStr = item.DxilPath;
		desc.Path = std::filesystem::path(pathStr).wstring();

		desc.shaderType = (item.Type == "Fragment" || item.Type == "Pixel")
			? EShaderType::Fragment : EShaderType::Vertex;

		desc.entryPoint = item.EntryPoint.empty() ? "main" : item.EntryPoint;
		desc.numSamplers = item.Samplers;
		desc.numStorageBuffers = item.StorageBuffers;
		desc.numUniformBuffers = item.UniformBuffers;

		rm.LoadResource<Engine::Shader>(&desc);
	}
	return true;
}

bool Editor::StartupResourceLoader::LoadMeshes(const std::filesystem::path& configPath)
{
	std::string content;
	if (!ReadJsonFile(configPath, content)) return false;

	MeshManifestConfig manifest;
	if (glz::read_json(manifest, content)) return false;

	auto& rm = Engine::ResourceManager::Get();
	for (const auto& pathStr : manifest.Meshes)
	{
		rm.LoadFile(std::filesystem::path(pathStr).wstring());
	}
	return true;
}

bool Editor::StartupResourceLoader::LoadMaterials(const std::filesystem::path& configPath)
{
	std::string content;
	if (!ReadJsonFile(configPath, content)) return false;

	MaterialManifestConfig manifest;
	if (glz::read_json(manifest, content)) return false;

	auto& rm = Engine::ResourceManager::Get();
	for (const auto& pathStr : manifest.Materials)
	{
		rm.LoadFile(std::filesystem::path(pathStr).wstring());
	}
	return true;
}

bool Editor::StartupResourceLoader::LoadMaterialInstances(const std::filesystem::path& configPath)
{
	std::string content;
	if (!ReadJsonFile(configPath, content)) return false;

	MaterialInstanceManifestConfig manifest;
	if (glz::read_json(manifest, content)) return false;

	auto& rm = Engine::ResourceManager::Get();
	for (const auto& pathStr : manifest.MaterialInstances)
	{
		rm.LoadFile(std::filesystem::path(pathStr).wstring());
	}
	return true;
}