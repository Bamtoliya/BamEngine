#include "AssetHotReloadManager.h"
#include "ShaderHotReloadManager.h"
#include "LocalizationManager.h"
#include "ResourceManager.h"
#include "Texture.h"
#include "Material.h"
#include "Mesh.h"
#include "MaterialInstance.h"
#include "Logger.h"
#include "NotificationManager.h"
#include "PlatformPath.h"
#include "AssetCache.h"
#include "Functions.h"

namespace
{
	enum class EAssetReloadKind { Unsupported, Texture, Material, Mesh, Localization };

	std::wstring LowerAscii(std::wstring value)
	{
		for (wchar_t& ch : value)
		{
			if (ch >= L'A' && ch <= L'Z')
				ch += L'a' - L'A';
		}
		return value;
	}

	EAssetReloadKind ClassifyAssetReload(const std::filesystem::path& path)
	{
		const auto ext = LowerAscii(path.extension().wstring());
		if (ext == L".png" || ext == L".jpg" || ext == L".jpeg" ||
			ext == L".tga" || ext == L".bmp" || ext == L".dds")
			return EAssetReloadKind::Texture;

		const auto name = LowerAscii(path.filename().wstring());
		if (name.ends_with(L".bammat.json") || name.ends_with(L".bammatinst.json") ||
			name.ends_with(L".bammat") || name.ends_with(L".bammatinst"))
			return EAssetReloadKind::Material;

		if (name.ends_with(L".bammesh") || name.ends_with(L".bammesh.json"))
			return EAssetReloadKind::Mesh;

		if (name == L"korean.json" || name == L"english.json" || name.ends_with(L".csv"))
			return EAssetReloadKind::Localization;

		return EAssetReloadKind::Unsupported;
	}

	class TextureFileListener : public efsw::FileWatchListener
	{
	public:
		explicit TextureFileListener(AssetHotReloadManager* manager) : m_Manager(manager) {}

		void handleFileAction(efsw::WatchID, const std::string& dir, const std::string& filename,
			efsw::Action action, std::string) override
		{
			if (action != efsw::Actions::Modified && action != efsw::Actions::Add)
				return;

			try
			{
				const auto path = Engine::Utf8ToPath(dir) / Engine::Utf8ToPath(filename);
				if (ClassifyAssetReload(path) != EAssetReloadKind::Unsupported)
					m_Manager->NotifyFileModified(path);
			}
			catch (const std::exception& error)
			{
				EDITOR_LOG_WARN("[Asset Hot Reload] Ignored file event: {}, {}", filename, error.what());
			}
			catch (...)
			{
				EDITOR_LOG_WARN("[Asset Hot Reload] Ignored file event: {}", filename);
			}
		}

	private:
		AssetHotReloadManager* m_Manager = nullptr;
	};

	std::wstring AssetFileKey(const std::filesystem::path& filePath)
	{
		if (filePath.empty()) return {};

		const auto absolutePath = Engine::PlatformPath::ResolveProjectRelative(filePath);

		std::error_code ec;
		const auto normalized = std::filesystem::weakly_canonical(absolutePath, ec);
		if (ec) return {};

		auto key = normalized.generic_wstring();
		for (wchar_t& character : key)
			character = static_cast<wchar_t>(std::towlower(character));

		return key;
	}

	bool MatchesResourceFile(const Engine::Resource& resource, const std::wstring& targetKey)
	{
		const auto& path = resource.GetPath().empty() ? resource.GetKey() : resource.GetPath();
		return !path.empty() && AssetFileKey(path) == targetKey;
	}

	void RequestThumbnailRefresh(const std::filesystem::path& changedPath, const Engine::Resource* resource = nullptr)
	{
		if (!Editor::AssetCache::IsCreated()) return;

		auto& cache = Editor::AssetCache::Get();
		cache.RequestInvalidate(changedPath);

		if (resource != nullptr)
		{
			const auto& path = resource->GetPath().empty() ? resource->GetKey() : resource->GetPath();
			if (!path.empty())
				cache.RequestInvalidate(path);
		}
	}
}

std::filesystem::path AssetHotReloadManager::ResolveTextureDirectory()
{
	return Engine::PlatformPath::GetProjectAssetRoot();
}

