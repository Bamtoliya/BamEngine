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
	// 초기 용량 설정 (필요에 따라 조정)
	m_Resources.reserve(100);
	RegisterExplicitLoader();
	return EResult::Success;
}

void ResourceManager::Free()
{
	for (auto& slot : m_Resources)
	{
		slot.Instance->Free();
		delete slot.Instance;
		slot.Instance = nullptr;
	}
	m_Resources.clear();
	m_FreeSlots.clear();
	m_HashToHandle.clear();
}

#pragma endregion

#pragma region Resource Management
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
void ResourceManager::RegisterExplicitLoader()
{
	//auto textureLoader = [this](wstring key, wstring path) -> Texture*
	//{
	//	return this->LoadTexture(key, path);
	//};
	//
	//m_LoaderRegistry[L".png"] = textureLoader;
	//m_LoaderRegistry[L".jpg"] = textureLoader;
	//m_LoaderRegistry[L".bmp"] = textureLoader;
	//m_LoaderRegistry[L".tga"] = textureLoader;
	//m_LoaderRegistry[L".jpeg"] = textureLoader;
	//
	//auto meshLoader = [this](wstring key, wstring path) -> Mesh*
	//{
	//	//MESHDESC meshDesc = {};
	//	//meshDesc.FilePath = path;
	//	//return this->LoadMesh(key, (void*)&meshDesc);
	//	return nullptr;
	//};
	//
	//m_LoaderRegistry[L".mesh"] = meshLoader;
	//m_LoaderRegistry[L".obj"] = meshLoader;
	//m_LoaderRegistry[L".fbx"] = meshLoader;
	//
	//auto jsonLoader = [this](wstring key, wstring path) -> void*
	//	{
	//		return this->LoadFromJsonFile(path);
	//	};
	//m_LoaderRegistry[L".json"] = jsonLoader;
	//
	//auto beveLoader = [this](wstring key, wstring path) -> void*
	//	{
	//		return this->LoadFromBeveFile(path);
	//	};
	//m_LoaderRegistry[L".beve"] = beveLoader;
	//
	//auto binaryLoader = [this](wstring key, wstring path) -> void*
	//	{
	//		return this->LoadFromBinaryFile(path);
	//	};
	//m_LoaderRegistry[L".bin"] = binaryLoader;
	//
	////Custonm binary formats
	//m_LoaderRegistry[L".asset"] = binaryLoader;
	//m_LoaderRegistry[L".bamtex"] = binaryLoader;
	//m_LoaderRegistry[L".bammat"] = binaryLoader;
}
const vector<Handle>& ResourceManager::GetResourceHandles(uint64 typeHash)
{
	static const vector<Handle> emptyList; // 반환용 빈 리스트
	shared_lock lock(m_PoolMutex); // 읽기 락
	auto it = m_TypeToHandles.find(typeHash);
	if (it != m_TypeToHandles.end())
	{
		return it->second;
	}
	return emptyList;
}
#pragma endregion

#pragma region Handle Management
void ResourceManager::AddRefResource(const Handle& handle)
{
	if (!handle.IsValid()) return;

	unique_lock lock(m_PoolMutex);

	uint32 index = handle.GetIndex();
	if (index < m_Resources.size() && m_Resources[index].Generation == handle.GetGeneration())
	{
		m_Resources[index].RefCount++;
	}
}

void ResourceManager::ReleaseResource(const Handle& handle)
{
	if (!handle.IsValid()) return;

	unique_lock lock(m_PoolMutex);

	uint32 index = handle.GetIndex();

	if (index < m_Resources.size() && m_Resources[index].Generation == handle.GetGeneration())
	{
		if (--m_Resources[index].RefCount <= 0)
		{
			FreeSlotInternal(index);
		}
	}
}

bool ResourceManager::IsValid(const Handle& handle)
{
	if (!handle.IsValid()) return false;
	shared_lock lock(m_PoolMutex);
	uint32 index = handle.GetIndex();
	return index < m_Resources.size() && m_Resources[index].Generation == handle.GetGeneration() && m_Resources[index].Instance != nullptr;
}

