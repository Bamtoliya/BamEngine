#pragma once
#include "Resources.h"
#include "ResourceHandle.h"
#include "ResourceMetrics.h"
#include "Logger.h"
#include "PlatformPath.h"
#include "PakArchive.h"
#include <archive/JsonArchive.h>
#include <archive/BeveArchive.h>
#include <cwctype>
#include <functional>
#include <thread>

BEGIN(Engine)

#pragma region Generic Interface
struct ResourceSlot
{
	Resource* Instance = { nullptr };
	uint32 Generation = { 0 };
	uint32 RefCount = { 0 };
	bool IsActive = { false };
};

struct ResourceDebugInfo
{
	uint32 SlotIndex = 0;
	uint32 Generation = 0;
	uint32 RefCount = 0;

	EResourceType Type = EResourceType::Unknown;

	wstring Key;
	wstring Path;
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
	vector<Handle> GetResourceHandles();
	vector<Handle> GetResourceHandles(uint64 typeHash);
public:
	EResult ImportFolder(const wstring& folderPath);
	Handle LoadFile(const string& filePath);
	Handle LoadFile(const wstring& filePath);
	void RegisterExplicitLoader();
	// 작업 스레드가 CPU 데이터까지 읽은 리소스를 GPU에 올려 등록하고 소유권을 가져온다. 메인 스레드 전용.
	// 같은 키가 이미 있으면 기존 핸들을, GPU 생성에 실패하면 빈 핸들을 반환하며 두 경우 모두 resource를 버린다.
	// 키와 경로는 LoadFile이 정하는 값과 같아야 한다(ResolveFileKey).
	Handle RegisterPreparedResource(Resource* resource);
	// 매니저를 만든 스레드인지
	[[nodiscard]] bool IsOwnerThread() const { return std::this_thread::get_id() == m_OwnerThread; }
	EResult DestroyResource(const Handle& handle);
	EResult DestroyResource(Resource* resource);
	// Unloads resources scoped to Project:// or Mount:// while preserving Engine://, Editor://, Plugin://, and built-in resources.
	uint32 UnloadProjectScopedResources();
public:
#if BAM_EDITOR
	vector<ResourceDebugInfo> GetDebugSnapshot();
#else
	inline vector<ResourceDebugInfo> GetDebugSnapshot() { return {}; }
#endif
#pragma endregion


#pragma region Metric
public:
	ResourceMetrics GetMetrics();
#pragma endregion


#pragma region Handle Management
public:
	void AddRefResource(const Handle& handle);
	void ReleaseResource(const Handle& handle);
	bool IsValid(const Handle& handle);
	Resource* GetResource(const Handle& handle);
	Handle FindHandleByKey(const wstring& key);
	static wstring ResolveFileKey(const std::filesystem::path& filePath);
	Handle FindHandleByFilePath(const std::filesystem::path& filePath);
	// Main-thread file transaction. The callback must not call ResourceManager and must undo its own failures.
	// Prepares the full index before calling commitFiles; preserves resource objects, handles and refcounts.
	// Call only while no worker accesses resource properties through previously obtained raw pointers.
	bool RelocateFileResources(const vector<FilePathChange>& changes, const std::function<bool()>& commitFiles);
#pragma endregion

#pragma region Slot Management
private:
	Handle FindHandle(uint64 hash);
	Handle AddResourceInternal(uint64 hash, Resource* resource);
	Handle AllocateSlot(Resource* resource);
	Resource* FreeSlotInternal(uint32 slotIndex);
#pragma endregion

#pragma region Save & Load
public:
	EResult SaveToJsonFile(Resource* resource, const wstring& filePath);
	EResult SaveToBeveFile(Resource* resource, const wstring& filePath);
	EResult SaveToBinaryFile(Resource* resource, const wstring& filePath);
	EResult SaveAllRegisteredResources(bool saveAsBinary = true);
public:
	Handle LoadFromJsonFile(const wstring& filePath);
	Handle LoadFromBeveFile(const wstring& filePath);
	Handle LoadFromBinaryFile(const wstring& filePath);
#pragma endregion

#pragma region Member Variables
private:
	vector<ResourceSlot> m_Resources;
	vector<uint32> m_FreeSlots;
	unordered_map<uint64, Handle> m_HashToHandle;
	unordered_map<uint64, vector<Handle>> m_TypeToHandles;
	shared_mutex m_PoolMutex;
private:
	unordered_map<wstring, function<Handle(wstring, wstring)>> m_LoaderRegistry;
	std::thread::id m_OwnerThread;
#pragma endregion
};

#include "ResourceManager.inl"
END