#pragma region Constructor&Destructor
EResult AssetHotReloadManager::Initialize(void* arg)
{
	m_WatchDirectory = ResolveTextureDirectory();
	m_EngineWatchDirectory = Engine::PlatformPath::GetEngineResourceRoot();
	const auto watchPath = Engine::WStrToStr(m_WatchDirectory.wstring());

	auto watcher = std::make_unique<efsw::FileWatcher>();
	auto listener = std::make_unique<TextureFileListener>(this);

	std::error_code ec;
	if (std::filesystem::is_directory(m_WatchDirectory, ec))
	{
		m_WatchID = watcher->addWatch(watchPath, listener.get(), true);
		if (m_WatchID <= 0)
		{
			EDITOR_LOG_WARN("[Asset Hot Reload] Failed to register watch: {}, code={}", watchPath, m_WatchID);
		}
		else
		{
			EDITOR_LOG_INFO("[Asset Hot Reload] Watching project directory: {}", watchPath);
		}
	}
	else
	{
		EDITOR_LOG_WARN("[Asset Hot Reload] Watch directory unavailable: {}, code={}", watchPath, ec.value());
	}

	if (m_EngineWatchDirectory != m_WatchDirectory && std::filesystem::is_directory(m_EngineWatchDirectory, ec))
	{
		const auto engineWatchPath = Engine::WStrToStr(m_EngineWatchDirectory.wstring());
		m_EngineWatchID = watcher->addWatch(engineWatchPath, listener.get(), true);
		if (m_EngineWatchID <= 0)
		{
			EDITOR_LOG_WARN("[Asset Hot Reload] Failed to register engine watch: {}, code={}", engineWatchPath, m_EngineWatchID);
		}
		else
		{
			EDITOR_LOG_INFO("[Asset Hot Reload] Watching engine directory: {}", engineWatchPath);
		}
	}

	watcher->watch();
	m_FileListener = listener.release();
	m_FileWatcher = watcher.release();

	return EResult::Success;
}

void AssetHotReloadManager::Free()
{
	if (m_FileWatcher)
	{
		if (m_WatchID != 0) m_FileWatcher->removeWatch(m_WatchID);
		if (m_EngineWatchID != 0) m_FileWatcher->removeWatch(m_EngineWatchID);
		delete m_FileWatcher;
		m_FileWatcher = nullptr;
	}
	delete m_FileListener;
	m_FileListener = nullptr;
	m_WatchID = 0;
	m_EngineWatchID = 0;
	m_MeshFileStates.clear();
	__super::Free();
}

AssetHotReloadManager* AssetHotReloadManager::Create(void* arg)
{
	AssetHotReloadManager* instance = new AssetHotReloadManager();
	if (instance->Initialize(arg) != EResult::Success)
	{
		instance->Free();
		delete instance;
		return nullptr;
	}
	return instance;
}
#pragma endregion

void AssetHotReloadManager::Update()
{
	for (const auto& file : CollectReadyFiles(std::chrono::milliseconds(300)))
	{
		switch (ClassifyAssetReload(file))
		{
		case EAssetReloadKind::Texture: ProcessModifiedTexture(file); break;
		case EAssetReloadKind::Material: ProcessModifiedMaterial(file); break;
		case EAssetReloadKind::Mesh: ProcessModifiedMesh(file); break;
		case EAssetReloadKind::Localization: ProcessModifiedLocalization(file); break;
		case EAssetReloadKind::Unsupported: break;
		}
	}
}

void AssetHotReloadManager::ProcessModifiedTexture(const std::filesystem::path& texturePath)
{
	const auto fullPath = Engine::PlatformPath::ResolveProjectRelative(texturePath);
	const auto targetKey = AssetFileKey(fullPath);
	if (targetKey.empty()) return;

	auto& manager = Engine::ResourceManager::Get();
	bool matched = false;

	for (const auto& handle : manager.GetResourceHandles<Engine::Texture>())
	{
		auto* texture = dynamic_cast<Engine::Texture*>(manager.GetResource(handle));
		if (texture == nullptr || !MatchesResourceFile(*texture, targetKey))
			continue;

		matched = true;

		if (!IsSuccess(texture->Reload(fullPath)))
			continue;

		RequestThumbnailRefresh(fullPath, texture);

		NotificationManager::Get().AddNotification(LOCAL("Notification.TextureHotReloaded"),
			LOCAL_FMT("Notification.ReloadedFile", Engine::WStrToStr(fullPath.filename().wstring())),
			ELogLevel::Info, 3.0f);
	}

	// 로드된 리소스 없이 콘텐츠 브라우저 썸네일만 존재할 수도 있습니다.
	if (!matched)
		RequestThumbnailRefresh(fullPath);
}

