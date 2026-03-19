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
	RegisterExplicitLoader();
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
void* ResourceManager::LoadFile(const wstring& filePath)
{
	namespace fs = std::filesystem;
	fs::path path(filePath);
	if(!fs::exists(path))
		return nullptr;

	wstring extension = path.extension().wstring();

	std::transform(extension.begin(), extension.end(), extension.begin(), ::towlower);

	auto iter = m_LoaderRegistry.find(extension);
	if (iter != m_LoaderRegistry.end())
	{
		wstring key = path.stem().wstring();
		return iter->second(key, filePath);
	}

	return nullptr;
}
void* ResourceManager::GetOrLoadFile(const wstring& key, const wstring& filePath)
{
	namespace fs = std::filesystem;
	fs::path path(filePath);
	if (!fs::exists(path))
		return nullptr;

	wstring extension = path.extension().wstring();

	std::transform(extension.begin(), extension.end(), extension.begin(), ::towlower);

	auto iter = m_LoaderRegistry.find(extension);
	if (iter != m_LoaderRegistry.end())
	{
		wstring key = path.stem().wstring();
		return iter->second(key, filePath);
	}

	return nullptr;
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

	auto jsonLoader = [this](wstring key, wstring path) -> void*
		{
			return this->LoadFromJsonFile(path);
		};
	m_LoaderRegistry[L".json"] = jsonLoader;

	auto beveLoader = [this](wstring key, wstring path) -> void*
		{
			return this->LoadFromBeveFile(path);
		};
	m_LoaderRegistry[L".beve"] = beveLoader;

	auto binaryLoader = [this](wstring key, wstring path) -> void*
		{
			return this->LoadFromBinaryFile(path);
		};
	m_LoaderRegistry[L".bin"] = binaryLoader;

	//Custonm binary formats
	m_LoaderRegistry[L".asset"] = binaryLoader;
	m_LoaderRegistry[L".bamtex"] = binaryLoader;
	m_LoaderRegistry[L".bammat"] = binaryLoader;
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

void* ResourceManager::LoadFromJsonFile(const wstring& filePath)
{
	JsonArchive archive(EArchiveMode::Read);
	if (!archive.LoadFromFile(WStrToStr(filePath)))
		return nullptr;
	string typeName;
	archive.Process("__Type__", typeName);

	if (typeName.empty())
		return nullptr;

	void* instance = ReflectionRegistry::Get().CreateInstance(typeName);
	if (!instance)
		return nullptr;

	Resource* resource = static_cast<Resource*>(instance);
	resource->Deserialize(archive);

	wstring tag = resource->GetTag();
	if (typeName == "Mesh") m_Meshes[tag] = static_cast<Mesh*>(resource);
	else if (typeName == "Model") m_Models[tag] = static_cast<Model*>(resource);
	else if (typeName == "Shader") m_Shaders[tag] = static_cast<Shader*>(resource);
	else if (typeName == "Sprite") m_Sprites[tag] = static_cast<Sprite*>(resource);
	else if (typeName == "Texture") m_Textures[tag] = static_cast<Texture*>(resource);
	else if (typeName == "Material") m_Materials[tag] = static_cast<Material*>(resource);

	return resource;
}

void* ResourceManager::LoadFromBeveFile(const wstring& filePath)
{
	BeveArchive archive(EArchiveMode::Read); // Read 모드로 수정
	if (!archive.LoadFromFile(WStrToStr(filePath)))
		return nullptr;

	string typeName;
	archive.Process("__Type__", typeName);

	if (typeName.empty())
		return nullptr;

	void* instance = ReflectionRegistry::Get().CreateInstance(typeName);
	if (!instance)
		return nullptr;
	Resource* resource = static_cast<Resource*>(instance);
	resource->Deserialize(archive);

	wstring tag = resource->GetTag();
	if (typeName == "Mesh") m_Meshes[tag] = static_cast<Mesh*>(resource);
	else if (typeName == "Model") m_Models[tag] = static_cast<Model*>(resource);
	else if (typeName == "Shader") m_Shaders[tag] = static_cast<Shader*>(resource);
	else if (typeName == "Sprite") m_Sprites[tag] = static_cast<Sprite*>(resource);
	else if (typeName == "Texture") m_Textures[tag] = static_cast<Texture*>(resource);
	else if (typeName == "Material") m_Materials[tag] = static_cast<Material*>(resource);

	return resource;
}

void* ResourceManager::LoadFromBinaryFile(const wstring& filePath)
{
	BinaryArchive archive(EArchiveMode::Read); // Read 모드로 수정
	if (!archive.LoadFromFile(WStrToStr(filePath)))
		return nullptr;

	string typeName;
	archive.Process("__Type__", typeName);

	if (typeName.empty())
		return nullptr;

	void* instance = ReflectionRegistry::Get().CreateInstance(typeName);
	if (!instance)
		return nullptr;
	Resource* resource = static_cast<Resource*>(instance);
	resource->Deserialize(archive);

	std::filesystem::path fsPath(filePath);

	// 1. Path 세팅 (실제 로드한 파일의 전체/상대 경로)
	resource->SetPath(filePath);

	// 2. 직렬화된 Tag가 비어있다면, 파일 경로를 기반으로 Tag를 만들어줍니다.
	if (resource->GetTag().empty())
	{
		// 추천: 파일 이름만 쓰지 말고, 충돌 방지를 위해 적절한 상대 경로를 Tag로 씁니다.
		// 임시로 filename의 확장자를 제외한 부분(stem)을 사용하되, 
		// 향후 프로젝트 규모가 커지면 "경로+이름" 형태의 고유 식별자로 바꾸시는 것을 권장합니다.
		wstring newTag = fsPath.stem().wstring();
		resource->SetTag(newTag);
	}
	// ---------------------------------------------------------

	wstring tag = resource->GetTag();
	if (typeName == "Mesh") m_Meshes[tag] = static_cast<Mesh*>(resource);
	else if (typeName == "Model") m_Models[tag] = static_cast<Model*>(resource);
	else if (typeName == "Shader") m_Shaders[tag] = static_cast<Shader*>(resource);
	else if (typeName == "Sprite") m_Sprites[tag] = static_cast<Sprite*>(resource);
	else if (typeName == "Texture") m_Textures[tag] = static_cast<Texture*>(resource);
	else if (typeName == "Material") m_Materials[tag] = static_cast<Material*>(resource);

	return resource;
}
#pragma endregion

#pragma endregion
