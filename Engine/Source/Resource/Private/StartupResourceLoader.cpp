#include "StartupResourceLoader.h"

#include "Logger.h"
#include "PlatformPath.h"
#include "Renderer.h"
#include "ResourceManager.h"
#include "Shader.h"

#include <glaze/glaze.hpp>

namespace Engine::StartupResourceLoaderDetail
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

	bool ReportStartupError(std::string& error, std::string message)
	{
		error = std::move(message);
		BAM_LOG(Error, "Engine", "StartupResourceLoader", "{}", error);
		return false;
	}

	bool ReadStartupJsonFile(const std::filesystem::path& path, std::string& outContent)
	{
		std::ifstream file(path);
		if (!file.is_open())
			return false;

		std::stringstream buffer;
		buffer << file.rdbuf();
		outContent = buffer.str();
		return true;
	}

	template<typename T>
	bool ReadStartupManifest(const std::filesystem::path& path, std::string_view category, T& manifest, std::string& error)
	{
		const auto resolved = PlatformPath::ResolveUri(path);
		std::string content;
		if (!ReadStartupJsonFile(resolved, content))
			return ReportStartupError(error, "Could not read " + std::string(category) + " manifest: " + resolved.string());

		if (glz::read_json(manifest, content))
			return ReportStartupError(error, "Could not parse " + std::string(category) + " manifest: " + resolved.string());

		return true;
	}

	std::filesystem::path ResolveStartupUri(const std::string& uri)
	{
		return PlatformPath::ResolveUri(Engine::Utf8ToPath(uri).wstring());
	}

	bool AppendStartupFile(const std::filesystem::path& path, std::string_view category,
		std::vector<std::filesystem::path>& files, std::string& error)
	{
		if (!std::filesystem::is_regular_file(path))
			return ReportStartupError(error, "Missing " + std::string(category) + " startup resource: " + path.string());

		const auto normalized = path.lexically_normal();
		if (std::ranges::find(files, normalized) == files.end())
			files.push_back(normalized);
		return true;
	}

	bool AppendStartupUris(const std::vector<std::string>& uris, std::string_view category,
		std::vector<std::filesystem::path>& files, std::string& error)
	{
		for (const auto& uri : uris)
		{
			if (uri.empty())
				return ReportStartupError(error, "Empty path in " + std::string(category) + " startup manifest.");
			if (!AppendStartupFile(ResolveStartupUri(uri), category, files, error))
				return false;
		}
		return true;
	}

	bool TryParseStartupShaderType(const std::string& type, EShaderType& shaderType)
	{
		if (type == "Vertex")
			shaderType = EShaderType::Vertex;
		else if (type == "Fragment" || type == "Pixel")
			shaderType = EShaderType::Fragment;
		else if (type == "Compute")
			shaderType = EShaderType::Compute;
		else
			return false;
		return true;
	}

	const std::string& GetStartupShaderPath(const ShaderItemConfig& item, ERHIType rhiType)
	{
		if (rhiType == ERHIType::SDLGPU && !item.SpirvPath.empty())
			return item.SpirvPath;
		return item.DxilPath;
	}

	bool CollectStartupCategoryFiles(const PipelineStepConfig& step, ERHIType rhiType,
		std::vector<std::filesystem::path>& files, std::string& error)
	{
		if (step.Config.empty())
			return ReportStartupError(error, "Empty config path for startup pipeline category: " + step.Category);

		const auto configPath = ResolveStartupUri(step.Config);
		if (!AppendStartupFile(configPath, step.Category + " config", files, error))
			return false;

		if (step.Category == "Textures")
		{
			TextureManifestConfig manifest;
			return ReadStartupManifest(configPath, step.Category, manifest, error) &&
				AppendStartupUris(manifest.Textures, step.Category, files, error);
		}
		if (step.Category == "Shaders")
		{
			ShaderManifestConfig manifest;
			if (!ReadStartupManifest(configPath, step.Category, manifest, error))
				return false;

			for (const auto& item : manifest.Shaders)
			{
				EShaderType shaderType = EShaderType::Unknown;
				if (!TryParseStartupShaderType(item.Type, shaderType))
					return ReportStartupError(error, "Unsupported shader type '" + item.Type + "' for key: " + item.Key);
				const auto& shaderPath = GetStartupShaderPath(item, rhiType);
				if (shaderPath.empty())
					return ReportStartupError(error, "No shader binary path for key: " + item.Key);
				if (!AppendStartupFile(ResolveStartupUri(shaderPath), "shader", files, error))
					return false;
			}
			return true;
		}
		if (step.Category == "Meshes")
		{
			MeshManifestConfig manifest;
			return ReadStartupManifest(configPath, step.Category, manifest, error) &&
				AppendStartupUris(manifest.Meshes, step.Category, files, error);
		}
		if (step.Category == "Materials")
		{
			MaterialManifestConfig manifest;
			return ReadStartupManifest(configPath, step.Category, manifest, error) &&
				AppendStartupUris(manifest.Materials, step.Category, files, error);
		}
		if (step.Category == "MaterialInstances")
		{
			MaterialInstanceManifestConfig manifest;
			return ReadStartupManifest(configPath, step.Category, manifest, error) &&
				AppendStartupUris(manifest.MaterialInstances, step.Category, files, error);
		}

		return ReportStartupError(error, "Unknown startup pipeline category: " + step.Category);
	}

	bool LoadStartupFiles(const std::vector<std::string>& paths, std::string_view category)
	{
		bool success = true;
		auto& resourceManager = ResourceManager::Get();

		for (const auto& path : paths)
		{
			const auto fullPath = ResolveStartupUri(path);
			if (!resourceManager.LoadFile(fullPath.wstring()).IsValid())
			{
				BAM_LOG(Error, "Engine", "StartupResourceLoader", "Failed to load {} resource: {}", category, path);
				success = false;
			}
		}

		return success;
	}
}

