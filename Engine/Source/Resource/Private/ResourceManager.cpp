#pragma once
#include "ResourceManager.h"
#include "Resources.h"
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
		if (slot.Instance)
		{
			slot.Instance->Free();
			delete slot.Instance;
		}
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
Handle ResourceManager::LoadFile(const wstring& filePath)
{
	namespace fs = std::filesystem;
	fs::path path(filePath);
	if (!fs::exists(path))
		return Handle(); // nullptr 대신 빈 핸들 반환

	wstring extension = path.extension().wstring();
	std::transform(extension.begin(), extension.end(), extension.begin(), ::towlower);

	auto iter = m_LoaderRegistry.find(extension);
	if (iter != m_LoaderRegistry.end())
	{
		wstring key = path.stem().wstring();
		return iter->second(key, filePath);
	}

	return Handle(); // 지원하지 않는 확장자일 경우 빈 핸들 반환
}
void ResourceManager::RegisterExplicitLoader()
{
	auto textureLoader = [this](wstring key, wstring path) -> Handle
		{
			tagTextureCreateDesc desc;
			desc.Key = path;
			desc.Path = path;
			return this->LoadResource<Texture>(&desc).GetRawHandle();
		};

	m_LoaderRegistry[L".png"] = textureLoader;
	m_LoaderRegistry[L".jpg"] = textureLoader;
	m_LoaderRegistry[L".bmp"] = textureLoader;
	m_LoaderRegistry[L".tga"] = textureLoader;
	m_LoaderRegistry[L".jpeg"] = textureLoader;
	m_LoaderRegistry[L".bamtex"] = textureLoader;

	m_LoaderRegistry[L".bamsprite"] = [this](wstring key, wstring path) -> Handle
		{
			tagSpriteCreateDesc desc;
			desc.Key = path;
			desc.Path = path;
			return this->LoadResource<Sprite>(&desc).GetRawHandle();
		};

	m_LoaderRegistry[L".bammat"] = [this](wstring key, wstring path) -> Handle
		{
			tagMaterialDesc desc;
			desc.Key = path;
			desc.Path = path;
			return this->LoadResource<Material>(&desc).GetRawHandle();
		};


	m_LoaderRegistry[L".bamshader"] = [this](wstring key, wstring path) -> Handle
		{
			tagShaderDesc desc;
			desc.Key = path;
			desc.Path = path;
			return this->LoadResource<Shader>(&desc).GetRawHandle();
		};

	m_LoaderRegistry[L".bammatinst"] = [this](wstring key, wstring path) -> Handle
		{
			tagMaterialInstanceDesc desc;
			desc.Key = path;
			desc.Path = path;
			return this->LoadResource<MaterialInstance>(&desc).GetRawHandle();
		};

	m_LoaderRegistry[L".bammesh"] = [this](wstring key, wstring path) -> Handle
		{
			tagMaterialInstanceDesc desc;
			desc.Key = path;
			desc.Path = path;
			return this->LoadResource<Mesh>(&desc).GetRawHandle();
		};

	m_LoaderRegistry[L".bamskeleton"] = [this](wstring key, wstring path) -> Handle
		{
			tagMaterialInstanceDesc desc;
			desc.Key = path;
			desc.Path = path;
			return this->LoadResource<Skeleton>(&desc).GetRawHandle();
		};

	m_LoaderRegistry[L".bamanim"] = [this](wstring key, wstring path) -> Handle
		{
			tagMaterialInstanceDesc desc;
			desc.Key = path;
			desc.Path = path;
			return this->LoadResource<Animation>(&desc).GetRawHandle();
		};

	m_LoaderRegistry[L".bammodel"] = [this](wstring key, wstring path) -> Handle
		{
			tagMaterialInstanceDesc desc;
			desc.Key = path;
			desc.Path = path;
			return this->LoadResource<Model>(&desc).GetRawHandle();
		};
}
EResult ResourceManager::DestroyResource(const Handle& handle)
{
	if (!handle.IsValid()) return EResult::InvalidArgument;

	Resource* toDelete = nullptr;
	{
		unique_lock lock(m_PoolMutex);
		uint32 index = handle.GetIndex();

		if (index < m_Resources.size() && m_Resources[index].Generation == handle.GetGeneration())
		{
			toDelete = FreeSlotInternal(index); // O(1) 해제, 반환된 자원은 락 밖에서 삭제
		}
	}

	if (toDelete)
	{
		toDelete->Free();
		delete toDelete;
		return EResult::Success;
	}

	return EResult::Fail;
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
vector<Handle> ResourceManager::GetResourceHandlesIncludingDerived(uint64 baseTypeID)
{
	vector<Handle> result;
	// 1. 기본 타입의 핸들들 추가
	const auto& baseHandles = GetResourceHandles(baseTypeID);
	result.insert(result.end(), baseHandles.begin(), baseHandles.end());
	// 2. 리플렉션에서 파생 타입 ID 목록을 가져와 각각의 핸들도 추가
	const auto derivedIDs = reflection::Registry::Get().GetDerivedTypeIDs(baseTypeID);
	for (uint64 derivedID : derivedIDs)
	{
		const auto& derivedHandles = GetResourceHandles(derivedID);
		result.insert(result.end(), derivedHandles.begin(), derivedHandles.end());
	}
	return result;
}
EResult ResourceManager::DestroyResource(Resource* resource)
{
	if (!resource) return EResult::InvalidArgument;

	Resource* toDelete = nullptr;
	{
		unique_lock lock(m_PoolMutex);

		for (uint32 i = 0; i < m_Resources.size(); ++i)
		{
			if (m_Resources[i].IsActive && m_Resources[i].Instance == resource)
			{
				toDelete = FreeSlotInternal(i);
				break;
			}
		}
	}

	if (toDelete)
	{
		toDelete->Free();
		delete toDelete;
	}
	else
	{
		// 풀에 없으면 강제 삭제
		resource->Free();
		delete resource;
	}

	return EResult::Success;
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

	Resource* toDelete = nullptr;
	{
		unique_lock lock(m_PoolMutex);

		uint32 index = handle.GetIndex();

		if (index < m_Resources.size() && m_Resources[index].Generation == handle.GetGeneration())
		{
			if (--m_Resources[index].RefCount <= 0)
			{
				toDelete = FreeSlotInternal(index);
			}
		}
	}

	if (toDelete)
	{
		toDelete->Free();
		delete toDelete;
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
	uint64 hash = RunTimeHash(NormalizePath(key));
	return FindHandle(hash);
}
#pragma endregion

#pragma region Resource Slot Management
Handle ResourceManager::FindHandle(uint64 hash)
{
	shared_lock lock(m_PoolMutex);
	auto iter = m_HashToHandle.find(hash);
	lock.unlock();
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
		Handle existingHandle = iter->second;
		lock.unlock(); // 데드락 방지: delete 전에 락 해제

		if (resource)
		{
			// If resource already exists, free the new one to prevent leaks
			resource->Free();
			delete resource;
		}
		return existingHandle; // Return existing handle
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

Resource* ResourceManager::FreeSlotInternal(uint32 slotIndex)
{
	if (slotIndex >= m_Resources.size() || !m_Resources[slotIndex].IsActive) return nullptr;

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

		// 데드락 방지를 위해 리소스 삭제는 락이 풀린 이후 호출자가 담당합니다.
		// resource->Free();
		// delete resource;
	}

	m_Resources[slotIndex].Instance = nullptr;
	m_Resources[slotIndex].RefCount = 0;
	m_Resources[slotIndex].IsActive = false;
	m_Resources[slotIndex].Generation++; // Invalidate old handles

	m_FreeSlots.push_back(slotIndex); // Add to free list
	
	return resource;
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

Handle ResourceManager::LoadFromJsonFile(const wstring& filePath)
{
	return LoadFile(filePath);
}

Handle ResourceManager::LoadFromBeveFile(const wstring& filePath)
{
	return LoadFile(filePath);
}

Handle ResourceManager::LoadFromBinaryFile(const wstring& filePath)
{
	return LoadFile(filePath);
}
#pragma endregion