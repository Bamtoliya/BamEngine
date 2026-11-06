#pragma once
#include "ResourceManager.h"
#include "Resources.h"
#include "Runtime.h"

namespace
{
	std::wstring ToProjectRelativePath(const std::wstring& inputPath)
	{
		namespace fs = std::filesystem;

		std::error_code ec;
		fs::path absolutePath = fs::weakly_canonical(fs::path(inputPath), ec);
		if (ec)
			absolutePath = fs::path(inputPath);

		fs::path projectRoot = Engine::Runtime::IsCreated() ? Engine::Runtime::Get().GetProjectRoot() : fs::current_path();
		if (ec)
			projectRoot = fs::current_path();

		fs::path relativePath = fs::relative(absolutePath, projectRoot, ec);
		if (ec || relativePath.empty())
			return NormalizePath(absolutePath.wstring());

		return NormalizePath(relativePath.wstring());
	}
}

void Engine::ResourceHandleAccess::AddRef(const Engine::Handle& handle)
{
	Engine::ResourceManager::Get().AddRefResource(handle);
}

void Engine::ResourceHandleAccess::Release(const Engine::Handle& handle)
{
	if (Engine::ResourceManager::IsCreated())
	{
		Engine::ResourceManager::Get().ReleaseResource(handle);
	}
}

bool Engine::ResourceHandleAccess::IsValid(const Engine::Handle& handle)
{
	return Engine::ResourceManager::Get().IsValid(handle);
}

