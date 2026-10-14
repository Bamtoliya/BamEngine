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

	std::filesystem::path fsPath(pathStr);
	wstring extension = fsPath.extension().wstring();
	std::transform(extension.begin(), extension.end(), extension.begin(), ::towlower);

	const auto loadArchive = [&](archive::IArchive& ar) -> ResourceHandle<T>
		{
			auto result = ar.LoadFromFile(fsPath);
			if (!result)
				return fail("archive file read");

			T* resource = T::CreateEmpty();
			if (!resource)
				return fail("archive resource creation");

			result = resource->ReadArchive(ar);
			if (!result)
			{
				ENGINE_LOG_ERROR("Resource archive read failed: code={}, field={}",
					static_cast<int>(result.Code), result.Field);

				resource->Free();
				delete static_cast<Resource*>(resource);
				return fail("archive resource deserialization");
			}

			resource->SetKey(keyStr);
			resource->SetPath(pathStr);

			const Handle resourceHandle = AddResourceInternal(hash, resource);
			return ResourceHandle<T>(resourceHandle);
		};

	if (extension == L".json")
	{
		archive::JsonArchive ar;
		return loadArchive(ar);
	}

	if (extension == L".beve")
	{
		archive::BeveArchive ar;
		return loadArchive(ar);
	}

	if (extension.rfind(L".bam", 0) == 0)
		return fail("legacy binary archive is not supported");

	T* resource = T::Create(std::forward<Args>(args)...);
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