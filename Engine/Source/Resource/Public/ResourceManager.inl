#pragma once
#include "ResourceManager.h"

template<typename T, typename... Args>
ResourceHandle<T> ResourceManager::LoadResource(Args&&... args)
{
	static_assert(std::is_base_of_v<Resource, T>, "T must be derived from Resource");

	using FirstArg = std::decay_t<std::tuple_element_t<0, std::tuple<Args...>>>;

	wstring pathStr;
	wstring keyStr;

	auto fail = [&](const char* stage) -> ResourceHandle<T>
		{
			ENGINE_LOG_ERROR("Resource load failed: stage={}, key=\"{}\", path=\"{}\"",
				stage, WStrToStr(keyStr), WStrToStr(pathStr));
			return {};
		};

	auto first = std::get<0>(std::forward_as_tuple(args...));

	if constexpr (std::is_pointer_v<FirstArg> &&
		std::is_base_of_v<ResourceCreateDesc, std::remove_pointer_t<FirstArg>>)
	{
		if (!first) return fail("null descriptor");

		pathStr = !first->Path.empty() ? first->Path : first->Key;
		keyStr = NormalizePath(first->Key.empty() ? pathStr : first->Key);
	}
	else if constexpr (std::is_convertible_v<FirstArg, std::wstring_view>)
	{
		pathStr = wstring(wstring_view(first));
		keyStr = NormalizePath(pathStr);
	}

	uint64 hash = RunTimeHash(keyStr);
	Handle handle = FindHandle(hash);
	if (handle.IsValid())
		return ResourceHandle<T>(handle);

	// 이 아래의 기존 확장자별 로드 코드는 유지

	std::filesystem::path fsPath(pathStr);
	T* resource = nullptr;
	std::string ext = fsPath.extension().string();

	// 3. 파일 확장자에 따른 로드 전략 분기
	if (ext.find(".bam") != std::string::npos)
	{
		BinaryArchive archive(EArchiveMode::Read);
		if (!archive.LoadFromFile(fsPath.string()))
			return fail("binary file read");

		resource = T::CreateEmpty();
		if (!resource)
			return fail("binary resource creation");

		resource->Deserialize(archive);
		if (archive.HasError())
		{
			resource->Free();
			delete static_cast<Resource*>(resource);
			return fail("binary resource deserialization");
		}

		resource->SetKey(keyStr);
		resource->SetPath(pathStr);
	}
	else if (ext.find(".json") != std::string::npos)
	{
		JsonArchive archive(EArchiveMode::Read);
		if (!archive.LoadFromFile(fsPath.string()))
			return fail("json file read");

		resource = T::CreateEmpty();
		if (!resource)
			return fail("json resource creation");

		resource->Deserialize(archive);
		if (archive.HasError())
		{
			resource->Free();
			delete static_cast<Resource*>(resource);
			return fail("json resource deserialization");
		}

		resource->SetKey(keyStr);
		resource->SetPath(pathStr);
	}
	else
	{
		// 일반 소스 리소스 (png, fbx 등)
		resource = T::Create(std::forward<Args>(args)...);
	}

	// 4. 신규 생성 및 등록
	if (!resource)
		return fail("source resource creation");

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
vector<Handle> ResourceManager::GetResourceHandles()
{
	return GetResourceHandles(entt::type_hash<T>::value());
}