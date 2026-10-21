#pragma once

#include "Editor_Includes.h"
#include <filesystem>

BEGIN(Editor)

class StartupResourceLoader
{
public:
	// StartupManifest.json을 읽어 의존성 순서대로 카테고리별 리소스를 순차 로드합니다.
	static bool LoadPipeline(const std::filesystem::path& manifestPath);

private:
	static bool LoadTextures(const std::filesystem::path& configPath);
	static bool LoadShaders(const std::filesystem::path& configPath);
	static bool LoadMeshes(const std::filesystem::path& configPath);
	static bool LoadMaterials(const std::filesystem::path& configPath);
	static bool LoadMaterialInstances(const std::filesystem::path& configPath);
};

END