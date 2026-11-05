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
	bool IsSupportedImageExtension(const std::string& ext)
	{
		return ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
			ext == ".tga" || ext == ".bmp" || ext == ".dds";
	}

	bool IsSupportedMaterialExtension(const std::filesystem::path& path)
	{
		std::string filename = path.filename().string();
		for (char& c : filename) c = static_cast<char>(tolower(c));
		return filename.ends_with(".bammat.json") || filename.ends_with(".bammatinst.json") ||
			filename.ends_with(".bammat") || filename.ends_with(".bammatinst");
	}
	bool IsSupportedMeshExtension(const std::filesystem::path& path)
	{
		std::string filename = path.filename().string();
		for (char& c : filename) c = static_cast<char>(tolower(c));
		return filename.ends_with(".bammesh") || filename.ends_with(".bammesh.json");
	}
	bool IsSupportedLocalizationExtension(const std::filesystem::path& path)
	{
		std::string filename = path.filename().string();
		for (char& c : filename) c = static_cast<char>(tolower(c));
		return filename == "korean.json" || filename == "english.json" || filename.ends_with(".csv");
	}
	class TextureFileListener : public efsw::FileWatchListener
	{
	public:
		TextureFileListener(AssetHotReloadManager* manager) : m_Manager(manager) {}
		void handleFileAction(efsw::WatchID watchid, const std::string& dir, const std::string& filename,
			efsw::Action action, std::string oldFilename) override
		{
			if (action == efsw::Actions::Modified || action == efsw::Actions::Add)
			{
				const auto fullPath = std::filesystem::u8path(dir) / std::filesystem::u8path(filename);
				std::string ext = fullPath.extension().string();
				for (char& c : ext) c = static_cast<char>(tolower(c));
				if (IsSupportedImageExtension(ext) || IsSupportedMaterialExtension(fullPath) ||
					IsSupportedMeshExtension(fullPath) || IsSupportedLocalizationExtension(fullPath))
				{
					m_Manager->NotifyFileModified(fullPath);
				}
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
	return Engine::PlatformPath::ResolveProjectRelative("Resources");
}

#pragma region Constructor&Destructor
EResult AssetHotReloadManager::Initialize(void* arg)
{
	m_WatchDirectory = ResolveTextureDirectory();
	std::error_code ec;
	if (!std::filesystem::exists(m_WatchDirectory, ec))
	{
		EDITOR_LOG_WARN("[Texture Hot Reload] Resources directory does not exist: {}", m_WatchDirectory.string());
		return EResult::Success;
	}

	m_FileWatcher = new efsw::FileWatcher();
	m_FileListener = new TextureFileListener(this);
	m_WatchID = m_FileWatcher->addWatch(m_WatchDirectory.string(), m_FileListener, true);
	m_FileWatcher->watch();

	EDITOR_LOG_INFO("[Texture Hot Reload] Watching directory: {}", m_WatchDirectory.string());
	return EResult::Success;
}

void AssetHotReloadManager::Free()
{
	if (m_FileWatcher)
	{
		if (m_WatchID != 0) m_FileWatcher->removeWatch(m_WatchID);
		delete m_FileWatcher;
		m_FileWatcher = nullptr;
	}
	delete m_FileListener;
	m_FileListener = nullptr;
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
	const auto readyFiles = CollectReadyFiles(std::chrono::milliseconds(300));

	for (const auto& file : readyFiles)
	{
		std::string ext = file.extension().string();
		for (char& c : ext)
			c = static_cast<char>(tolower(static_cast<unsigned char>(c)));

		if (IsSupportedImageExtension(ext))
		{
			ProcessModifiedTexture(file);
		}
		else if (IsSupportedMaterialExtension(file))
		{
			ProcessModifiedMaterial(file);
		}
		else if (IsSupportedMeshExtension(file))
		{
			ProcessModifiedMesh(file);
		}
		else if (IsSupportedLocalizationExtension(file))
		{
			ProcessModifiedLocalization(file);
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
	std::string ext = locPath.extension().string();
	for (char& c : ext) c = static_cast<char>(tolower(c));

	// .csv 파일이 수정된 경우 파이썬 변환 스크립트 실행
	if (ext == ".csv")
	{
		std::filesystem::path scriptPath = std::filesystem::current_path() / "Utility/CSVToJsonConverter/localizationcsvtojson.py";
		if (std::filesystem::exists(scriptPath))
		{
			std::string cmd = "python \"" + scriptPath.string() + "\" --input \"Resources/Data/Localization\" --output \"Resources/Data/Localization\"";
			system(cmd.c_str());
		}
	}

	if (IsSuccess(Engine::LocalizationManager::Get().ReloadData()))
	{
		EDITOR_LOG_INFO("[HotReload] Localization reloaded: {}", locPath.filename().string());
		NotificationManager::Get().AddNotification(LOCAL("Notification.LocalizationReloaded"),
			LOCAL_FMT("Notification.ReloadedFile", locPath.filename().string()), ELogLevel::Info, 3.0f);
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
			LOCAL_FMT("Notification.ReloadedFile", meshPath.filename().string()), ELogLevel::Info, 3.0f);
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