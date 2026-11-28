#pragma once
#include "Resources.h"
#include "ResourceHandle.h"
#include "ResourceDependencySnapshot.h"
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
	Resource* Instance = nullptr;
	uint32 Generation = 0;
	uint32 RefCount = 0;

	bool IsActive = false;
	bool HasManagerReference = false;

	wstring RegisteredKey;

	bool CacheEvictable = false;

	std::chrono::steady_clock::time_point IdleSince{};
};

struct ResourceDebugInfo
{
	uint32 SlotIndex = 0;
	uint32 Generation = 0;
	uint32 RefCount = 0;

	EResourceType Type = EResourceType::Unknown;

	wstring Key;
	wstring Path;

	bool HasManagerReference = false;
	bool CacheEvictable = false;

	uint32 ExternalRefCount = 0;

	f64 IdleSeconds = 0.0;
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
	// 등록된 리소스 강제 삭제.
	// 기존 ResourceHandle은 모두 무효화된다.
	// Owner Thread 전용.
	EResult DestroyResource(const Handle& handle);
	EResult DestroyResource(Resource* resource);

	// 등록되지 않은 객체의 소유권을 전달받아 삭제한다.
	// Worker Thread에서는 CPU 전용 객체에 한해 호출 가능하다.
	static EResult DestroyUnregisteredResource(Resource* resource);

	// Worker Thread에서 마지막 참조가 해제된 리소스를 삭제한다.
	uint32 FlushDeferredDeletes();
	// Unloads resources scoped to Project:// or Mount:// while preserving Engine://, Editor://, Plugin://, and built-in resources.
	uint32 UnloadProjectScopedResources();
public:
	// 모든 리소스를 제거하되 매니저와 로더는 유지한다.
	void ClearResources();
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

#pragma region Dependencies
public:
	ResourceDependencySnapshot GetDependencySnapshot();
#pragma endregion

#pragma region Handle Management
public:
	bool TryAddRefResource(const Handle& handle, ResourceHandleAccess::TypeValidator validator);
	bool AddRefResource(const Handle& handle);
	void ReleaseResource(const Handle& handle);
	bool ReleaseManagerReference(const Handle& handle);
	bool IsValid(const Handle& handle);
	Resource* GetResource(const Handle& handle);
	Handle FindHandleByKey(const wstring& key);
	Handle FindHandleByResource(Resource* resource);
	static wstring ResolveFileKey(const std::filesystem::path& filePath);
	Handle FindHandleByFilePath(const std::filesystem::path& filePath);
	bool RenameResourceKey(const Handle& handle, const wstring& newKey);
#if BAM_EDITOR
	// Main-thread file transaction. The callback must not call ResourceManager and must undo its own failures.
	// Prepares the full index before calling commitFiles; preserves resource objects, handles and refcounts.
	// Call only while no worker accesses resource properties through previously obtained raw pointers.
	bool RelocateFileResources(const vector<FilePathChange>& changes, const std::function<bool()>& commitFiles);
#endif
#pragma endregion

#pragma region Cache Management	
public:
	// 명시적으로 캐시 제거를 허용하거나 금지한다.
	bool SetCacheEvictable(const Handle& handle, bool evictable);

	// 외부 참조가 없고 지정된 시간 이상 사용되지 않은 리소스를 제거한다.
	// Owner Thread의 안전한 정리 시점에서 호출한다.
	uint32 CollectUnusedResources(uint32 maxCount = 64, std::chrono::milliseconds minIdle = std::chrono::seconds(30));
#pragma endregion	

#pragma region Slot Management
private:
	bool IsShuttingDown();
	void ReleaseAllResources(bool shutdown);
private:
	enum class ERegistrationAction
	{
		Insert,
		ReturnExisting,
		DiscardDuplicate,
		RejectNew,
		RejectOwned
	};

	struct RegistrationDecision
	{
		ERegistrationAction Action;
		Handle Existing;
	};

	RegistrationDecision CheckRegistrationLocked(
		const wstring& key, Resource* resource);

	Handle CompleteRegistrationDecision(
		Resource* resource,
		const wstring& key,
		const RegistrationDecision& decision);

	Handle FindHandle(const wstring& normalizedKey);

	Handle AddResourceInternal(wstring key, Resource* resource, bool assignDefaultPath = false);
	bool ReleaseReference(const Handle& handle, bool managerReference);
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
	unordered_map<wstring, Handle> m_KeyToHandle;
	unordered_map<Resource*, Handle> m_PointerToHandle;
	unordered_map<uint64, vector<Handle>> m_TypeToHandles;
	vector<Resource*> m_DeferredDeletes;
	shared_mutex m_PoolMutex;
	bool m_IsShuttingDown = false;
private:
	unordered_map<wstring, function<Handle(wstring, wstring)>> m_LoaderRegistry;
	std::thread::id m_OwnerThread;
	bool m_IsCollectingCache = false;
#pragma endregion
};

#include "ResourceManager.inl"
END