Resource* ResourceManager::GetResource(const Handle& handle)
{
	if (!handle.IsValid()) return nullptr;
	shared_lock lock(m_PoolMutex);
	uint32 index = handle.GetIndex();
	if (index < m_Resources.size() && m_Resources[index].Generation == handle.GetGeneration())
	{
		return m_Resources[index].Instance;
	}
	return nullptr;
}
Handle ResourceManager::FindHandleByKey(const wstring& key)
{
	uint64 hash = RunTimeHash(key);
	return FindHandle(hash);
}
#pragma endregion

#pragma region Resource Slot Management
Handle ResourceManager::FindHandle(uint64 hash)
{
	shared_lock lock(m_PoolMutex);
	auto iter = m_HashToHandle.find(hash);
	if (iter != m_HashToHandle.end())
	{
		return iter->second;
	}
	return Handle();
}

Handle ResourceManager::AddResourceInternal(uint64 hash, Resource* resource)
{
	unique_lock lock(m_PoolMutex);
	// Check for existing resource
	auto iter = m_HashToHandle.find(hash);
	if (iter != m_HashToHandle.end())
	{
		if (resource)
		{
			// If resource already exists, free the new one to prevent leaks
			resource->Free();
			delete resource;
		}
		return iter->second; // Return existing handle
	}

	Handle handle = AllocateSlot(resource);
	m_HashToHandle[hash] = handle; // Map hash to handle

	uint64 typeHash = resource->GetTypeInfo().ID;
	m_TypeToHandles[typeHash].push_back(handle);
	return handle;
}

Handle ResourceManager::AllocateSlot(Resource* resource)
{
	uint32 index;

	if (m_FreeSlots.empty())
	{
		index = static_cast<uint32>(m_Resources.size());
		m_Resources.emplace_back();
	}
	else
	{
		index = m_FreeSlots.back();
		m_FreeSlots.pop_back();
	}

	m_Resources[index].Instance = resource;
	m_Resources[index].RefCount = 1;
	m_Resources[index].IsActive = true;
	return Handle(index, m_Resources[index].Generation);
}

void ResourceManager::FreeSlotInternal(uint32 slotIndex)
{
	if (slotIndex < m_Resources.size() || !m_Resources[slotIndex].IsActive) return;

	Resource* resource = m_Resources[slotIndex].Instance;
	if(resource)
	{
		uint64 hash = RunTimeHash(resource->GetKey());
		m_HashToHandle.erase(hash);

		uint64 typeHash = resource->GetTypeInfo().ID;
		auto& handles = m_TypeToHandles[typeHash];
		Handle targetHandle(slotIndex, m_Resources[slotIndex].Generation);

		auto it = std::find(handles.begin(), handles.end(), targetHandle);
		if (it != handles.end())
		{
			*it = handles.back();
			handles.pop_back();
		}

		resource->Free();
		delete resource;
	}

	m_Resources[slotIndex].Instance = nullptr;
	m_Resources[slotIndex].RefCount = 0;
	m_Resources[slotIndex].IsActive = false;
	m_Resources[slotIndex].Generation++; // Invalidate old handles

	m_FreeSlots.push_back(slotIndex); // Add to free list
}
#pragma endregion

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
	//JsonArchive archive(EArchiveMode::Read);
	//if (!archive.LoadFromFile(WStrToStr(filePath)))
	//	return nullptr;
	//string typeName;
	//archive.Process("__Type__", typeName);
	//
	//if (typeName.empty())
	//	return nullptr;
	//
	//void* instance = ReflectionRegistry::Get().CreateInstance(typeName);
	//if (!instance)
	//	return nullptr;
	//
	//Resource* resource = static_cast<Resource*>(instance);
	//resource->Deserialize(archive);
	//
	//wstring tag = resource->GetTag();
	//if (typeName == "Mesh") m_Meshes[tag] = static_cast<Mesh*>(resource);
	//else if (typeName == "Model") m_Models[tag] = static_cast<Model*>(resource);
	//else if (typeName == "Shader") m_Shaders[tag] = static_cast<Shader*>(resource);
	//else if (typeName == "Sprite") m_Sprites[tag] = static_cast<Sprite*>(resource);
	//else if (typeName == "Texture") m_Textures[tag] = static_cast<Texture*>(resource);
	//else if (typeName == "Material") m_Materials[tag] = static_cast<Material*>(resource);
	//
	//return resource;
	return nullptr;
}