Engine::Resource* Engine::ResourceHandleAccess::Resolve(
	const Engine::Handle& handle)
{
	return Engine::ResourceManager::Get().GetResource(handle);
}

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
			slot.Instance = nullptr;
		}
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

	fs::path absolutePath(filePath);
	if (!absolutePath.is_absolute())
	{
		if (Runtime::IsCreated())
			absolutePath = Runtime::Get().ResolveProjectRelativePath(absolutePath);
		else
			absolutePath = fs::absolute(absolutePath);
	}

	const wstring relativePath = ToProjectRelativePath(absolutePath.wstring());

	if (!fs::exists(absolutePath))
	{
		ENGINE_LOG_ERROR("Resource load failed: stage=file not found, key=\"{}\", path=\"{}\"", WStrToStr(relativePath), WStrToStr(absolutePath.wstring()));
		return {};
	}

	auto ToLowerExt = [](wstring ext)
		{
			std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
			return ext;
		};

	const wstring extension = ToLowerExt(absolutePath.extension().wstring());
	wstring logicalExtension = extension;

	// .bamshader.json 같은 이중 확장자 지원:
	// 라우팅은 .bamshader 로더를 타고, 실제 로드는 원본 .json 경로를 유지한다.
	if (extension == L".json" || extension == L".beve")
	{
		const wstring embeddedExtension = ToLowerExt(absolutePath.stem().extension().wstring());
		if (embeddedExtension.rfind(L".bam", 0) == 0)
			logicalExtension = embeddedExtension;
	}

	auto iter = m_LoaderRegistry.find(logicalExtension);
	if (iter == m_LoaderRegistry.end() && logicalExtension != extension)
	{
		iter = m_LoaderRegistry.find(extension);
	}

	if (iter != m_LoaderRegistry.end())
		return iter->second(relativePath, absolutePath.wstring());

	ENGINE_LOG_ERROR("Resource load failed: stage=loader not registered, key=\"{}\", path=\"{}\", extension=\"{}\"",
		WStrToStr(relativePath), WStrToStr(absolutePath.wstring()), WStrToStr(logicalExtension));
	return {};
}
Handle ResourceManager::LoadFile(const string& filePath)
{
	return LoadFile(StrToWStr(filePath));
}
void ResourceManager::RegisterExplicitLoader()
{
	const auto registerSimpleLoader = [this]<typename ResourceType, typename Desc>(
		std::initializer_list<const wchar_t*> extensions)
	{
		const auto loader = [this](wstring key, wstring) -> Handle
			{
				Desc desc;
				desc.Key = key;
				desc.Path = key;
				return this->LoadResource<ResourceType>(&desc).GetRawHandle();
			};

		for (const wchar_t* extension : extensions)
			m_LoaderRegistry[extension] = loader;
	};

	registerSimpleLoader.operator() < Texture, TextureCreateDesc > ({ L".png", L".jpg", L".bmp", L".tga", L".jpeg", L".bamtex" });
	registerSimpleLoader.operator() < Sprite, SpriteCreateDesc > ({ L".bamsprite" });
	registerSimpleLoader.operator() < Material, MaterialDesc > ({ L".bammat" });
	registerSimpleLoader.operator() < MaterialInstance, MaterialInstanceDesc > ({ L".bammatinst" });
	registerSimpleLoader.operator() < Mesh, MeshCreateDesc > ({ L".bammesh" });
	registerSimpleLoader.operator() < Skeleton, SkeletonCreateDesc > ({ L".bamskel", L".bamskeleton" });
	registerSimpleLoader.operator() < Animation, AnimationCreateDesc > ({ L".bamanim" });
	registerSimpleLoader.operator() < Model, ModelCreateDesc > ({ L".bammodel" });
	registerSimpleLoader.operator() < AudioClip, AudioClipCreateDesc > ({ L".wav", L".mp3", L".ogg" });

	const auto shaderLoader = [this](wstring key, wstring) -> Handle
		{
			const std::filesystem::path filePath(key);
			wstring ext = filePath.extension().wstring();
			std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);

			ShaderDesc desc;
			desc.Key = key;
			desc.Path = key;

			if (ext == L".cso" || ext == L".dxil" || ext == L".spv")
			{
				wstring stem = filePath.stem().wstring();
				std::transform(stem.begin(), stem.end(), stem.begin(), ::towupper);
				if (stem.ends_with(L"VS") || stem.ends_with(L"_VS")) desc.shaderType = EShaderType::Vertex;
				else if (stem.ends_with(L"PS") || stem.ends_with(L"_PS")) desc.shaderType = EShaderType::Fragment;
				else if (stem.ends_with(L"CS") || stem.ends_with(L"_CS")) desc.shaderType = EShaderType::Compute;

				if (ext == L".spv") desc.spirvPath = key;
				else desc.dxilPath = key;
			}

			return this->LoadResource<Shader>(&desc).GetRawHandle();
		};

	for (const wchar_t* extension : { L".bamshader", L".cso", L".dxil", L".spv" })
		m_LoaderRegistry[extension] = shaderLoader;
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
vector<Handle> ResourceManager::GetResourceHandles(uint64 typeHash)
{
	shared_lock lock(m_PoolMutex); // 읽기 락
	auto it = m_TypeToHandles.find(typeHash);
	if (it != m_TypeToHandles.end())
	{
		return it->second; // 값 복사 반환 — 락 보호 하에 복사 완료
	}
	return {};
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
#ifdef BAM_EDITOR
vector<ResourceDebugInfo> ResourceManager::GetDebugSnapshot()
{
	vector<ResourceDebugInfo> result;

	std::shared_lock<std::shared_mutex> lock(m_PoolMutex);

	result.reserve(m_Resources.size());

	for (size_t index = 0; index < m_Resources.size(); ++index)
	{
		const ResourceSlot& slot = m_Resources[index];

		if (!slot.IsActive || !slot.Instance)
			continue;

		ResourceDebugInfo info;
		info.SlotIndex = static_cast<uint32>(index);
		info.Generation = slot.Generation;
		info.RefCount = slot.RefCount;

		// ResourceManager는 Resource의 friend로 선언되어 있다.
		info.Type = slot.Instance->m_ResourceType;
		info.Key = slot.Instance->GetKey();
		info.Path = slot.Instance->GetPath();

		result.push_back(std::move(info));
	}

	return result;
}
#endif
#pragma endregion

#pragma region Metrics
ResourceMetrics ResourceManager::GetMetrics()
{
	ResourceMetrics metrics{};

	std::shared_lock lock(m_PoolMutex);

	metrics.TotalSlots =
		static_cast<uint64>(m_Resources.size());

	metrics.FreeSlots =
		static_cast<uint64>(m_FreeSlots.size());

	for (const auto& slot : m_Resources)
	{
		if (!slot.IsActive || !slot.Instance)
			continue;

		++metrics.ActiveResources;

		const auto typeIndex = static_cast<std::size_t>(
			slot.Instance->m_ResourceType);

		if (typeIndex < metrics.CountsByType.size())
		{
			++metrics.CountsByType[typeIndex];
		}
		else
		{
			++metrics.CountsByType[
				static_cast<std::size_t>(EResourceType::Unknown)];
		}
	}

	return metrics;
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
	if (iter != m_HashToHandle.end())
	{
		return iter->second;
	}
	return Handle();
	// lock은 스코프 종료 시 RAII로 자동 해제
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

	uint64 typeHash = resource->GetTypeID();
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
		uint64 hash = RunTimeHash(NormalizePath(resource->GetKey()));
		m_HashToHandle.erase(hash);

		uint64 typeHash = resource->GetTypeID();
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
	if (!resource || filePath.empty())
		return EResult::InvalidArgument;
	std::filesystem::path targetPath(filePath);
	if (targetPath.is_relative())
		targetPath = PlatformPath::ResolveProjectRelative(targetPath);
	if (targetPath.has_parent_path())
	{
		std::error_code ec;
		std::filesystem::create_directories(targetPath.parent_path(), ec);
	}
	archive::JsonArchive ar;
	auto result = resource->WriteArchive(ar);
	if (result)
		result = ar.SaveToFile(targetPath);
	if (!result)
	{
		ENGINE_LOG_ERROR("JSON resource save failed: code={}, field={}, path={}",
			static_cast<int>(result.Code), result.Field, WStrToStr(filePath));
		return result.Code == archive::ArchiveErrorCode::UnsupportedType
			? EResult::NotImplemented : EResult::Fail;
	}
	return EResult::Success;
}
EResult ResourceManager::SaveToBeveFile(Resource* resource, const wstring& filePath)
{
	if (!resource || filePath.empty())
		return EResult::InvalidArgument;
	std::filesystem::path targetPath(filePath);
	if (targetPath.is_relative())
		targetPath = PlatformPath::ResolveProjectRelative(targetPath);
	if (targetPath.has_parent_path())
	{
		std::error_code ec;
		std::filesystem::create_directories(targetPath.parent_path(), ec);
	}
	archive::BeveArchive ar;
	auto result = resource->WriteArchive(ar);
	if (result)
		result = ar.SaveToFile(targetPath);
	if (!result)
	{
		ENGINE_LOG_ERROR("BEVE resource save failed: code={}, field={}, path={}",
			static_cast<int>(result.Code), result.Field, WStrToStr(filePath));
		return result.Code == archive::ArchiveErrorCode::UnsupportedType
			? EResult::NotImplemented : EResult::Fail;
	}
	return EResult::Success;
}

EResult ResourceManager::SaveToBinaryFile(Resource* resource, const wstring& filePath)
{
	return SaveToBeveFile(resource, filePath);
}

Engine::EResult Engine::ResourceManager::SaveAllRegisteredResources(bool saveAsBinary)
{
	std::shared_lock<std::shared_mutex> lock(m_PoolMutex);
	uint32 savedCount = 0;
	for (size_t index = 0; index < m_Resources.size(); ++index)
	{
		const ResourceSlot& slot = m_Resources[index];
		if (!slot.IsActive || !slot.Instance) continue;
		Resource* res = slot.Instance;
		std::wstring key = res->GetKey();
		if (key.empty()) continue;
		std::filesystem::path outPath(key);
		if (saveAsBinary)
		{
			// 기존 .json 제거 후 바이너리 전용 확장자로 치환
			if (outPath.extension() == ".json") outPath.replace_extension();
			outPath += Engine::GetResourceBinaryExtension(res->m_ResourceType);
		}
		else
		{
			if (outPath.extension() != ".json") outPath += L".json";
		}
		// 부모 디렉터리가 없으면 자동 생성
		if (outPath.has_parent_path())
		{
			std::error_code ec;
			std::filesystem::create_directories(outPath.parent_path(), ec);
		}
		EResult result = saveAsBinary ? SaveToBeveFile(res, outPath.wstring()) : SaveToJsonFile(res, outPath.wstring());
		if (result == EResult::Success)
		{
			savedCount++;
		}
	}
	ENGINE_LOG_INFO("[ResourceManager] Batch save completed: {} resources saved (Format: {}).",
		savedCount, saveAsBinary ? "Binary(BEVE)" : "JSON");
	return EResult::Success;
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
	ENGINE_LOG_WARN("Legacy binary resource loading is unavailable: {}", WStrToStr(filePath));
	return {};
}
#pragma endregion