bool Engine::StartupResourceLoader::CollectPipelineFiles(const std::filesystem::path& manifestPath, Engine::ERHIType rhiType,
	std::vector<std::filesystem::path>& files, std::string& error)
{
	using namespace StartupResourceLoaderDetail;
	files.clear();
	error.clear();

	const auto resolvedManifest = PlatformPath::ResolveUri(manifestPath);
	if (resolvedManifest.empty())
		return ReportStartupError(error, "Startup pipeline manifest path is empty.");

	std::vector<std::filesystem::path> collectedFiles;
	if (!AppendStartupFile(resolvedManifest, "pipeline manifest", collectedFiles, error))
		return false;

	StartupManifestConfig manifest;
	if (!ReadStartupManifest(resolvedManifest, "pipeline", manifest, error))
		return false;

	for (const auto& step : manifest.Pipeline)
		if (!CollectStartupCategoryFiles(step, rhiType, collectedFiles, error))
			return false;

	files = std::move(collectedFiles);
	return true;
}

bool Engine::StartupResourceLoader::LoadPipeline(const std::filesystem::path& manifestPath)
{
	using namespace StartupResourceLoaderDetail;
	const auto rhiType = Renderer::IsCreated() ? Renderer::Get().GetRHIType() : ERHIType::DirectX12;
	std::vector<std::filesystem::path> files;
	std::string error;
	if (!CollectPipelineFiles(manifestPath, rhiType, files, error))
		return false;

	StartupManifestConfig manifest;
	if (!ReadStartupManifest(manifestPath, "pipeline", manifest, error))
		return false;

	BAM_LOG(Info, "Engine", "StartupResourceLoader", "Starting resource preload pipeline.");
	bool success = true;
	for (const auto& step : manifest.Pipeline)
	{
		const auto configPath = ResolveStartupUri(step.Config);
		bool stepSuccess = false;

		if (step.Category == "Textures") stepSuccess = LoadTextures(configPath);
		else if (step.Category == "Shaders") stepSuccess = LoadShaders(configPath);
		else if (step.Category == "Meshes") stepSuccess = LoadMeshes(configPath);
		else if (step.Category == "Materials") stepSuccess = LoadMaterials(configPath);
		else if (step.Category == "MaterialInstances") stepSuccess = LoadMaterialInstances(configPath);

		if (!stepSuccess)
		{
			BAM_LOG(Error, "Engine", "StartupResourceLoader", "Pipeline step failed: category={}, config={}",
				step.Category, step.Config);
			success = false;
		}
	}

	if (success)
		BAM_LOG(Info, "Engine", "StartupResourceLoader", "Resource preload pipeline completed successfully.");
	else
		BAM_LOG(Error, "Engine", "StartupResourceLoader", "Resource preload pipeline failed.");
	return success;
}

