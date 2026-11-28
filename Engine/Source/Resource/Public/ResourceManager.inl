#pragma once

template<typename T, typename... Args>
ResourceHandle<T> ResourceManager::LoadResource(Args&&... args)
{
	static_assert(std::is_base_of_v<Resource, T>, "T must be derived from Resource");

	if (!IsOwnerThread() || IsShuttingDown())
		return {};

	using FirstArg = std::decay_t<std::tuple_element_t<0, std::tuple<Args...>>>;

	wstring pathStr;
	wstring keyStr;

	auto fail = [&](const char* stage) -> ResourceHandle<T>
		{
			ENGINE_LOG_ERROR("Resource load failed: stage={}, key=\"{}\", path=\"{}\"",
				stage, Engine::Unicode::WideToUtf8(keyStr), Engine::Unicode::WideToUtf8(pathStr));
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

	if (keyStr.empty())
		return fail("empty resource key");

	Handle handle = FindHandle(keyStr);

	if (handle.IsValid())
	{
		ResourceHandle<T> cached(handle);

		if (!cached.IsValid())
			return fail("cached resource type or acquisition failure");

		return cached;
	}

	std::filesystem::path fsPath = PlatformPath::ResolveUri(pathStr);
	wstring extension = fsPath.extension().wstring();
	std::transform(extension.begin(), extension.end(), extension.begin(), ::towlower);
	const auto loadArchive = [&](archive::IArchive& ar) -> ResourceHandle<T>
		{
			auto result = PakMountManager::Get().LoadArchiveWithFallback(ar, fsPath);
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

			resource->SetPath(pathStr);

			const Handle resourceHandle = AddResourceInternal(keyStr, resource);
			return ResourceHandle<T>(resourceHandle);
		};

	if (PakMountManager::Get().GetMountedPakCount() > 0)
	{
		const std::string pakUri = PakMountManager::ToPakUri(fsPath);
		if (const PakEntryInfo* pakEntry = pakUri.empty() ? nullptr : PakMountManager::Get().FindEntry(pakUri))
		{
			if (pakEntry->PayloadFormat == EPakPayloadFormat::Json)
			{
				archive::JsonArchive ar;
				return loadArchive(ar);
			}
			if (pakEntry->PayloadFormat == EPakPayloadFormat::Beve)
			{
				archive::BeveArchive ar;
				return loadArchive(ar);
			}
		}
	}

	if (extension == L".json" || extension == L".spriteanim" || extension == L".actiontimeline" ||
		extension == L".effect" || extension == L".bamvfx" || extension == L".vfx")
	{
		archive::JsonArchive ar;
		return loadArchive(ar);
	}

	if (extension == L".beve" || extension.rfind(L".bam", 0) == 0)
	{
		archive::BeveArchive ar;
		return loadArchive(ar);
	}

	T* resource = T::Create(std::forward<Args>(args)...);
	if (!resource)
		return fail("source resource creation");

	handle = AddResourceInternal(keyStr, resource);

	return ResourceHandle<T>(handle);
}

template<typename T>
ResourceHandle<T> ResourceManager::AddResource(const wstring_view& key, T* resource)
{
	static_assert(std::is_base_of_v<Resource, T>,
		"T must be derived from Resource");

	if (!resource)
		return {};

	return ResourceHandle<T>(
		AddResourceInternal(NormalizePath(wstring(key)), resource, true));
}

template<typename T>
ResourceHandle<T> ResourceManager::GetResourceHandle(const wstring& key)
{
	return ResourceHandle<T>(FindHandleByKey(key));
}

template<typename T>
vector<Handle> ResourceManager::GetResourceHandles()
{
	return GetResourceHandles(entt::type_hash<T>::value());
}