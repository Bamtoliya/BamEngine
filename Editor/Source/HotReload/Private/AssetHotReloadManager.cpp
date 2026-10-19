#include "AssetHotReloadManager.h"
#include "ShaderHotReloadManager.h"
#include "LocalizationManager.h"
#include "ResourceManager.h"
#include "Texture.h"
#include "Material.h"
#include "Mesh.h"
#include "MaterialInstance.h"
#include "Logger.h"
#include <windows.h>

IMPLEMENT_SINGLETON(AssetHotReloadManager)

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
				std::filesystem::path fullPath = std::filesystem::path(dir) / filename;
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
}

std::filesystem::path AssetHotReloadManager::ResolveTextureDirectory()
{
	std::error_code ec;
	std::filesystem::path candidate = std::filesystem::current_path() / "Resources";
	if (std::filesystem::exists(candidate, ec)) return candidate;

	wchar_t exePathBuf[MAX_PATH] = {};
	GetModuleFileNameW(nullptr, exePathBuf, MAX_PATH);
	std::filesystem::path cur = std::filesystem::path(exePathBuf).parent_path();
	for (int i = 0; i < 5; ++i)
	{
		candidate = cur / "Resources";
		if (std::filesystem::exists(candidate, ec)) return candidate;
		if (!cur.has_parent_path()) break;
		cur = cur.parent_path();
	}

	return "Resources";
}

EResult AssetHotReloadManager::Initialize(void* arg)
{
	m_WatchDirectory = ResolveTextureDirectory();
	std::error_code ec;
	if (!std::filesystem::exists(m_WatchDirectory, ec))
	{
		ENGINE_LOG_WARN("[Texture Hot Reload] Resources directory does not exist: {}", m_WatchDirectory.string());
		return EResult::Success;
	}

	m_FileWatcher = new efsw::FileWatcher();
	m_FileListener = new TextureFileListener(this);
	m_WatchID = m_FileWatcher->addWatch(m_WatchDirectory.string(), m_FileListener, true);
	m_FileWatcher->watch();

	ENGINE_LOG_INFO("[Texture Hot Reload] Watching directory: {}", m_WatchDirectory.string());
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
	__super::Free();
}

void AssetHotReloadManager::NotifyFileModified(const std::filesystem::path& filePath)
{
	if (!m_AutoReload) return;
	std::lock_guard<std::mutex> lock(m_QueueMutex);
	m_PendingFiles[filePath.string()] = std::chrono::steady_clock::now();
}

void AssetHotReloadManager::Update()
{
	if (m_PendingFiles.empty()) return;

	const auto now = std::chrono::steady_clock::now();
	std::vector<std::filesystem::path> readyFiles;

	{
		std::lock_guard<std::mutex> lock(m_QueueMutex);
		for (auto it = m_PendingFiles.begin(); it != m_PendingFiles.end(); )
		{
			if (std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second).count() >= 300)
			{
				readyFiles.push_back(it->first);
				it = m_PendingFiles.erase(it);
			}
			else
			{
				++it;
			}
		}
	}

	for (const auto& file : readyFiles)
	{
		std::string ext = file.extension().string();
		for (char& c : ext) c = static_cast<char>(tolower(c));
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
	Engine::ResourceManager& rm = Engine::ResourceManager::Get();
	auto textureHandles = rm.GetResourceHandles<Engine::Texture>();

	std::string targetStem = texturePath.stem().string();
	for (char& c : targetStem) c = static_cast<char>(tolower(c));

	bool reloadedAny = false;
	for (const auto& handle : textureHandles)
	{
		Engine::Texture* tex = static_cast<Engine::Texture*>(rm.GetResource(handle));
		if (!tex) continue;

		std::filesystem::path resKeyPath = tex->GetKey();
		std::filesystem::path resFilePath = tex->GetPath();

		std::string keyStem = resKeyPath.stem().string();
		for (char& c : keyStem) c = static_cast<char>(tolower(c));

		std::string fileStem = resFilePath.stem().string();
		for (char& c : fileStem) c = static_cast<char>(tolower(c));

		const bool isMatch = !fileStem.empty() ? (fileStem == targetStem) : (keyStem == targetStem);

		if (isMatch)
		{
			if (IsSuccess(tex->Reload()))
			{
				reloadedAny = true;
				// 방금 만든 우측 하단 토스트 알림 연동!
				ShaderHotReloadManager::Get().AddNotification(LOCAL("Notification.TextureHotReloaded"),
					LOCAL_FMT("Notification.ReloadedFile", texturePath.filename().string()), false, 3.0f);
			}
		}
	}
}

