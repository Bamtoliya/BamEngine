#include "AssetHotReloadManager.h"
#include "ShaderHotReloadManager.h"
#include "LocalizationManager.h"
#include "ResourceManager.h"
#include "Texture.h"
#include "Material.h"
#include "Mesh.h"
#include "MaterialInstance.h"
#include "ActionTimelineAsset.h"
#include "TerrainLayerAsset.h"
#include "Logger.h"
#include "NotificationManager.h"
#include "PlatformPath.h"
#include "AssetCache.h"
#include "Functions.h"
#include <utility>

namespace
{
	enum class EAssetReloadKind { Unsupported, Texture, Material, Mesh, ActionTimeline, TerrainLayer, Localization };

	std::wstring LowerAscii(std::wstring value)
	{
		for (wchar_t& ch : value)
		{
			if (ch >= L'A' && ch <= L'Z')
				ch += L'a' - L'A';
		}
		return value;
	}

	bool IsLocalizationDirectory(const std::filesystem::path& path)
	{
		return LowerAscii(path.filename().wstring()) == L"localization" &&
			LowerAscii(path.parent_path().filename().wstring()) == L"data";
	}

	bool IsLocalizationFile(const std::filesystem::path& path)
	{
		const auto ext = LowerAscii(path.extension().wstring());
		return ext == L".csv" || ext == L".json";
	}

	EAssetReloadKind ClassifyAssetReload(const std::filesystem::path& path)
	{
		const auto ext = LowerAscii(path.extension().wstring());
		if (ext == L".png" || ext == L".jpg" || ext == L".jpeg" ||
			ext == L".tga" || ext == L".bmp" || ext == L".dds" || ext == L".bamtex")
			return EAssetReloadKind::Texture;

		const auto name = LowerAscii(path.filename().wstring());
		if (name.ends_with(L".bammat.json") || name.ends_with(L".bammatinst.json") ||
			name.ends_with(L".bammat") || name.ends_with(L".bammatinst"))
			return EAssetReloadKind::Material;

		if (name.ends_with(L".bammesh") || name.ends_with(L".bammesh.json"))
			return EAssetReloadKind::Mesh;

		// 타임라인 패널은 별도 사본을 저장하므로, 씬이 이미 불러온 에셋은 파일 변경으로만 갱신된다.
		if (name.ends_with(L".bamtimeline.json") || name.ends_with(L".bamtimeline") ||
			name.ends_with(L".actiontimeline.json") || name.ends_with(L".actiontimeline"))
			return EAssetReloadKind::ActionTimeline;

		if (name.ends_with(L".bamterrainlayer.json") || name.ends_with(L".bamterrainlayer"))
			return EAssetReloadKind::TerrainLayer;

		if (IsLocalizationDirectory(path) || (IsLocalizationDirectory(path.parent_path()) && IsLocalizationFile(path)))
			return EAssetReloadKind::Localization;

		return EAssetReloadKind::Unsupported;
	}

	class TextureFileListener : public efsw::FileWatchListener
	{
	public:
		explicit TextureFileListener(AssetHotReloadManager* manager) : m_Manager(manager) {}

