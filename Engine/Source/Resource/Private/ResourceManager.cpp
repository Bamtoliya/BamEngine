#pragma once
#include "ResourceManager.h"
#include "Resources.h"
#include "PhysicsMaterial.h"
#include "Runtime.h"

namespace
{
	// ResourceManager 재생성 시에도 초기화하지 않는다.
	uint64 g_NextResourceGeneration = 1;

	std::filesystem::path ResolveResourceLoadPath(const std::filesystem::path& filePath)
	{
		return Engine::PlatformPath::ResolveUri(filePath);
	}

	std::wstring ToProjectRelativePath(const std::wstring& inputPath)
	{
		if (Engine::PlatformPath::IsVirtualUri(inputPath))
			return NormalizePath(inputPath);

		const auto uri = Engine::PlatformPath::ToVirtualUri(inputPath);
		if (!uri.empty() && Engine::PlatformPath::IsVirtualUri(uri))
			return NormalizePath(uri);

		if (Engine::PakMountManager::Get().GetMountedPakCount() > 0)
		{
			const std::string pakUri = Engine::PakMountManager::ToPakUri(inputPath);
			if (!pakUri.empty())
				return NormalizePath(Engine::Utf8ToPath(pakUri).wstring());
		}

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

	std::wstring_view LastPathComponent(std::wstring_view path)
	{
		while (!path.empty() && (path.back() == L'/' || path.back() == L'\\'))
			path.remove_suffix(1);
		const auto separator = path.find_last_of(L"/\\");
		return separator == std::wstring_view::npos ? path : path.substr(separator + 1);
	}

	// 두 경로의 파일 이름이 다르다고 확실할 때만 false. 경로 해석은 파일 이름을 바꾸지 않으므로,
	// 비용이 큰 ResolveUri·정규화 전에 대부분의 리소스를 걸러낸다. 비ASCII 문자는 판정하지 않고 정확한 비교에 맡긴다.
	bool MayHaveSameFileName(std::wstring_view lhs, std::wstring_view rhs)
	{
		lhs = LastPathComponent(lhs);
		rhs = LastPathComponent(rhs);
		const auto isOrdinary = [](std::wstring_view name) { return !name.empty() && name != L"." && name != L".."; };
		if (!isOrdinary(lhs) || !isOrdinary(rhs))
			return true;
		if (lhs.size() != rhs.size())
			return false;
		const auto foldAscii = [](wchar_t c) { return c >= L'A' && c <= L'Z' ? static_cast<wchar_t>(c - L'A' + L'a') : c; };
		for (size_t i = 0; i < lhs.size(); ++i)
		{
			if (lhs[i] > 0x7F || rhs[i] > 0x7F)
				continue;
			if (foldAscii(lhs[i]) != foldAscii(rhs[i]))
				return false;
		}
		return true;
	}

	bool IsProjectOrMountScoped(const Engine::Resource& resource)
	{
		const std::wstring& key = resource.GetKey();
		const std::wstring& path = resource.GetPath();

		if (Engine::PlatformPath::IsEngineUri(key) || Engine::PlatformPath::IsVirtualSchemeUri(key, L"Editor") || Engine::PlatformPath::IsPluginUri(key))
			return false;
		if (Engine::PlatformPath::IsEngineUri(path) || Engine::PlatformPath::IsVirtualSchemeUri(path, L"Editor") || Engine::PlatformPath::IsPluginUri(path))
			return false;

		if (Engine::PlatformPath::IsProjectUri(key) || Engine::PlatformPath::IsMountUri(key))
			return true;
		if (Engine::PlatformPath::IsProjectUri(path) || Engine::PlatformPath::IsMountUri(path))
			return true;

		if (!path.empty())
		{
			const std::wstring virtualPath = Engine::PlatformPath::ToVirtualUri(Engine::PlatformPath::ResolveUri(path));
			if (Engine::PlatformPath::IsEngineUri(virtualPath) || Engine::PlatformPath::IsVirtualSchemeUri(virtualPath, L"Editor") ||
				Engine::PlatformPath::IsPluginUri(virtualPath))
				return false;
			if (Engine::PlatformPath::IsProjectUri(virtualPath) || Engine::PlatformPath::IsMountUri(virtualPath))
				return true;
		}

		return false;
	}
}

bool Engine::ResourceHandleAccess::TryAddRef(const Engine::Handle& handle, TypeValidator validator)
{
	if (!Engine::ResourceManager::IsCreated())
		return false;

	return Engine::ResourceManager::Get().TryAddRefResource(handle, validator);
}

bool Engine::ResourceHandleAccess::AddRef(const Engine::Handle& handle)
{
	if (!Engine::ResourceManager::IsCreated())
		return false;

	return Engine::ResourceManager::Get().AddRefResource(handle);
}

void Engine::ResourceHandleAccess::Release(const Engine::Handle& handle)
{
	if (Engine::ResourceManager::IsCreated())
		Engine::ResourceManager::Get().ReleaseResource(handle);
}

bool Engine::ResourceHandleAccess::IsValid(const Engine::Handle& handle)
{
	if (!Engine::ResourceManager::IsCreated())
		return false;

	return Engine::ResourceManager::Get().IsValid(handle);
}

Engine::Resource* Engine::ResourceHandleAccess::Resolve(const Engine::Handle& handle)
{
	if (!Engine::ResourceManager::IsCreated())
		return nullptr;

	return Engine::ResourceManager::Get().GetResource(handle);
}

IMPLEMENT_SINGLETON(ResourceManager)

#pragma region Constructor&Destructor
EResult ResourceManager::Initialize(void* arg)
{
	// 초기 용량 설정 (필요에 따라 조정)
	m_OwnerThread = std::this_thread::get_id();
	m_Resources.reserve(100);
	RegisterExplicitLoader();
	return EResult::Success;
}

void ResourceManager::Free()
{
	ReleaseAllResources(true);
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
	if (!IsOwnerThread() || IsShuttingDown())
		return {};

	namespace fs = std::filesystem;

	Handle existing = FindHandleByKey(filePath);
	if (existing.IsValid())
		return existing;

	const wstring relativePath = ToProjectRelativePath(filePath);
	if (!relativePath.empty() && relativePath != filePath)
	{
		existing = FindHandleByKey(relativePath);
		if (existing.IsValid())
			return existing;
	}

	fs::path absolutePath = PlatformPath::ResolveUri(
		(!relativePath.empty() && PlatformPath::IsVirtualUri(relativePath)) ? relativePath : filePath);
	existing = FindHandleByFilePath(absolutePath.wstring());
	if (existing.IsValid())
		return existing;

	if (!PakMountManager::Get().ExistsOrOnDisk(relativePath.empty() ? absolutePath : fs::path(relativePath)))
	{
		ENGINE_LOG_ERROR("Resource load failed: stage=file not found, key=\"{}\", path=\"{}\"", Engine::Unicode::WideToUtf8(relativePath), Engine::Unicode::WideToUtf8(absolutePath.wstring()));
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
		if (embeddedExtension.rfind(L".bam", 0) == 0 || embeddedExtension == L".actiontimeline")
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
		Engine::Unicode::WideToUtf8(relativePath), Engine::Unicode::WideToUtf8(absolutePath.wstring()), Engine::Unicode::WideToUtf8(logicalExtension));
	return {};
}
Handle ResourceManager::LoadFile(const string& filePath)
{
	return LoadFile(Engine::Unicode::Utf8ToWide(filePath));
}
void ResourceManager::RegisterExplicitLoader()
{
	const auto registerSimpleLoader = [this]<typename ResourceType, typename Desc>(
		std::initializer_list<const wchar_t*> extensions)
	{
		const auto loader = [this](wstring key, wstring path) -> Handle
			{
				Desc desc;
				desc.Key = key;
				desc.Path = path.empty() ? key : path;
				return this->LoadResource<ResourceType>(&desc).GetRawHandle();
			};

		for (const wchar_t* extension : extensions)
			m_LoaderRegistry[extension] = loader;
	};

	registerSimpleLoader.operator() < Texture, TextureCreateDesc > ({ L".png", L".jpg", L".bmp", L".tga", L".jpeg", L".bamtex", L".dds", L".hdr" });
	registerSimpleLoader.operator() < Sprite, SpriteCreateDesc > ({ L".bamsprite" });
	registerSimpleLoader.operator() < Material, MaterialDesc > ({ L".bammat" });
	registerSimpleLoader.operator() < MaterialInstance, MaterialInstanceDesc > ({ L".bammatinst" });
	registerSimpleLoader.operator() < Mesh, MeshCreateDesc > ({ L".bammesh" });
	registerSimpleLoader.operator() < Skeleton, SkeletonCreateDesc > ({ L".bamskel", L".bamskeleton" });
	registerSimpleLoader.operator() < Animation, AnimationCreateDesc > ({ L".bamanim" });
	registerSimpleLoader.operator() < Model, ModelCreateDesc > ({ L".bammodel" });
	registerSimpleLoader.operator() < AudioClip, AudioClipCreateDesc > ({ L".wav", L".mp3", L".ogg" });
	registerSimpleLoader.operator() < ParticleEffect, ParticleEffectCreateDesc > ({ L".bameffect", L".effect" });
	registerSimpleLoader.operator() < SpriteAnimation, SpriteAnimationCreateDesc > ({ L".spriteanim", L".bamspriteanim" });
	registerSimpleLoader.operator() < ActionTimelineAsset, ActionTimelineCreateDesc > ({ L".bamtimeline", L".actiontimeline" });
	registerSimpleLoader.operator() < CompositeEffect, CompositeEffectCreateDesc > ({ L".bamvfx", L".vfx" });
	registerSimpleLoader.operator() < TerrainLayerAsset, TerrainLayerCreateDesc > ({ L".bamterrainlayer" });
	m_LoaderRegistry[L".bamphysmat"] = [this](wstring key, wstring path) -> Handle
		{
			PhysicsMaterialCreateDesc desc;
			desc.Key = key;
			desc.Path = path.empty() ? key : path;
			if (auto* mat = PhysicsMaterial::Create(&desc))
				return this->AddResource<PhysicsMaterial>(key, mat).GetRawHandle();
			return Handle{};
		};

	const auto shaderLoader = [this](wstring key, wstring path) -> Handle
		{
			const std::wstring effectivePath = path.empty() ? key : path;
			const std::filesystem::path filePath(effectivePath);
			wstring ext = filePath.extension().wstring();
			std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);

			ShaderDesc desc;
			desc.Key = key;
			desc.Path = effectivePath;

			if (ext == L".cso" || ext == L".dxil" || ext == L".spv" || ext == L".wgsl")
			{
				wstring stem = filePath.stem().wstring();
				std::transform(stem.begin(), stem.end(), stem.begin(), ::towupper);
				if (stem.ends_with(L"VS") || stem.ends_with(L"_VS")) desc.shaderType = EShaderType::Vertex;
				else if (stem.ends_with(L"PS") || stem.ends_with(L"_PS")) desc.shaderType = EShaderType::Fragment;
				else if (stem.ends_with(L"CS") || stem.ends_with(L"_CS")) desc.shaderType = EShaderType::Compute;

				if (ext == L".spv") desc.spirvPath = effectivePath;
				else if (ext == L".wgsl") desc.wgslPath = effectivePath;
				else desc.dxilPath = effectivePath;
			}

			return this->LoadResource<Shader>(&desc).GetRawHandle();
		};

