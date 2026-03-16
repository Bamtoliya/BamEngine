#pragma once
#include "ResourceManager.h"
#include "Mesh.h"
#include "Model.h"
#include "Shader.h"
#include "Sprite.h"
#include "Texture.h"
#include "Material.h"

#include "Archives.h"

IMPLEMENT_SINGLETON(ResourceManager)

#pragma region Constructor&Destructor
EResult ResourceManager::Initialize(void* arg)
{
	SerializationHelper::SetResourceInstantiatorFunction([](string_view typeName, Archive& ar, void** outInstance) -> bool
	{
		wstring path;
		ar.Process("m_Path", path);
		wstring tag;
		ar.Process("m_Tag", tag);
		if (typeName == "Mesh")
		{
			*outInstance = ResourceManager::Get().GetMesh(tag);
			return true;
		}
		else if (typeName == "Model")
		{
			*outInstance = ResourceManager::Get().GetModel(tag);
			return true;
		}
		else if (typeName == "Shader")
		{
			*outInstance = ResourceManager::Get().GetShader(tag);
			return true;
		}
		else if (typeName == "Sprite")
		{
			*outInstance = ResourceManager::Get().GetSprite(tag);
			return true;
		}
		else if (typeName == "Texture")
		{
			*outInstance = ResourceManager::Get().GetTexture(tag);
			return true;
		}
		else if (typeName == "Material")
		{
			*outInstance = ResourceManager::Get().GetMaterial(tag);
			return true;
		}
		
		return false; // 알 수 없는 타입
	});
	return EResult::Success;
}

void ResourceManager::Free()
{
	RELEASE_MAP(m_Meshes);
	RELEASE_MAP(m_Shaders)
	RELEASE_MAP(m_Textures);
	RELEASE_MAP(m_Materials);
	RELEASE_MAP(m_Sprites);
	RELEASE_MAP(m_Models);
	
	for (auto& pair : m_GenericContainers)
	{
		pair.second->Clear();
		delete pair.second;
	}
	m_GenericContainers.clear();
}
#pragma endregion

#pragma region Resource Management

#pragma region Load
Mesh* ResourceManager::LoadMesh(const wstring& key, void* arg)
{
	if (m_Meshes.find(key) != m_Meshes.end())
		return m_Meshes[key];
	Mesh* mesh = Mesh::Create(arg);
	if (!mesh)
		return nullptr;
	mesh->SetTag(key);
	m_Meshes.emplace(key, mesh);

	return mesh;
}
Model* ResourceManager::LoadModel(const wstring& key, void* arg)
{
	if (m_Models.find(key) != m_Models.end())
		return m_Models[key];
	Model* model = Model::Create(arg);
	if (!model)
		return nullptr;
	model->SetTag(key);
	m_Models.emplace(key, model);
	return model;
}
Shader* ResourceManager::LoadShader(const wstring& key, void* arg)
{
	if (m_Shaders.find(key) != m_Shaders.end())
		return m_Shaders[key];
	Shader* shader = Shader::Create(arg);
	if (!shader)
		return nullptr;
	shader->SetTag(key);
	m_Shaders.emplace(key, shader);
	return shader;
}
Sprite*  ResourceManager::LoadSprite(const wstring& key, void* arg)
{
	if (m_Sprites.find(key) != m_Sprites.end())
		return m_Sprites[key];
	Sprite* sprite = Sprite::Create(arg);
	if (!sprite)
	{
		Safe_Release(sprite);
		return nullptr;
	}
	sprite->SetTag(key);
	m_Sprites.emplace(key, sprite);
	return sprite;
}
Texture* ResourceManager::LoadTexture(const wstring& key, const wstring& texturePath)
{
	if (m_Textures.find(key) != m_Textures.end())
		return m_Textures[key];
	Texture* texture = Texture::Create((void*)(texturePath.c_str()));
	if (!texture)
	{
		Safe_Release(texture);
		return nullptr;
	}
	texture->SetTag(key);
	m_Textures.emplace(key, texture);
	return texture;
}
Material* ResourceManager::LoadMaterial(const wstring& key, void* arg)
{
	if (m_Materials.find(key) != m_Materials.end())
		return m_Materials[key];
	Material* material = Material::Create(arg);
	if (!material)
	{
		Safe_Release(material);
		return nullptr;
	}
	material->SetTag(key);
	m_Materials.emplace(key, material);
	return material;
}
#pragma endregion

#pragma region Getter
Mesh* ResourceManager::GetMesh(const wstring& key)
{
	auto	iter = m_Meshes.find(key);
	if (iter != m_Meshes.end())
		return iter->second;
	return nullptr;
}
Model* ResourceManager::GetModel(const wstring& key)
{
	auto	iter = m_Models.find(key);
	if (iter != m_Models.end())
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
Sprite* ResourceManager::GetSprite(const wstring& key)
{
	auto	iter = m_Sprites.find(key);
	if (iter != m_Sprites.end())
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
#pragma endregion

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
		if (iter->second(key, filePath))
			return EResult::Success;
		else
			return EResult::Fail;;
	}
	else
	{
		return EResult::NotImplemented;
	}

	return EResult::Success;
}
void ResourceManager::RegisterExplicitLoader()
{
	auto textureLoader = [this](wstring key, wstring path) -> Texture*
	{
		return this->LoadTexture(key, path);
	};

	m_LoaderRegistry[L".png"] = textureLoader;
	m_LoaderRegistry[L".jpg"] = textureLoader;
	m_LoaderRegistry[L".bmp"] = textureLoader;
	m_LoaderRegistry[L".tga"] = textureLoader;
	m_LoaderRegistry[L".jpeg"] = textureLoader;
	m_LoaderRegistry[L".bamtexture"] = textureLoader;

	auto meshLoader = [this](wstring key, wstring path) -> Mesh*
	{
		//MESHDESC meshDesc = {};
		//meshDesc.FilePath = path;
		//return this->LoadMesh(key, (void*)&meshDesc);
		return nullptr;
	};

	m_LoaderRegistry[L".mesh"] = meshLoader;
	m_LoaderRegistry[L".obj"] = meshLoader;
	m_LoaderRegistry[L".fbx"] = meshLoader;
}


#pragma region Save & Load 
EResult ResourceManager::SaveToJsonFile(Resource* resource, const wstring& filePath)
{

	JsonArchive archive(EArchiveMode::Write);
	resource->Serialize(archive);
	return archive.SaveToFile(WStrToStr(filePath)) ? EResult::Success : EResult::Fail;
}

EResult ResourceManager::SaveToBeveFile(Resource* resource, const wstring& filePath)
{
	BeveArchive archive(EArchiveMode::Write);
	resource->Serialize(archive);
	return archive.SaveToFile(WStrToStr(filePath)) ? EResult::Success : EResult::Fail;
}

EResult ResourceManager::SaveToBinaryFile(Resource* resource, const wstring& filePath)
{
	BinaryArchive archive(EArchiveMode::Write);
	resource->Serialize(archive);
	return archive.SaveToFile(WStrToStr(filePath)) ? EResult::Success : EResult::Fail;
}
#pragma endregion

#pragma endregion