void AssetHotReloadManager::ProcessModifiedMaterial(const std::filesystem::path& materialPath)
{
	Engine::ResourceManager& rm = Engine::ResourceManager::Get();

	std::string targetStem = materialPath.filename().string();
	for (char& c : targetStem) c = static_cast<char>(tolower(c));
	if (targetStem.ends_with(".json")) targetStem = targetStem.substr(0, targetStem.length() - 5);
	if (targetStem.ends_with(".bammat")) targetStem = targetStem.substr(0, targetStem.length() - 7);
	if (targetStem.ends_with(".bammatinst")) targetStem = targetStem.substr(0, targetStem.length() - 11);

	auto matHandles = rm.GetResourceHandles<Engine::Material>();
	auto instHandles = rm.GetResourceHandles<Engine::MaterialInstance>();

	auto tryReload = [&](const std::vector<Engine::Handle>& handles) {
		for (const auto& handle : handles)
		{
			auto* mat = static_cast<Engine::MaterialInterface*>(rm.GetResource(handle));
			if (!mat) continue;

			std::string keyStem = std::filesystem::path(mat->GetKey()).stem().string();
			for (char& c : keyStem) c = static_cast<char>(tolower(c));

			std::string fileStem = std::filesystem::path(mat->GetPath()).stem().string();
			for (char& c : fileStem) c = static_cast<char>(tolower(c));

			if (keyStem == targetStem || fileStem == targetStem)
			{
				if (IsSuccess(mat->Reload(materialPath)))
				{
					ShaderHotReloadManager::Get().AddNotification(LOCAL("Notification.MaterialHotReloaded"),
						LOCAL_FMT("Notification.ReloadedFile", materialPath.filename().string()), false, 3.0f);
				}
			}
		}
		};

	tryReload(matHandles);
	tryReload(instHandles);
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
		ENGINE_LOG_INFO("[HotReload] Localization reloaded: {}", locPath.filename().string());
		ShaderHotReloadManager::Get().AddNotification(LOCAL("Notification.LocalizationReloaded"),
			LOCAL_FMT("Notification.ReloadedFile", locPath.filename().string()), false, 3.0f);
	}
}

void AssetHotReloadManager::ProcessModifiedMesh(const std::filesystem::path& meshPath)
{
	Engine::ResourceManager& rm = Engine::ResourceManager::Get();
	auto meshHandles = rm.GetResourceHandles<Engine::Mesh>();

	std::string targetStem = meshPath.stem().string();
	for (char& c : targetStem) c = static_cast<char>(tolower(c));
	if (targetStem.ends_with(".bammesh")) targetStem = targetStem.substr(0, targetStem.length() - 8);

	for (const auto& handle : meshHandles)
	{
		auto* mesh = static_cast<Engine::Mesh*>(rm.GetResource(handle));
		if (!mesh) continue;

		std::string keyStem = std::filesystem::path(mesh->GetKey()).stem().string();
		for (char& c : keyStem) c = static_cast<char>(tolower(c));
		if (keyStem.ends_with(".bammesh")) keyStem = keyStem.substr(0, keyStem.length() - 8);

		std::string fileStem = std::filesystem::path(mesh->GetPath()).stem().string();
		for (char& c : fileStem) c = static_cast<char>(tolower(c));
		if (fileStem.ends_with(".bammesh")) fileStem = fileStem.substr(0, fileStem.length() - 8);

		if (keyStem == targetStem || fileStem == targetStem)
		{
			if (IsSuccess(mesh->Reload(meshPath)))
			{
				ShaderHotReloadManager::Get().AddNotification(LOCAL("Notification.MeshHotReloaded"),
					LOCAL_FMT("Notification.ReloadedFile", meshPath.filename().string()), false, 3.0f);
			}
		}
	}
}