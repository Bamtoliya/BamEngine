#pragma once

#include "AssetManager.h"

#pragma region Importer
#include "TextureImporter.h"
#include "ModelImporter.h"
#include "AnimationImporter.h"
#pragma endregion

#pragma region Exporter
//#include "TextureExporter.h"
//#include "ModelExporter.h"
//#include "AnimationExporter.h"
#pragma endregion

IMPLEMENT_SINGLETON(AssetManager);

#pragma region Construcotr&Destructor
EResult AssetManager::Initialize(void* arg)
{
	m_Importers[".png"] = TextureImporter::Create();
	m_Importers[".jpg"] = m_Importers[".png"];
	m_Importers[".tga"] = m_Importers[".png"];
	m_Importers[".bmp"] = m_Importers[".png"];

	m_Importers[".fbx"] = ModelImporter::Create();
	m_Importers[".obj"] = m_Importers[".fbx"];
	m_Importers[".gltf"] = m_Importers[".fbx"];

	m_Importers[".anim"] = AnimationImporter::Create();

	return EResult::Success;
}

void AssetManager::Free()
{
	RELEASE_MAP(m_Importers);
	RELEASE_MAP(m_Exporters);
}
#pragma endregion


#pragma region Codec
EResult AssetManager::Import(const filesystem::path& sourcePath, const filesystem::path& destDir)
{
	string extension = sourcePath.extension().string();
	for (char& c : extension) c = tolower(c);
	if (m_Importers.find(extension) != m_Importers.end())
	{
		return m_Importers[extension]->Import(sourcePath, destDir);
	}
	return EResult::Fail;
}

EResult AssetManager::Export(const filesystem::path& sourcePath, const filesystem::path& destDir)
{
	return EResult();
}
#pragma endregion