bool Engine::StartupResourceLoader::LoadTextures(const std::filesystem::path& configPath)
{
	using namespace StartupResourceLoaderDetail;
	TextureManifestConfig manifest;
	std::string error;
	return ReadStartupManifest(configPath, "Textures", manifest, error) &&
		LoadStartupFiles(manifest.Textures, "Textures");
}

bool Engine::StartupResourceLoader::LoadShaders(const std::filesystem::path& configPath)
{
	using namespace StartupResourceLoaderDetail;
	ShaderManifestConfig manifest;
	std::string error;
	if (!ReadStartupManifest(configPath, "Shaders", manifest, error))
		return false;

	const auto rhiType = Renderer::IsCreated() ? Renderer::Get().GetRHIType() : ERHIType::DirectX12;
	auto& resourceManager = ResourceManager::Get();
	bool success = true;
	for (const auto& item : manifest.Shaders)
	{
		EShaderType shaderType = EShaderType::Unknown;
		if (!TryParseStartupShaderType(item.Type, shaderType))
		{
			BAM_LOG(Error, "Engine", "StartupResourceLoader", "Unsupported shader type '{}' for key={}", item.Type, item.Key);
			success = false;
			continue;
		}

		const auto& path = GetStartupShaderPath(item, rhiType);
		ShaderDesc desc = {};
		desc.Key = Engine::Utf8ToPath(item.Key).wstring();
		desc.Path = ResolveStartupUri(path).wstring();
		desc.shaderType = shaderType;
		desc.entryPoint = item.EntryPoint.empty() ? "main" : item.EntryPoint;
		desc.numSamplers = item.Samplers;
		desc.numStorageBuffers = item.StorageBuffers;
		desc.numUniformBuffers = item.UniformBuffers;

		if (!resourceManager.LoadResource<Shader>(&desc))
		{
			BAM_LOG(Error, "Engine", "StartupResourceLoader", "Failed to load shader: key={}, path={}", item.Key, path);
			success = false;
		}
	}

	return success;
}

bool Engine::StartupResourceLoader::LoadMeshes(const std::filesystem::path& configPath)
{
	using namespace StartupResourceLoaderDetail;
	MeshManifestConfig manifest;
	std::string error;
	return ReadStartupManifest(configPath, "Meshes", manifest, error) &&
		LoadStartupFiles(manifest.Meshes, "Meshes");
}

bool Engine::StartupResourceLoader::LoadMaterials(const std::filesystem::path& configPath)
{
	using namespace StartupResourceLoaderDetail;
	MaterialManifestConfig manifest;
	std::string error;
	return ReadStartupManifest(configPath, "Materials", manifest, error) &&
		LoadStartupFiles(manifest.Materials, "Materials");
}

bool Engine::StartupResourceLoader::LoadMaterialInstances(const std::filesystem::path& configPath)
{
	using namespace StartupResourceLoaderDetail;
	MaterialInstanceManifestConfig manifest;
	std::string error;
	return ReadStartupManifest(configPath, "MaterialInstances", manifest, error) &&
		LoadStartupFiles(manifest.MaterialInstances, "MaterialInstances");
}