	for (const wchar_t* extension : { L".bamshader", L".cso", L".dxil", L".spv", L".wgsl" })
		m_LoaderRegistry[extension] = shaderLoader;
}
Handle ResourceManager::RegisterPreparedResource(Resource* resource)
{
	if (!resource || !IsOwnerThread())
		return {};

	const wstring key = NormalizePath(resource->GetKey());

	RegistrationDecision decision;

	{
		unique_lock lock(m_PoolMutex);
		decision = CheckRegistrationLocked(key, resource);
	}

	if (decision.Action != ERegistrationAction::Insert)
		return CompleteRegistrationDecision(resource, key, decision);

	// 실제 GPU 업로드는 매니저 잠금 밖에서 수행한다.
	if (resource->UploadToGPU() != EResult::Success)
	{
		resource->Free();
		delete resource;

		return {};
	}

	// 업로드 중 다른 신규 객체가 같은 키로 등록될 수 있으므로
	// 내부 등록 단계에서 다시 검사한다.
	return AddResourceInternal(key, resource);
}

EResult ResourceManager::DestroyResource(const Handle& handle)
{
	if (!handle.IsValid() || !IsOwnerThread())
		return EResult::InvalidArgument;

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
	if (!resource || !IsOwnerThread())
		return EResult::InvalidArgument;

	Resource* toDelete = nullptr;

	{
		unique_lock lock(m_PoolMutex);

		if (m_IsShuttingDown)
			return EResult::Fail;

		const auto it = m_PointerToHandle.find(resource);

		if (it == m_PointerToHandle.end())
			return EResult::Fail;

		toDelete = FreeSlotInternal(it->second.GetIndex());
	}

	if (!toDelete)
		return EResult::Fail;

	toDelete->Free();
	delete toDelete;

	return EResult::Success;
}
EResult ResourceManager::DestroyUnregisteredResource(Resource* resource)
{
	if (!resource)
		return EResult::InvalidArgument;

	if (resource->IsManagerOwned())
		return EResult::Fail;

	// Free() 중 재등록 및 재귀적인 삭제를 방지한다.
	resource->m_ManagerOwnership.Owned = true;

	resource->Free();
	delete resource;

	return EResult::Success;
}
uint32 ResourceManager::FlushDeferredDeletes()
{
	if (!IsOwnerThread())
		return 0;

	vector<Resource*> pending;

	{
		unique_lock lock(m_PoolMutex);

		pending.swap(m_DeferredDeletes);
	}

	for (Resource* resource : pending)
	{
		if (!resource)
			continue;

		resource->Free();
		delete resource;
	}

	return static_cast<uint32>(pending.size());
}
uint32 ResourceManager::UnloadProjectScopedResources()
{
	if (!IsOwnerThread())
		return 0;

	vector<Resource*> toDelete;
	{
		unique_lock lock(m_PoolMutex);
		for (uint32 i = 0; i < static_cast<uint32>(m_Resources.size()); ++i)
		{
			const ResourceSlot& slot = m_Resources[i];
			if (!slot.IsActive || !slot.Instance)
				continue;

			if (IsProjectOrMountScoped(*slot.Instance))
			{
				if (Resource* freed = FreeSlotInternal(i))
					toDelete.push_back(freed);
			}
		}
	}

	for (Resource* res : toDelete)
	{
		if (res)
		{
			res->Free();
			delete res;
		}
	}

	const uint32 unloadedCount = static_cast<uint32>(toDelete.size());
	ENGINE_LOG_INFO("[ResourceManager] Unloaded {} project/mount-scoped resources.", unloadedCount);
	return unloadedCount;
}
void ResourceManager::ClearResources()
{
	ReleaseAllResources(false);
}
#if BAM_EDITOR
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

		info.HasManagerReference = slot.HasManagerReference;
		info.CacheEvictable = slot.CacheEvictable;

		const uint32 managerRefCount = slot.HasManagerReference ? 1u : 0u;

		info.ExternalRefCount = slot.RefCount >= managerRefCount
			? slot.RefCount - managerRefCount
			: 0u;

		if (info.ExternalRefCount == 0 && slot.HasManagerReference)
		{
			info.IdleSeconds = std::chrono::duration<f64>(
				std::chrono::steady_clock::now() - slot.IdleSince).count();
		}

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

	metrics.DeferredDeleteCount =
		static_cast<uint64>(m_DeferredDeletes.size());

	metrics.TotalSlots =
		static_cast<uint64>(m_Resources.size());

	metrics.FreeSlots =
		static_cast<uint64>(m_FreeSlots.size());

	for (const auto& slot : m_Resources)
	{
		if (!slot.IsActive || !slot.Instance)
			continue;

		++metrics.ActiveResources;

		const uint32 managerRefCount = slot.HasManagerReference ? 1u : 0u;

		if (slot.RefCount > managerRefCount)
			++metrics.ReferencedResources;

		if (slot.HasManagerReference && slot.RefCount == 1)
		{
			++metrics.ManagerOnlyResources;

			if (slot.CacheEvictable)
				++metrics.CacheEvictableResources;
		}

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

#pragma region Dependencies
ResourceDependencySnapshot ResourceManager::GetDependencySnapshot()
{
	ResourceDependencySnapshot snapshot;

	if (!IsOwnerThread())
		return snapshot;

	vector<Handle> handles;

	{
		shared_lock lock(m_PoolMutex);

		if (m_IsShuttingDown)
			return snapshot;

		handles.reserve(m_Resources.size());

		for (size_t i = 0; i < m_Resources.size(); ++i)
		{
			const auto& slot = m_Resources[i];

			if (!slot.IsActive || !slot.Instance)
				continue;

			handles.emplace_back(
				static_cast<uint32>(i), slot.Generation);
		}
	}

	snapshot.Nodes.reserve(handles.size());

	for (const Handle& handle : handles)
	{
		const Resource* resource = GetResource(handle);

		if (!resource)
			continue;

		ResourceDependencyNode node;

		node.Id = handle;
		node.Key = resource->GetKey();

		node.TrackingSupported =
			resource->CollectResourceDependencies(node.Dependencies);

		// 동일 Texture가 여러 슬롯에서 사용되는 경우에도
		// 하나의 의존성 관계로 표현한다.
		std::sort(node.Dependencies.begin(), node.Dependencies.end());

		node.Dependencies.erase(
			std::unique(node.Dependencies.begin(), node.Dependencies.end()),
			node.Dependencies.end());

		snapshot.Nodes.push_back(std::move(node));
	}

	return snapshot;
}
#pragma endregion


#pragma region Handle Management
bool ResourceManager::TryAddRefResource(const Handle& handle, ResourceHandleAccess::TypeValidator validator)
{
	if (!handle.IsValid() || !validator)
		return false;

	unique_lock lock(m_PoolMutex);

	if (m_IsShuttingDown)
		return false;

	const uint32 index = handle.GetIndex();

	if (index >= m_Resources.size())
		return false;

	auto& slot = m_Resources[index];

	if (!slot.IsActive ||
		slot.Generation != handle.GetGeneration() ||
		!slot.Instance)
	{
		return false;
	}

	if (slot.RefCount == 0 ||
		slot.RefCount == (std::numeric_limits<uint32>::max)())
	{
		return false;
	}

	if (!validator(slot.Instance))
		return false;

	++slot.RefCount;

	return true;
}
bool ResourceManager::AddRefResource(const Handle& handle)
{
	return TryAddRefResource(handle,
		[](const Resource*) noexcept
		{
			return true;
		});
}

void ResourceManager::ReleaseResource(const Handle& handle)
{
	ReleaseReference(handle, false);
}
bool ResourceManager::ReleaseManagerReference(const Handle& handle)
{
	if (!IsOwnerThread())
		return false;
	return ReleaseReference(handle, true);
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
	if (!handle.IsValid() || !IsOwnerThread())
		return nullptr;

	shared_lock lock(m_PoolMutex);

	const uint32 index = handle.GetIndex();

	if (index >= m_Resources.size())
		return nullptr;

	const auto& slot = m_Resources[index];

	if (!slot.IsActive ||
		slot.Generation != handle.GetGeneration() ||
		!slot.Instance ||
		slot.RefCount == 0)
	{
		return nullptr;
	}

	return slot.Instance;
}
Handle ResourceManager::FindHandleByKey(const wstring& key)
{
	return FindHandle(NormalizePath(key));
}
wstring ResourceManager::ResolveFileKey(const std::filesystem::path& filePath)
{
	if (filePath.empty())
		return {};

	return ToProjectRelativePath(ResolveResourceLoadPath(filePath).wstring());
}
Handle ResourceManager::FindHandleByFilePath(const std::filesystem::path& filePath)
{
	const auto key = ResolveFileKey(filePath);
	if (key.empty())
		return {};
	const auto keyed = FindHandleByKey(key);
	if (keyed.IsValid())
		return keyed;
	// A resource can have a semantic key unrelated to its backing file name.
	const auto physical = PlatformPath::ResolveUri(filePath);
	const wstring physicalName = physical.wstring();
	shared_lock lock(m_PoolMutex);
	for (size_t index = 0; index < m_Resources.size(); ++index)
	{
		const auto& slot = m_Resources[index];
		if (slot.IsActive && slot.Instance && !slot.Instance->GetPath().empty() &&
			MayHaveSameFileName(physicalName, slot.Instance->GetPath()) &&
			PlatformPath::IsSamePath(physical, PlatformPath::ResolveUri(slot.Instance->GetPath())))
			return Handle(static_cast<uint32>(index), slot.Generation);
	}
	return {};
}
bool ResourceManager::RenameResourceKey(const Handle& handle, const wstring& newKey)
{
	if (!handle.IsValid() || !IsOwnerThread())
		return false;

	wstring normalizedKey = NormalizePath(newKey);

	if (normalizedKey.empty())
		return false;

	wstring registeredKey = normalizedKey;

	unique_lock lock(m_PoolMutex);

	if (m_IsShuttingDown)
		return false;

	const uint32 index = handle.GetIndex();

	if (index >= m_Resources.size())
		return false;

	auto& slot = m_Resources[index];

	if (!slot.IsActive ||
		slot.Generation != handle.GetGeneration() ||
		!slot.Instance)
	{
		return false;
	}

	if (slot.RegisteredKey == normalizedKey)
		return true;

	if (m_KeyToHandle.contains(normalizedKey))
		return false;

	// 신규 인덱스 등록을 먼저 완료한다.
	const auto [it, inserted] =
		m_KeyToHandle.emplace(normalizedKey, handle);

	if (!inserted)
		return false;

	// 기존 인덱스 제거 및 식별자 변경
	m_KeyToHandle.erase(slot.RegisteredKey);

	slot.RegisteredKey.swap(registeredKey);
	slot.Instance->ApplyKey(std::move(normalizedKey));

	return true;
}
#pragma endregion

#pragma region Resource Slot Management
bool ResourceManager::IsShuttingDown()
{
	shared_lock lock(m_PoolMutex);
	return m_IsShuttingDown;
}
void ResourceManager::ReleaseAllResources(bool shutdown)
{
	if (!IsOwnerThread())
		return;

	vector<ResourceSlot> retiredResources;
	vector<Resource*> deferredResources;

	{
		unique_lock lock(m_PoolMutex);

		if (m_IsShuttingDown)
			return;

		m_IsShuttingDown = true;

		retiredResources.swap(m_Resources);
		deferredResources.swap(m_DeferredDeletes);

		m_FreeSlots.clear();
		m_KeyToHandle.clear();
		m_PointerToHandle.clear();
		m_TypeToHandles.clear();

		if (shutdown)
			m_LoaderRegistry.clear();
	}

	for (Resource* resource : deferredResources)
	{
		if (!resource)
			continue;

		resource->Free();
		delete resource;
	}

	for (auto& slot : retiredResources)
	{
		if (!slot.Instance)
			continue;

		Resource* resource = slot.Instance;
		slot.Instance = nullptr;

		resource->Free();
		delete resource;
	}

	if (!shutdown)
	{
		unique_lock lock(m_PoolMutex);
		m_IsShuttingDown = false;
	}
}

bool ResourceManager::SetCacheEvictable(const Handle& handle, bool evictable)
{
	if (!IsOwnerThread() || !handle.IsValid())
		return false;

	unique_lock lock(m_PoolMutex);

	if (m_IsShuttingDown)
		return false;

	const uint32 index = handle.GetIndex();

	if (index >= m_Resources.size())
		return false;

	auto& slot = m_Resources[index];

	if (!slot.IsActive ||
		slot.Generation != handle.GetGeneration() ||
		!slot.Instance ||
		!slot.HasManagerReference)
	{
		return false;
	}

	if (slot.CacheEvictable != evictable)
	{
		slot.CacheEvictable = evictable;
		slot.IdleSince = std::chrono::steady_clock::now();
	}

	return true;
}

uint32 ResourceManager::CollectUnusedResources(
	uint32 maxCount, std::chrono::milliseconds minIdle)
{
	if (!IsOwnerThread() || maxCount == 0 || m_IsCollectingCache)
		return 0;

	if (minIdle < std::chrono::milliseconds::zero())
		minIdle = std::chrono::milliseconds::zero();

	m_IsCollectingCache = true;

	struct CollectionGuard
	{
		bool& Collecting;

		~CollectionGuard()
		{
			Collecting = false;
		}
	} guard{ m_IsCollectingCache };

	using Clock = std::chrono::steady_clock;

	struct Candidate
	{
		Handle Id;
		Clock::time_point IdleSince;
	};

	const auto now = Clock::now();

	vector<Candidate> candidates;

	const auto canEvict = [&](const ResourceSlot& slot)
		{
			return slot.IsActive &&
				slot.Instance &&
				slot.HasManagerReference &&
				slot.RefCount == 1 &&
				slot.CacheEvictable &&
				slot.Instance->GetStreamSlot() == 0 &&
				now - slot.IdleSince >= minIdle;
		};

	{
		shared_lock lock(m_PoolMutex);

		if (m_IsShuttingDown)
			return 0;

		for (size_t i = 0; i < m_Resources.size(); ++i)
		{
			const auto& slot = m_Resources[i];

			if (!canEvict(slot))
				continue;

			candidates.push_back({
				Handle(static_cast<uint32>(i), slot.Generation),
				slot.IdleSince
				});
		}
	}

	std::sort(candidates.begin(), candidates.end(),
		[](const Candidate& lhs, const Candidate& rhs)
		{
			if (lhs.IdleSince != rhs.IdleSince)
				return lhs.IdleSince < rhs.IdleSince;

			return lhs.Id < rhs.Id;
		});

	uint32 collected = 0;

	for (const auto& candidate : candidates)
	{
		if (collected >= maxCount)
			break;

		Resource* toDelete = nullptr;

		{
			unique_lock lock(m_PoolMutex);

			if (m_IsShuttingDown)
				break;

			const uint32 index = candidate.Id.GetIndex();

			if (index >= m_Resources.size())
				continue;

			const auto& slot = m_Resources[index];

			// 후보 수집 이후 참조 상태가 변경되었을 수 있다.
			if (slot.Generation != candidate.Id.GetGeneration() ||
				slot.IdleSince != candidate.IdleSince ||
				!canEvict(slot))
			{
				continue;
			}

			toDelete = FreeSlotInternal(index);
		}

		if (toDelete)
		{
			toDelete->Free();
			delete toDelete;

			++collected;
		}
	}

	return collected;
}

ResourceManager::RegistrationDecision ResourceManager::CheckRegistrationLocked(const wstring& key, Resource* resource)
{
	using Action = ERegistrationAction;

	if (auto it = m_PointerToHandle.find(resource);
		it != m_PointerToHandle.end())
	{
		const Handle handle = it->second;
		const auto& slot = m_Resources[handle.GetIndex()];

		if (!m_IsShuttingDown && slot.RegisteredKey == key)
			return { Action::ReturnExisting, handle };

		return { Action::RejectOwned, {} };
	}

	// 이미 등록됐거나 삭제 대기 중인 객체는
	// 신규 객체로 취급해서는 안 된다.
	if (resource->IsManagerOwned())
		return { Action::RejectOwned, {} };

	if (m_IsShuttingDown || key.empty())
		return { Action::RejectNew, {} };

	if (auto it = m_KeyToHandle.find(key);
		it != m_KeyToHandle.end())
	{
		const Handle existing = it->second;
		const auto* instance = m_Resources[existing.GetIndex()].Instance;

		if (instance && typeid(*instance) == typeid(*resource))
			return { Action::DiscardDuplicate, existing };

		return { Action::RejectNew, {} };
	}

	return { Action::Insert, {} };
}

Handle ResourceManager::CompleteRegistrationDecision(
	Resource* resource,
	const wstring& key,
	const RegistrationDecision& decision)
{
	using Action = ERegistrationAction;

	switch (decision.Action)
	{
	case Action::ReturnExisting:
		return decision.Existing;

	case Action::DiscardDuplicate:
		DestroyUnregisteredResource(resource);
		return decision.Existing;

	case Action::RejectNew:
		ENGINE_LOG_ERROR(
			"Resource registration rejected: key={}",
			Engine::Unicode::WideToUtf8(key));

		DestroyUnregisteredResource(resource);
		return {};

	case Action::RejectOwned:
		ENGINE_LOG_ERROR(
			"Resource already owned or being destroyed: key={}",
			Engine::Unicode::WideToUtf8(key));

		// 이미 매니저 소유인 객체는 삭제하지 않는다.
		return {};

	default:
		return {};
	}
}

Handle ResourceManager::FindHandle(const wstring& normalizedKey)
{
	shared_lock lock(m_PoolMutex);

	const auto it = m_KeyToHandle.find(normalizedKey);

	return it != m_KeyToHandle.end()
		? it->second : Handle{};
}

Handle ResourceManager::FindHandleByResource(Resource* resource)
{
	if (!resource)
		return {};

	shared_lock lock(m_PoolMutex);

	const auto it = m_PointerToHandle.find(resource);

	return it != m_PointerToHandle.end()
		? it->second : Handle{};
}


Handle ResourceManager::AddResourceInternal(wstring key, Resource* resource, bool assignDefaultPath)
{
	if (!resource || !IsOwnerThread())
		return {};

	key = NormalizePath(std::move(key));

	unique_lock lock(m_PoolMutex);

	const auto decision = CheckRegistrationLocked(key, resource);

	if (decision.Action != ERegistrationAction::Insert)
	{
		lock.unlock();

		return CompleteRegistrationDecision(resource, key, decision);
	}

	// 신규 객체로 확정된 이후에만 식별자를 변경한다.
	resource->ApplyKey(key);

	if (assignDefaultPath && resource->GetPath().empty())
		resource->SetPath(key);

	const Handle handle = AllocateSlot(resource);

	if (!handle.IsValid())
	{
		lock.unlock();

		ENGINE_LOG_ERROR("Resource slot allocation failed.");

		resource->Free();
		delete resource;

		return {};
	}

	auto& slot = m_Resources[handle.GetIndex()];

	slot.RegisteredKey = key;

	m_KeyToHandle.emplace(key, handle);
	m_PointerToHandle.emplace(resource, handle);
	m_TypeToHandles[resource->GetTypeID()].push_back(handle);

	resource->m_ManagerOwnership.Owned = true;

	return handle;
}

bool ResourceManager::ReleaseReference(const Handle& handle, bool managerReference)
{
	if (!handle.IsValid())
		return false;

	Resource* toDelete = nullptr;

	{
		unique_lock lock(m_PoolMutex);

		if (m_IsShuttingDown)
			return false;

		const uint32 index = handle.GetIndex();

		if (index >= m_Resources.size())
			return false;

		auto& slot = m_Resources[index];

		if (!slot.IsActive ||
			slot.Generation != handle.GetGeneration() ||
			!slot.Instance ||
			slot.RefCount == 0)
		{
			return false;
		}

		if (managerReference)
		{
			// 매니저의 기본 참조는 한 번만 제거할 수 있다.
			if (!slot.HasManagerReference)
				return false;

			slot.HasManagerReference = false;
		}
		else
		{
			// 매니저가 보유한 기본 참조는
			// 일반적인 핸들 해제로 제거할 수 없다.
			const uint32 minimumRefCount =
				slot.HasManagerReference ? 1u : 0u;

			if (slot.RefCount <= minimumRefCount)
				return false;
		}

		--slot.RefCount;

		if (slot.HasManagerReference && slot.RefCount == 1)
			slot.IdleSince = std::chrono::steady_clock::now();

		if (slot.RefCount == 0)
		{
			if (IsOwnerThread())
			{
				toDelete = FreeSlotInternal(index);
			}
			else
			{
				m_DeferredDeletes.push_back(slot.Instance);

				// 객체 삭제는 지연되지만 핸들은 즉시 무효화한다.
				FreeSlotInternal(index);
			}
		}
	}

	// 다른 ResourceHandle이 해제될 수 있으므로
	// 실제 리소스 삭제는 잠금 밖에서 수행한다.
	if (toDelete)
	{
		toDelete->Free();
		delete toDelete;
	}

	return true;
}

Handle ResourceManager::AllocateSlot(Resource* resource)
{
	constexpr uint64 maxGeneration = (std::numeric_limits<uint32>::max)();

	// Generation 재사용 및 순환 방지
	if (g_NextResourceGeneration >= maxGeneration)
		return {};

	uint32 index;

	if (m_FreeSlots.empty())
	{
		if (m_Resources.size() >= maxGeneration)
			return {};

		index = static_cast<uint32>(m_Resources.size());
		m_Resources.emplace_back();
	}
	else
	{
		index = m_FreeSlots.back();
		m_FreeSlots.pop_back();
	}

	auto& slot = m_Resources[index];

	slot.Instance = resource;
	slot.Generation = static_cast<uint32>(g_NextResourceGeneration++);
	slot.RefCount = 1;
	slot.IsActive = true;
	slot.HasManagerReference = true;
	slot.CacheEvictable = false;
	slot.IdleSince = std::chrono::steady_clock::now();

	return Handle(index, slot.Generation);
}

Resource* ResourceManager::FreeSlotInternal(uint32 slotIndex)
{
	if (slotIndex >= m_Resources.size())
		return nullptr;

	auto& slot = m_Resources[slotIndex];

	if (!slot.IsActive)
		return nullptr;

	Resource* resource = slot.Instance;

	const Handle targetHandle(slotIndex, slot.Generation);

	if (resource)
	{
		// 객체의 현재 Key가 아닌 등록 당시의 Key 사용
		m_KeyToHandle.erase(slot.RegisteredKey);
		m_PointerToHandle.erase(resource);

		const uint64 typeHash = resource->GetTypeID();

		auto typeIt = m_TypeToHandles.find(typeHash);

		if (typeIt != m_TypeToHandles.end())
		{
			auto& handles = typeIt->second;

			auto it = std::find(
				handles.begin(), handles.end(), targetHandle);

			if (it != handles.end())
			{
				*it = handles.back();
				handles.pop_back();
			}

			if (handles.empty())
				m_TypeToHandles.erase(typeIt);
		}
	}

	slot.Instance = nullptr;
	slot.Generation = 0;
	slot.RefCount = 0;
	slot.IsActive = false;
	slot.HasManagerReference = false;
	slot.RegisteredKey.clear();
	slot.CacheEvictable = false;
	slot.IdleSince = {};

	m_FreeSlots.push_back(slotIndex);

	return resource;
}
#pragma endregion

#pragma region Save & Load 
EResult ResourceManager::SaveToJsonFile(Resource* resource, const wstring& filePath)
{
	if (!resource || filePath.empty())
		return EResult::InvalidArgument;
	if (PlatformPath::IsReadOnlyTarget(filePath))
	{
		ENGINE_LOG_ERROR("Resource save failed: target is read-only (Engine:// or system directory), path=\"{}\"", Engine::Unicode::WideToUtf8(filePath));
		return EResult::AccessDenied;
	}
	std::filesystem::path targetPath = PlatformPath::ResolveUri(filePath);
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
			static_cast<int>(result.Code), result.Field, Engine::Unicode::WideToUtf8(filePath));
		return result.Code == archive::ArchiveErrorCode::UnsupportedType
			? EResult::NotImplemented : EResult::Fail;
	}
	return EResult::Success;
}
EResult ResourceManager::SaveToBeveFile(Resource* resource, const wstring& filePath)
{
	if (!resource || filePath.empty())
		return EResult::InvalidArgument;
	if (PlatformPath::IsReadOnlyTarget(filePath))
	{
		ENGINE_LOG_ERROR("Resource save failed: target is read-only (Engine:// or system directory), path=\"{}\"", Engine::Unicode::WideToUtf8(filePath));
		return EResult::AccessDenied;
	}
	std::filesystem::path targetPath = PlatformPath::ResolveUri(filePath);
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
			static_cast<int>(result.Code), result.Field, Engine::Unicode::WideToUtf8(filePath));
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
	ENGINE_LOG_WARN("Legacy binary resource loading is unavailable: {}", Engine::Unicode::WideToUtf8(filePath));
	return {};
}
#pragma endregion