#pragma once

#include "Base.h"
#include "Resources.h"
#include "ResourceHandle.h"
#include "Archives.h"

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
	ResourceHandle<T> LoadResource(Args&&... args);
	template <typename T>
	ResourceHandle<T> AddResource(const wstring_view& key, T* resource);
	template <typename T>
	ResourceHandle<T> GetResourceHandle(const wstring& key);
	template<typename T>
	const vector<Handle>& GetResourceHandles();
	const vector<Handle>& GetResourceHandles(uint64 typeHash);
public:
	EResult ImportFolder(const wstring& folderPath);
	void* LoadFile(const wstring& filePath);
	void RegisterExplicitLoader();
	EResult DestroyResource(Resource* resource);
#pragma endregion

#pragma region Handle Management
public:
	void AddRefResource(const Handle& handle);
	void ReleaseResource(const Handle& handle);
	bool IsValid(const Handle& handle);
	Resource* GetResource(const Handle& handle);
	Handle FindHandleByKey(const wstring& key);
#pragma endregion

#pragma region Slot Management
private:
	Handle FindHandle(uint64 hash);
	Handle AddResourceInternal(uint64 hash, Resource* resource);
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
#include "ResourceManager.inl"
END