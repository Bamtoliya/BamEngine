#pragma once
#include "ResourceManager.h"
#include "Mesh.h"
#include "Shader.h"
#include "Texture.h"

IMPLEMENT_SINGLETON(ResourceManager)

#pragma region Constructor&Destructor
EResult ResourceManager::Initialize(void* arg)
{
	return EResult::Success;
}

void ResourceManager::Free()
{
	RELEASE_MAP(m_Meshes);
	RELEASE_MAP(m_Textures);
	RELEASE_MAP(m_GenericContainers);
}
#pragma endregion


#pragma region Resource Management
EResult ResourceManager::LoadMesh(const wstring& key, void* arg)
{
	if (m_Meshes.find(key) != m_Meshes.end())
		return EResult::AlreadyInitialized;
	Mesh* mesh = Mesh::Create(arg);
	if (!mesh)
		return EResult::Fail;
	m_Meshes.emplace(key, mesh);
	return EResult::Success;
}
EResult ResourceManager::LoadTexture(const wstring& key, const wstring& texturePath)
{
	if (m_Textures.find(key) != m_Textures.end())
		return EResult::AlreadyInitialized;
	Texture* texture = Texture::Create((void*)&texturePath);
	if (!texture)
		return EResult::Fail;
	m_Textures.emplace(key, texture);
	return EResult::Success;
}
Mesh* ResourceManager::GetMesh(const wstring& key)
{
	auto	iter = m_Meshes.find(key);
	if (iter != m_Meshes.end())
		return iter->second;
	return nullptr;
}
Texture* ResourceManager::GetTexture(const wstring& key)
{
	auto	iter = m_Textures.find(key);
	if (iter != m_Textures.end())
		return iter->second;
	return nullptr;
}
EResult ResourceManager::ImportFolder(const wstring& folderPath)
{
	return EResult();
}
EResult ResourceManager::LoadFile(const wstring& filePath)
{
	return EResult();
}
void ResourceManager::RegisterExplicitLoader()
{
	auto textureLoader = [this](wstring key, wstring path) -> EResult
	{
		return this->LoadTexture(key, path);
	};

	m_LoaderRegistry[L".png"] = textureLoader;
	m_LoaderRegistry[L".jpg"] = textureLoader;
	m_LoaderRegistry[L".bmp"] = textureLoader;

	auto meshLoader = [this](wstring key, wstring path) -> EResult
	{
		//MESHDESC meshDesc = {};
		//meshDesc.FilePath = path;
		//return this->LoadMesh(key, (void*)&meshDesc);
	};

	m_LoaderRegistry[L".mesh"] = meshLoader;


}
#pragma endregion
