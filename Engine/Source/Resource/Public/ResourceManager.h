#pragma once

#include "Base.h"
#include "Resources.h"
#include "ResourceHandle.h"

BEGIN(Engine)

#pragma region Generic Interface
struct ResourceSlot
{
	Resource* Instance = { nullptr };
	uint32 Generation = { 0 };
	uint32 RefCount = { 0 };
	bool IsActive = { false };
};
#pragma endregion

class ENGINE_API ResourceManager : public Base
{
	DECLARE_SINGLETON(ResourceManager)
#pragma region Constructor&Destructor
private:
	ResourceManager() {}
	virtual ~ResourceManager() = default;
	EResult Initialize(void* arg = nullptr);
public:
	virtual void Free() override;
#pragma endregion

#pragma region Resource Management
public:
	template<typename T, typename... Args>
	ResourceHandle<T> LoadResource(const wstring& path, Args&&... args)
	{
		static_assert(is_base_of_v<Resource, T>, "T must be derived from Resource");

		uint64 hash = RunTimeHash(path);
		Handle handle = FindHandle(hash);

		if (handle.IsValid())
		{
			return ResourceHandle<T>(handle);
		}

		T* resource = T::Create(forward<Args>(args)...);
		if(!resource)
			return ResourceHandle<T>();

		resource->SetKey(path);
		handle = AddResourceInternal(path, resource);
		return ResourceHandle<T>(handle);
	}
	template <typename T>
	ResourceHandle<T> AddResource(const wstring& key, T* resource)
	{
		static_assert(is_base_of_v<Resource, T>, "T must be derived from Resource");
		if(!resource) return ResourceHandle<T>();

		uint64 hash = RunTimeHash(key);
		resource->SetKey(key);

		return ResourceHandle<T>(AddResourceInternal(key, resource));
	}

	template <typename T>
	ResourceHandle<T> GetResourceHandle(const wstring& key)
	{
		uint64 hash = RunTimeHash(key);
		return ResourceHandle<T>(FindHandle(hash));
	}
	template<typename T>
	const vector<Handle>& GetResourceHandles()
	{
		return GetResourceHandles(T::GetStaticTypeInfo().ID);
	}

	const vector<Handle>& GetResourceHandles(uint64 typeHash);
public:
	EResult ImportFolder(const wstring& folderPath);
	void* LoadFile(const wstring& filePath);
	void RegisterExplicitLoader();
#pragma endregion

#pragma region Handle Management
public:
	void AddRefResource(const Handle& handle);
	void ReleaseResource(const Handle& handle);
	bool IsValid(const Handle& handle);
	Resource* GetResource(const Handle& handle);
#pragma endregion

#pragma region Slot Management
private:
	Handle FindHandle(uint64 hash);
	Handle AddResourceInternal(const wstring& key, Resource* resource);
	Handle AllocateSlot(Resource* resource);
	void FreeSlotInternal(uint32 slotIndex);
#pragma endregion

#pragma region Save & Load
public:
	EResult SaveToJsonFile(Resource* resource, const wstring& filePath);
	EResult SaveToBeveFile(Resource* resource, const wstring& filePath);
	EResult SaveToBinaryFile(Resource* resource, const wstring& filePath);
public:
	void* LoadFromJsonFile(const wstring& filePath);
	void* LoadFromBeveFile(const wstring& filePath);
	void* LoadFromBinaryFile(const wstring& filePath);
#pragma endregion

#pragma region Member Variables
private:
	vector<ResourceSlot> m_Resources;
	vector<uint32> m_FreeSlots;
	unordered_map<uint64, Handle> m_HashToHandle;
	unordered_map<uint64, vector<Handle>> m_TypeToHandles;
	shared_mutex m_PoolMutex;
private:
	unordered_map<wstring, function<void*(wstring, wstring)>> m_LoaderRegistry;
#pragma endregion
};

#include "ResourceHandle.inl"
END