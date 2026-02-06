#pragma once
#include "ResourceManager.h"
#include "Mesh.h"
#include "Shader.h"
#include "Sprite.h"
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
	mesh->SetTag(key);
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
	shader->SetTag(key);
	m_Shaders.emplace(key, shader);
	return EResult::Success;
}
EResult ResourceManager::LoadSprite(const wstring& key, void* arg)
{
	if (m_Sprites.find(key) != m_Sprites.end())
		return EResult::AlreadyInitialized;
	Sprite* sprite = Sprite::Create(arg);
	if (!sprite)
		return EResult::Fail;
	sprite->SetTag(key);
	m_Sprites.emplace(key, sprite);
	return EResult::Success;
}
EResult ResourceManager::LoadTexture(const wstring& key, const wstring& texturePath)
{
	if (m_Textures.find(key) != m_Textures.end())
		return EResult::AlreadyInitialized;
	Texture* texture = Texture::Create((void*)(texturePath.c_str()));
	if (!texture)
	{
		Safe_Release(texture);
		return EResult::Fail;
	}
	if (!texture)
		return EResult::Fail;
	texture->SetTag(key);
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

#ifdef _DEBUG
#pragma region Test
void ResourceManager::CreateQuadMesh()
{
	vector<Vertex2D> 	vertices;
	vector<uint32>	indices;
	vertices.resize(4);
	indices.resize(6);
	
	vertices[0].position = glm::vec3(-0.5f, 0.5f, 0.f);
	vertices[1].position = glm::vec3(0.5f, 0.5f, 0.f);
	vertices[2].position = glm::vec3(0.5f, -0.5f, 0.f);
	vertices[3].position = glm::vec3(-0.5f, -0.5f, 0.f);

	vertices[0].color = glm::vec4(1.f, 0.f, 0.f, 1.f);
	vertices[1].color = glm::vec4(0.f, 1.f, 0.f, 1.f);
	vertices[2].color = glm::vec4(0.f, 0.f, 1.f, 1.f);
	vertices[3].color = glm::vec4(1.f, 1.f, 1.f, 1.f);

	vertices[0].texCoord = glm::vec2(0.f, 0.f);
	vertices[1].texCoord = glm::vec2(1.f, 0.f);
	vertices[2].texCoord = glm::vec2(1.f, 1.f);
	vertices[3].texCoord = glm::vec2(0.f, 1.f);

	indices = { 0, 1, 2, 2, 3, 0 };

	//Mesh* quadMesh = Mesh::CreateFromData(vertices.data(), sizeof(Vertex2D), (uint32)vertices.size(),
	//	indices.data(), (uint32)indices.size());


}
#pragma endregion
#endif
