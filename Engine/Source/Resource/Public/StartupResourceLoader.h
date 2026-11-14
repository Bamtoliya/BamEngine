#pragma once

#include "Engine_Includes.h"

namespace Engine
{
	enum class ERHIType;

	class ENGINE_API StartupResourceLoader
	{
	public:
		static bool LoadPipeline(const std::filesystem::path& manifestPath);
		static bool CollectPipelineFiles(const std::filesystem::path& manifestPath, ERHIType rhiType,
			std::vector<std::filesystem::path>& files, std::string& error);

	private:
		static bool LoadTextures(const std::filesystem::path& configPath);
		static bool LoadShaders(const std::filesystem::path& configPath);
		static bool LoadMeshes(const std::filesystem::path& configPath);
		static bool LoadMaterials(const std::filesystem::path& configPath);
		static bool LoadMaterialInstances(const std::filesystem::path& configPath);
	};
}
