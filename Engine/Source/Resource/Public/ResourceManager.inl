#pragma once
#include "ResourceManager.h"

template<typename T, typename... Args>
ResourceHandle<T> ResourceManager::LoadResource(Args&&... args)
{
	static_assert(std::is_base_of_v<Resource, T>, "T must be derived from Resource");

	// 1. 인자에서 Desc 추출 및 경로 식별
	using FirstArg = std::decay_t<std::tuple_element_t<0, std::tuple<Args...>>>;
	std::wstring pathStr; // wstring_view 대신 wstring으로 변경

	auto first = std::get<0>(std::forward_as_tuple(args...));

	if constexpr (std::is_pointer_v<FirstArg> &&
		std::is_base_of_v<tagResourceCreateDesc, std::remove_pointer_t<FirstArg>>)
	{
		if (first)
			pathStr = !first->Path.empty() ? first->Path : first->Key;
	}
	else if constexpr (std::is_convertible_v<FirstArg, std::wstring_view>)
	{
		pathStr = std::wstring(std::wstring_view(first)); // 명시적 형변환 후 저장
	}

	// 2. 캐시 확인
	uint64 hash = RunTimeHash(NormalizePath(first->Key.empty() ? pathStr : first->Key)); // 정상적으로 const wstring& 으로 매칭됨
	Handle handle = FindHandle(hash);
	if (handle.IsValid())
		return ResourceHandle<T>(handle);

	std::filesystem::path fsPath(pathStr);
	T* resource = nullptr;
	std::string ext = fsPath.extension().string();

	// 3. 파일 확장자에 따른 로드 전략 분기
	if (ext.find(".bam") != std::string::npos)
	{
		resource = T::CreateEmpty();
		if (resource)
		{
			BinaryArchive archive(EArchiveMode::Read); // pathStr 자체를 넘김
			if (archive.LoadFromFile(fsPath.string()))
			{
				resource->Deserialize(archive);
			}
			resource->SetKey(pathStr);
			resource->SetPath(pathStr);
		}
	}
	else if (ext.find(".json") != std::string::npos)
	{
		resource = T::CreateEmpty();
		if (resource)
		{
			JsonArchive archive(EArchiveMode::Read); // pathStr 자체를 넘김
			if (archive.LoadFromFile(fsPath.string()))
			{
				resource->Deserialize(archive);
			}
			resource->SetKey(pathStr);
			resource->SetPath(pathStr);
		}
	}
	else
	{
		// 일반 소스 리소스 (png, fbx 등)
		resource = T::Create(std::forward<Args>(args)...);
	}

	// 4. 신규 생성 및 등록
	if (!resource)
		return ResourceHandle<T>();

	handle = AddResourceInternal(hash, resource);
	return ResourceHandle<T>(handle);
}

template <typename T>
ResourceHandle<T> ResourceManager::AddResource(const wstring_view& key, T* resource)
{
	static_assert(is_base_of_v<Resource, T>, "T must be derived from Resource");
	if (!resource) return ResourceHandle<T>();

	uint64 hash = RunTimeHash(key);
	resource->SetKey(key.data());

	return ResourceHandle<T>(AddResourceInternal(hash, resource));
}

template <typename T>
ResourceHandle<T> ResourceManager::GetResourceHandle(const wstring& key)
{
	uint64 hash = RunTimeHash(key);
	return ResourceHandle<T>(FindHandle(hash));
}

template<typename T>
const vector<Handle>& ResourceManager::GetResourceHandles()
{
	return GetResourceHandles(T::GetStaticTypeInfo().ID);
}