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
	ResourceHandle<T> LoadResource(Args&&... args)
	{
		static_assert(std::is_base_of_v<Resource, T>, "T must be derived from Resource");

		// 1. 인자에서 Desc 추출 및 경로 식별
		using FirstArg = std::decay_t<std::tuple_element_t<0, std::tuple<Args...>>>;
		std::wstring_view pathView;

		auto first = std::get<0>(std::forward_as_tuple(args...));

		if constexpr (std::is_pointer_v<FirstArg> &&
			std::is_base_of_v<tagResourceCreateDesc, std::remove_pointer_t<FirstArg>>)
		{
			// Desc 구조체 포인터가 들어온 경우 내부의 Path나 Key를 식별자로 사용
			if (first)
				pathView = !first->Path.empty() ? first->Path : first->Key;
		}
		else if constexpr (std::is_convertible_v<FirstArg, std::wstring_view>)
		{
			// 단순 경로 문자열만 들어온 경우
			pathView = first;
		}

		// 2. 캐시 확인
		uint64 hash = RunTimeHash(first->Key);
		Handle handle = FindHandle(hash);
		if (handle.IsValid())
			return ResourceHandle<T>(handle);

		// 3. 신규 생성 및 등록
		T* resource = T::Create(std::forward<Args>(args)...);
		if (!resource)
			return ResourceHandle<T>();

		handle = AddResourceInternal(hash, resource);
		return ResourceHandle<T>(handle);
	}
	template <typename T>
	ResourceHandle<T> AddResource(const wstring_view& key, T* resource)
	{
		static_assert(is_base_of_v<Resource, T>, "T must be derived from Resource");
		if(!resource) return ResourceHandle<T>();

		uint64 hash = RunTimeHash(key);
		resource->SetKey(key.data());

		return ResourceHandle<T>(AddResourceInternal(hash, resource));
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
END