void* ResourceManager::LoadFromBeveFile(const wstring& filePath)
{
	//BeveArchive archive(EArchiveMode::Read); // Read 모드로 수정
	//if (!archive.LoadFromFile(WStrToStr(filePath)))
	//	return nullptr;
	//
	//string typeName;
	//archive.Process("__Type__", typeName);
	//
	//if (typeName.empty())
	//	return nullptr;
	//
	//void* instance = ReflectionRegistry::Get().CreateInstance(typeName);
	//if (!instance)
	//	return nullptr;
	//Resource* resource = static_cast<Resource*>(instance);
	//resource->Deserialize(archive);
	//
	//wstring tag = resource->GetTag();
	//if (typeName == "Mesh") m_Meshes[tag] = static_cast<Mesh*>(resource);
	//else if (typeName == "Model") m_Models[tag] = static_cast<Model*>(resource);
	//else if (typeName == "Shader") m_Shaders[tag] = static_cast<Shader*>(resource);
	//else if (typeName == "Sprite") m_Sprites[tag] = static_cast<Sprite*>(resource);
	//else if (typeName == "Texture") m_Textures[tag] = static_cast<Texture*>(resource);
	//else if (typeName == "Material") m_Materials[tag] = static_cast<Material*>(resource);
	//
	//return resource;
	return nullptr;
}

void* ResourceManager::LoadFromBinaryFile(const wstring& filePath)
{
	//BinaryArchive archive(EArchiveMode::Read); // Read 모드로 수정
	//if (!archive.LoadFromFile(WStrToStr(filePath)))
	//	return nullptr;
	//
	//string typeName;
	//archive.Process("__Type__", typeName);
	//
	//if (typeName.empty())
	//	return nullptr;
	//
	//void* instance = ReflectionRegistry::Get().CreateInstance(typeName);
	//if (!instance)
	//	return nullptr;
	//Resource* resource = static_cast<Resource*>(instance);
	//resource->Deserialize(archive);
	//
	//std::filesystem::path fsPath(filePath);
	//
	//// 1. Path 세팅 (실제 로드한 파일의 전체/상대 경로)
	//resource->SetPath(filePath);
	//
	//// 2. 직렬화된 Tag가 비어있다면, 파일 경로를 기반으로 Tag를 만들어줍니다.
	//if (resource->GetTag().empty())
	//{
	//	// 추천: 파일 이름만 쓰지 말고, 충돌 방지를 위해 적절한 상대 경로를 Tag로 씁니다.
	//	// 임시로 filename의 확장자를 제외한 부분(stem)을 사용하되, 
	//	// 향후 프로젝트 규모가 커지면 "경로+이름" 형태의 고유 식별자로 바꾸시는 것을 권장합니다.
	//	wstring newTag = fsPath.stem().wstring();
	//	resource->SetTag(newTag);
	//}
	//// ---------------------------------------------------------
	//
	//wstring tag = resource->GetTag();
	//if (typeName == "Mesh") m_Meshes[tag] = static_cast<Mesh*>(resource);
	//else if (typeName == "Model") m_Models[tag] = static_cast<Model*>(resource);
	//else if (typeName == "Shader") m_Shaders[tag] = static_cast<Shader*>(resource);
	//else if (typeName == "Sprite") m_Sprites[tag] = static_cast<Sprite*>(resource);
	//else if (typeName == "Texture") m_Textures[tag] = static_cast<Texture*>(resource);
	//else if (typeName == "Material") m_Materials[tag] = static_cast<Material*>(resource);
	//
	//return resource;
	return nullptr;
}
#pragma endregion