		void handleFileAction(efsw::WatchID, const std::string& dir, const std::string& filename,
			efsw::Action action, std::string oldFilename) override
		{
			try
			{
				const auto directory = Engine::Utf8ToPath(dir);
				const auto path = directory / Engine::Utf8ToPath(filename);
				const auto kind = ClassifyAssetReload(path);
				const bool modified = action == efsw::Actions::Modified || action == efsw::Actions::Add ||
					action == efsw::Actions::Moved;
				if ((modified && kind != EAssetReloadKind::Unsupported) ||
					(action == efsw::Actions::Delete && kind == EAssetReloadKind::Localization))
					m_Manager->NotifyFileModified(path);
				if (action == efsw::Actions::Moved && !oldFilename.empty())
				{
					const auto oldPath = directory / Engine::Utf8ToPath(oldFilename);
					if (ClassifyAssetReload(oldPath) == EAssetReloadKind::Localization)
						m_Manager->NotifyFileModified(oldPath);
				}
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

	std::filesystem::path ProjectLocalizationDirectory()
	{
		return Engine::PlatformPath::ResolveUri(Engine::LocalizationManager::ProjectCatalogDirectory);
	}

	bool IsProjectLocalizationEvent(const std::filesystem::path& path)
	{
		const auto root = AssetFileKey(ProjectLocalizationDirectory());
		return !root.empty() && (AssetFileKey(path) == root || AssetFileKey(path.parent_path()) == root);
	}

	std::wstring AssetRelativeKey(const std::filesystem::path& filePath)
	{
		if (filePath.empty()) return {};

		const std::wstring uri = filePath.wstring();
		if (Engine::PlatformPath::IsVirtualUri(uri))
		{
			const auto resolved = Engine::PlatformPath::ResolveUri(uri);
			return AssetRelativeKey(resolved);
		}

		std::error_code ec;
		const auto canonicalPath = std::filesystem::weakly_canonical(filePath, ec);
		const auto absPath = ec ? filePath : canonicalPath;

		for (const auto& wt : Engine::PlatformPath::GetWorktreeRoots())
		{
			const auto canonicalWt = std::filesystem::weakly_canonical(wt, ec);
			const auto wtRoot = ec ? wt : canonicalWt;
			const auto rel = absPath.lexically_relative(wtRoot);
			if (!rel.empty() && !rel.native().starts_with(L".."))
			{
				auto key = rel.generic_wstring();
				for (wchar_t& ch : key)
					ch = static_cast<wchar_t>(std::towlower(ch));
				return key;
			}
		}

		const auto rel = absPath.lexically_relative(Engine::PlatformPath::GetProjectRoot());
		if (!rel.empty() && !rel.native().starts_with(L".."))
		{
			auto key = rel.generic_wstring();
			for (wchar_t& ch : key)
				ch = static_cast<wchar_t>(std::towlower(ch));
			return key;
		}

		return AssetFileKey(absPath);
	}

	std::wstring_view StripAssetPrefix(std::wstring_view sv)
	{
		if (sv.starts_with(L"assets/")) return sv.substr(7);
		if (sv.starts_with(L"resources/")) return sv.substr(10);
		if (sv.starts_with(L"engine/resources/")) return sv.substr(17);
		if (sv.starts_with(L"editor/resources/")) return sv.substr(17);
		return sv;
	}

	bool MatchesResourceFile(const Engine::Resource& resource, const std::filesystem::path& modifiedFilePath)
	{
		const auto& storedPath = resource.GetPath().empty() ? resource.GetKey() : resource.GetPath();
		if (storedPath.empty()) return false;

		// 1. 기존 절대 경로 일치 검사 (동일 워크트리 내 수정)
		const auto modAbsKey = AssetFileKey(modifiedFilePath);
		if (!modAbsKey.empty() && AssetFileKey(storedPath) == modAbsKey)
			return true;

		// 2. 워크트리 상대 경로 일치 검사 (다중 워크트리 간 수정)
		const auto modRelKey = AssetRelativeKey(modifiedFilePath);
		if (modRelKey.empty()) return false;

		const auto resRelKey = AssetRelativeKey(storedPath);
		if (!resRelKey.empty() && modRelKey == resRelKey)
			return true;

		// 3. 접두사 제거 하위 경로 일치 검사 (Assets/ vs Resources/ 등)
		const auto modSub = StripAssetPrefix(modRelKey);
		const auto resSub = StripAssetPrefix(resRelKey);
		if (!modSub.empty() && modSub == resSub)
			return true;

		return false;
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
	auto watcher = std::make_unique<efsw::FileWatcher>();
	auto listener = std::make_unique<TextureFileListener>(this);

	const auto registerDirectory = [&](const std::filesystem::path& dir, const char* label)
	{
		std::error_code ec;
		if (dir.empty() || !std::filesystem::is_directory(dir, ec))
			return;

		const auto norm = Engine::PlatformPath::Normalize(dir);
		for (const auto& watched : m_WatchedDirectories)
		{
			if (watched == norm)
				return;
		}

		const auto watchPath = Engine::WStrToStr(norm.wstring());
		const efsw::WatchID watchId = watcher->addWatch(watchPath, listener.get(), true);
		if (watchId <= 0)
		{
			EDITOR_LOG_WARN("[Asset Hot Reload] Failed to register {} watch: {}, code={}", label, watchPath, watchId);
		}
		else
		{
			m_WatchIDs.push_back(watchId);
			m_WatchedDirectories.push_back(norm);
			EDITOR_LOG_INFO("[Asset Hot Reload] Watching {} directory: {}", label, watchPath);
		}
	};

	// 1. 현재 프로젝트 루트 기준 기본 디렉터리 등록
	registerDirectory(Engine::PlatformPath::GetProjectAssetRoot(), "project assets");
	registerDirectory(Engine::PlatformPath::GetEngineResourceRoot(), "engine resources");
	registerDirectory(Engine::PlatformPath::GetEditorResourceRoot(), "editor resources");

	// 2. 모든 연결된 Git 워크트리의 에셋 및 리소스 디렉터리 자동 감시 등록
	for (const auto& root : Engine::PlatformPath::GetWorktreeRoots())
	{
		registerDirectory(root / "Assets", "worktree assets");
		registerDirectory(root / "Resources", "worktree resources");
		registerDirectory(root / "Engine/Resources", "worktree engine resources");
		registerDirectory(root / "Editor/Resources", "worktree editor resources");
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
		for (const auto watchId : m_WatchIDs)
		{
			if (watchId > 0)
				m_FileWatcher->removeWatch(watchId);
		}
		m_WatchIDs.clear();
		m_WatchedDirectories.clear();
		delete m_FileWatcher;
		m_FileWatcher = nullptr;
	}

	delete m_FileListener;
	m_FileListener = nullptr;
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
	bool reloadProject = std::exchange(m_ProjectLocaleReloadRequested, false);
	if (!m_PendingProjectLocaleImport.empty())
		ImportProjectLocalization(std::exchange(m_PendingProjectLocaleImport, std::filesystem::path{}));

	for (const auto& file : CollectReadyFiles(std::chrono::milliseconds(300)))
	{
		switch (ClassifyAssetReload(file))
		{
		case EAssetReloadKind::Texture: ProcessModifiedTexture(file); break;
		case EAssetReloadKind::Material: ProcessModifiedMaterial(file); break;
		case EAssetReloadKind::Mesh: ProcessModifiedMesh(file); break;
		case EAssetReloadKind::ActionTimeline: ProcessModifiedTimeline(file); break;
		case EAssetReloadKind::TerrainLayer: ProcessModifiedTerrainLayer(file); break;
		case EAssetReloadKind::Localization:
			if (IsProjectLocalizationEvent(file)) reloadProject = true;
			else ProcessModifiedLocalization(file);
			break;
		case EAssetReloadKind::Unsupported: break;
		}
	}
	if (reloadProject) ReloadProjectLocalization();
}

void AssetHotReloadManager::RequestProjectLocalizationImport(std::filesystem::path source)
{
	m_PendingProjectLocaleImport = std::move(source);
}

void AssetHotReloadManager::RequestProjectLocalizationReload()
{
	m_ProjectLocaleReloadRequested = true;
}

void AssetHotReloadManager::ReloadProjectLocalization()
{
	const bool loaded = IsSuccess(Engine::LocalizationManager::Get().ReloadProjectData());
	NotificationManager::Get().AddNotification(LOCAL("Settings.ProjectLocale"),
		LOCAL(loaded ? "Settings.ProjectLocaleReloaded" : "Settings.ProjectLocaleReloadFailed"),
		loaded ? ELogLevel::Info : ELogLevel::Warn, 3.0f);
}

void AssetHotReloadManager::ImportProjectLocalization(const std::filesystem::path& source)
{
	const auto directory = ProjectLocalizationDirectory();
	const auto destination = directory / source.filename();
	const auto sourceName = Engine::WStrToStr(source.filename().wstring());
	std::error_code ec;
	if (directory.empty() || !IsLocalizationFile(source) || !std::filesystem::is_regular_file(source, ec))
	{
		NotificationManager::Get().AddNotification(LOCAL("Settings.ProjectLocale"),
			LOCAL_FMT("Settings.ProjectLocaleImportFailed", sourceName), ELogLevel::Warn);
		return;
	}

	// Selecting a catalog already inside the active directory just refreshes it.
	if (std::filesystem::equivalent(source, destination, ec) && !ec)
	{
		ReloadProjectLocalization();
		return;
	}
	ec.clear();
	std::filesystem::create_directories(directory, ec);
	const bool copied = !ec && std::filesystem::copy_file(source, destination, std::filesystem::copy_options::none, ec);
	if (!copied)
	{
		EDITOR_LOG_WARN("[Localization] Could not import {}: {}", sourceName, ec.message());
		NotificationManager::Get().AddNotification(LOCAL("Settings.ProjectLocale"),
			LOCAL_FMT("Settings.ProjectLocaleImportFailed", sourceName), ELogLevel::Warn);
		return;
	}

	// The manager keeps the active catalog if any file is invalid. Roll back this newly added file only.
	if (IsFailure(Engine::LocalizationManager::Get().ReloadProjectData()))
	{
		std::filesystem::remove(destination, ec);
		if (ec) EDITOR_LOG_WARN("[Localization] Could not remove rejected catalog {}: {}", sourceName, ec.message());
		NotificationManager::Get().AddNotification(LOCAL("Settings.ProjectLocale"),
			LOCAL_FMT("Settings.ProjectLocaleImportFailed", sourceName), ELogLevel::Warn);
		return;
	}
	NotificationManager::Get().AddNotification(LOCAL("Settings.ProjectLocale"),
		LOCAL_FMT("Settings.ProjectLocaleImported", sourceName), ELogLevel::Info, 3.0f);
}

void AssetHotReloadManager::ProcessModifiedTexture(const std::filesystem::path& texturePath)
{
	if (texturePath.empty()) return;

	auto& manager = Engine::ResourceManager::Get();
	bool matched = false;

	for (const auto& handle : manager.GetResourceHandles<Engine::Texture>())
	{
		auto* texture = dynamic_cast<Engine::Texture*>(manager.GetResource(handle));
		if (texture == nullptr || !MatchesResourceFile(*texture, texturePath))
			continue;

		matched = true;

		if (!IsSuccess(texture->Reload(texturePath)))
			continue;

		RequestThumbnailRefresh(texturePath, texture);

		NotificationManager::Get().AddNotification(LOCAL("Notification.TextureHotReloaded"),
			LOCAL_FMT("Notification.ReloadedFile", Engine::WStrToStr(texturePath.filename().wstring())),
			ELogLevel::Info, 3.0f);
	}

	// 로드된 리소스 없이 콘텐츠 브라우저 썸네일만 존재할 수도 있습니다.
	if (!matched)
		RequestThumbnailRefresh(texturePath);
}

void AssetHotReloadManager::ProcessModifiedMaterial(const std::filesystem::path& materialPath)
{
	if (materialPath.empty()) return;

	auto& manager = Engine::ResourceManager::Get();
	bool matched = false;

	auto tryReload = [&](const std::vector<Engine::Handle>& handles)
		{
			for (const auto& handle : handles)
			{
				auto* material = dynamic_cast<Engine::MaterialInterface*>(manager.GetResource(handle));
				if (material == nullptr || !MatchesResourceFile(*material, materialPath))
					continue;

				matched = true;

				if (!IsSuccess(material->Reload(materialPath)))
					continue;

				RequestThumbnailRefresh(materialPath, material);

				NotificationManager::Get().AddNotification(LOCAL("Notification.MaterialHotReloaded"),
					LOCAL_FMT("Notification.ReloadedFile", Engine::WStrToStr(materialPath.filename().wstring())),
					ELogLevel::Info, 3.0f);
			}
		};

	tryReload(manager.GetResourceHandles<Engine::Material>());
	tryReload(manager.GetResourceHandles<Engine::MaterialInstance>());

	if (!matched)
		RequestThumbnailRefresh(materialPath);
}

void AssetHotReloadManager::ProcessModifiedTimeline(const std::filesystem::path& timelinePath)
{
	if (timelinePath.empty()) return;

	// 이미 불러온 타임라인만 제자리에서 다시 읽는다. 재생 중인 컴포넌트는 같은 핸들을 쓰므로 다음 틱부터 새 길이·트랙을 따른다.
	auto& manager = Engine::ResourceManager::Get();
	for (const auto& handle : manager.GetResourceHandles<Engine::ActionTimelineAsset>())
	{
		auto* timeline = dynamic_cast<Engine::ActionTimelineAsset*>(manager.GetResource(handle));
		if (timeline == nullptr || !MatchesResourceFile(*timeline, timelinePath) || !IsSuccess(timeline->Reload(timelinePath)))
			continue;

		NotificationManager::Get().AddNotification(LOCAL("Notification.TimelineHotReloaded"),
			LOCAL_FMT("Notification.ReloadedFile", Engine::WStrToStr(timelinePath.filename().wstring())),
			ELogLevel::Info, 3.0f);
	}
}

void AssetHotReloadManager::ProcessModifiedTerrainLayer(const std::filesystem::path& layerPath)
{
	if (layerPath.empty()) return;

	// 레이어 편집기가 저장한 파일을 이미 불러온 런타임 에셋에 반영한다. 같은 에셋을 참조하는 모든 지형이 다음 프레임부터 새 설정을 본다.
	auto& manager = Engine::ResourceManager::Get();
	for (const auto& handle : manager.GetResourceHandles<Engine::TerrainLayerAsset>())
	{
		auto* layer = dynamic_cast<Engine::TerrainLayerAsset*>(manager.GetResource(handle));
		if (layer == nullptr || !MatchesResourceFile(*layer, layerPath) || !IsSuccess(layer->Reload(layerPath)))
			continue;

		NotificationManager::Get().AddNotification(LOCAL("Notification.TerrainLayerHotReloaded"),
			LOCAL_FMT("Notification.ReloadedFile", Engine::WStrToStr(layerPath.filename().wstring())),
			ELogLevel::Info, 3.0f);
	}
}

void AssetHotReloadManager::ProcessModifiedLocalization(const std::filesystem::path& locPath)
{
	const auto parent = AssetFileKey(locPath.parent_path());
	const auto engine = AssetFileKey(Engine::PlatformPath::GetEngineResourceRoot() / "Data/Localization");
	const auto editor = AssetFileKey(Engine::PlatformPath::GetEditorResourceRoot() / "Data/Localization");
	if (parent.empty() || (parent != engine && parent != editor) || !IsLocalizationFile(locPath)) return;
	// .csv 파일이 수정된 경우 파이썬 변환 스크립트 실행
	if (LowerAscii(locPath.extension().wstring()) == L".csv")
	{
		std::error_code ec;
		std::filesystem::path scriptPath = Engine::PlatformPath::ResolveProjectRelative(
			"Tools/Codegen/CSVToJsonConverter/localizationcsvtojson.py");
		if (!std::filesystem::exists(scriptPath, ec))
		{
			for (const auto& wt : Engine::PlatformPath::GetWorktreeRoots())
			{
				const auto candidate = wt / "Tools/Codegen/CSVToJsonConverter/localizationcsvtojson.py";
				if (std::filesystem::exists(candidate, ec))
				{
					scriptPath = candidate;
					break;
				}
			}
		}

		if (std::filesystem::exists(scriptPath, ec))
		{
			const auto parentDir = locPath.parent_path();
			std::wstring cmd = L"python \"" + scriptPath.wstring() + L"\" --input \"" +
				parentDir.wstring() + L"\" --output \"" + parentDir.wstring() + L"\"";
			_wsystem(cmd.c_str());
		}
	}

	// 로컬라이제이션 데이터 등록 및 리로드
	const auto parentDir = locPath.parent_path();
	const auto koreanJson = parentDir / L"Korean.json";
	const auto englishJson = parentDir / L"English.json";
	std::error_code ec;
	if (std::filesystem::exists(koreanJson, ec))
		Engine::LocalizationManager::Get().RegisterLocalizationData(koreanJson.wstring());
	if (std::filesystem::exists(englishJson, ec))
		Engine::LocalizationManager::Get().RegisterLocalizationData(englishJson.wstring());

	if (IsSuccess(Engine::LocalizationManager::Get().ReloadData()))
	{
		EDITOR_LOG_INFO("[HotReload] Localization reloaded: {}",
			Engine::WStrToStr(locPath.filename().wstring()));
		NotificationManager::Get().AddNotification(LOCAL("Notification.LocalizationReloaded"),
			LOCAL_FMT("Notification.ReloadedFile", Engine::WStrToStr(locPath.filename().wstring())),
			ELogLevel::Info, 3.0f);
	}
}

void AssetHotReloadManager::ProcessModifiedMesh(const std::filesystem::path& meshPath)
{
	if (meshPath.empty()) return;

	const auto targetKey = AssetRelativeKey(meshPath);
	if (targetKey.empty()) return;

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
		if (!mesh) continue;

		if (!MatchesResourceFile(*mesh, meshPath))
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
	const auto key = AssetRelativeKey(filePath);
	if (key.empty())
		return;

	MeshFileState state;
	if (ReadMeshFileState(filePath, state))
		m_MeshFileStates.insert_or_assign(key, state);
	else
		m_MeshFileStates.erase(key);
}