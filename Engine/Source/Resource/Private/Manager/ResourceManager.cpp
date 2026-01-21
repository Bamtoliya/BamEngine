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
	RELEASE_MAP(m_Shaders)
	RELEASE_MAP(m_Textures);
	RELEASE_MAP(m_Materials);
	
	for (auto& pair : m_GenericContainers)
	{
		pair.second->Clear();
		delete pair.second;
	}
	m_GenericContainers.clear();
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
EResult ResourceManager::LoadShader(const wstring& key, void* arg)
{
	if (m_Shaders.find(key) != m_Shaders.end())
		return EResult::AlreadyInitialized;
	Shader* shader = Shader::Create(arg);
	if (!shader)
		return EResult::Fail;
	m_Shaders.emplace(key, shader);
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
EResult ResourceManager::LoadMaterial(const wstring& key, void* arg)
{
	// Implementation for loading material goes here
	return EResult::NotImplemented;
}
Mesh* ResourceManager::GetMesh(const wstring& key)
{
	auto	iter = m_Meshes.find(key);
	if (iter != m_Meshes.end())
		return iter->second;
	return nullptr;
}
Shader* ResourceManager::GetShader(const wstring& key)
{
	auto	iter = m_Shaders.find(key);
	if (iter != m_Shaders.end())
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
Material* ResourceManager::GetMaterial(const wstring& key)
{
	auto	iter = m_Materials.find(key);
	if (iter != m_Materials.end())
		return iter->second;
	return nullptr;
}
EResult ResourceManager::ImportFolder(const wstring& folderPath)
{
	namespace fs = std::filesystem;
	fs::path rootPath(folderPath);

	if (!fs::exists(rootPath) || !fs::is_directory(rootPath))
		return EResult::FileNotFound;

	for (const auto& entry : fs::recursive_directory_iterator(rootPath))
	{
		if (fs::is_regular_file(entry.status()))
		{
			LoadFile(entry.path().wstring());
		}
	}

	return EResult::Success;
}
EResult ResourceManager::LoadFile(const wstring& filePath)
{
	namespace fs = std::filesystem;
	fs::path path(filePath);
	if(!fs::exists(path))
		return EResult::FileNotFound;

	wstring extension = path.extension().wstring();

	std::transform(extension.begin(), extension.end(), extension.begin(), ::towlower);

	auto iter = m_LoaderRegistry.find(extension);
	if (iter != m_LoaderRegistry.end())
	{
		wstring key = path.stem().wstring();
		return iter->second(key, filePath);
	}
	else
	{
		return EResult::NotImplemented;
	}

	return EResult::Success;
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
	m_LoaderRegistry[L".tga"] = textureLoader;
	m_LoaderRegistry[L".jpeg"] = textureLoader;

	auto meshLoader = [this](wstring key, wstring path) -> EResult
	{
		//MESHDESC meshDesc = {};
		//meshDesc.FilePath = path;
		//return this->LoadMesh(key, (void*)&meshDesc);
		return EResult::NotImplemented;
	};

	m_LoaderRegistry[L".mesh"] = meshLoader;
	m_LoaderRegistry[L".obj"] = meshLoader;
	m_LoaderRegistry[L".fbx"] = meshLoader;
}
#pragma endregion