void AssetHotReloadManager::ProcessModifiedMaterial(const std::filesystem::path& materialPath)
{
	const auto fullPath = Engine::PlatformPath::ResolveProjectRelative(materialPath);
	const auto targetKey = AssetFileKey(fullPath);
	if (targetKey.empty()) return;

	auto& manager = Engine::ResourceManager::Get();
	bool matched = false;

	auto tryReload = [&](const std::vector<Engine::Handle>& handles)
		{
			for (const auto& handle : handles)
			{
				auto* material = dynamic_cast<Engine::MaterialInterface*>(manager.GetResource(handle));
				if (material == nullptr || !MatchesResourceFile(*material, targetKey))
					continue;

				matched = true;

				if (!IsSuccess(material->Reload(fullPath)))
					continue;

				RequestThumbnailRefresh(fullPath, material);

				NotificationManager::Get().AddNotification(LOCAL("Notification.MaterialHotReloaded"),
					LOCAL_FMT("Notification.ReloadedFile", Engine::WStrToStr(fullPath.filename().wstring())),
					ELogLevel::Info, 3.0f);
			}
		};

	tryReload(manager.GetResourceHandles<Engine::Material>());
	tryReload(manager.GetResourceHandles<Engine::MaterialInstance>());

	if (!matched)
		RequestThumbnailRefresh(fullPath);
}

void AssetHotReloadManager::ProcessModifiedLocalization(const std::filesystem::path& locPath)
{
	// .csv 파일이 수정된 경우 파이썬 변환 스크립트 실행
	if (LowerAscii(locPath.extension().wstring()) == L".csv")
	{
		std::error_code ec;
		std::filesystem::path scriptPath = std::filesystem::current_path() / "Tools/Codegen/CSVToJsonConverter/localizationcsvtojson.py";
		if (std::filesystem::exists(scriptPath, ec))
		{
			const auto parentDir = locPath.parent_path();
			std::wstring cmd = L"python \"" + scriptPath.wstring() + L"\" --input \"" + parentDir.wstring() + L"\" --output \"" + parentDir.wstring() + L"\"";
			_wsystem(cmd.c_str());
		}
	}

	if (IsSuccess(Engine::LocalizationManager::Get().ReloadData()))
	{
		EDITOR_LOG_INFO("[HotReload] Localization reloaded: {}", Engine::WStrToStr(locPath.filename().wstring()));
		NotificationManager::Get().AddNotification(LOCAL("Notification.LocalizationReloaded"),
			LOCAL_FMT("Notification.ReloadedFile", Engine::WStrToStr(locPath.filename().wstring())), ELogLevel::Info, 3.0f);
	}
}

void AssetHotReloadManager::ProcessModifiedMesh(const std::filesystem::path& meshPath)
{
	const auto targetKey = AssetFileKey(meshPath);
	if (targetKey.empty())
		return;

	MeshFileState currentState;
	const bool hasState = ReadMeshFileState(meshPath, currentState);
	const auto known = m_MeshFileStates.find(targetKey);

	if (hasState && known != m_MeshFileStates.end() && known->second == currentState)
		return;

	auto& manager = ResourceManager::Get();
	bool matched = false;
	bool allSucceeded = true;

	for (const auto& handle : manager.GetResourceHandles<Mesh>())
	{
		auto* mesh = dynamic_cast<Mesh*>(manager.GetResource(handle));
		if (!mesh)
			continue;

		const auto& resourcePath = mesh->GetPath().empty() ? mesh->GetKey() : mesh->GetPath();
		if (AssetFileKey(std::filesystem::path(resourcePath)) != targetKey)
			continue;

		matched = true;

		if (mesh->Reload(meshPath) != EResult::Success)
		{
			allSucceeded = false;
			continue;
		}

		RequestThumbnailRefresh(meshPath, mesh);
		NotificationManager::Get().AddNotification(LOCAL("Notification.MeshHotReloaded"),
			LOCAL_FMT("Notification.ReloadedFile", Engine::WStrToStr(meshPath.filename().wstring())), ELogLevel::Info, 3.0f);
	}

	if (!matched)
		RequestThumbnailRefresh(meshPath);

	if (matched && allSucceeded && hasState)
		m_MeshFileStates.insert_or_assign(targetKey, currentState);
}

bool AssetHotReloadManager::ReadMeshFileState(const std::filesystem::path& filePath, MeshFileState& state)
{
	std::error_code ec;
	state.WriteTime = std::filesystem::last_write_time(filePath, ec);
	if (ec)
		return false;

	state.Size = std::filesystem::file_size(filePath, ec);
	return !ec;
}

void AssetHotReloadManager::RecordMeshFileState(const std::filesystem::path& filePath)
{
	const auto key = AssetFileKey(filePath);
	if (key.empty())
		return;

	MeshFileState state;
	if (ReadMeshFileState(filePath, state))
		m_MeshFileStates.insert_or_assign(key, state);
	else
		m_MeshFileStates.erase(key);
}