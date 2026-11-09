#include "ShaderHotReloadManager.h"
#include "ResourceManager.h"
#include "Shader.h"
#include "Renderer.h"
#include "Logger.h"
#include "NotificationManager.h"
#include <windows.h>
#include "PlatformPath.h"

namespace
{
	std::string ToLowerCopy(std::string value)
	{
		for (char& c : value) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
		return value;
	}

	std::string NormalizePathKey(const std::filesystem::path& path)
	{
		return ToLowerCopy(path.lexically_normal().generic_string());
	}

	std::vector<std::filesystem::path> ExtractIncludedFiles(const std::filesystem::path& shaderFile)
	{
		std::vector<std::filesystem::path> includes;
		std::ifstream stream(shaderFile);
		if (!stream.is_open()) return includes;

		const std::filesystem::path parentDir = shaderFile.parent_path();
		std::string line;
		while (std::getline(stream, line))
		{
			const size_t firstNonSpace = line.find_first_not_of(" \t");
			if (firstNonSpace == std::string::npos || line[firstNonSpace] != '#') continue;

			const size_t includePos = line.find("include", firstNonSpace + 1);
			if (includePos == std::string::npos) continue;
			if (line.find_first_not_of(" \t", firstNonSpace + 1) != includePos) continue;

			const size_t firstQuote = line.find('"', includePos + 7);
			if (firstQuote == std::string::npos) continue;
			const size_t secondQuote = line.find('"', firstQuote + 1);
			if (secondQuote == std::string::npos || secondQuote <= firstQuote + 1) continue;

			const std::string relativeInclude = line.substr(firstQuote + 1, secondQuote - firstQuote - 1);
			includes.push_back((parentDir / relativeInclude).lexically_normal());
		}
		return includes;
	}

	bool DependsOnAnyHeader(const std::filesystem::path& hlslPath, const std::unordered_set<std::string>& modifiedHeaderKeys)
	{
		std::unordered_set<std::string> visited;
		std::vector<std::filesystem::path> queue = { hlslPath };
		visited.insert(NormalizePathKey(hlslPath));

		while (!queue.empty())
		{
			const std::filesystem::path current = queue.back();
			queue.pop_back();

			for (const std::filesystem::path& incPath : ExtractIncludedFiles(current))
			{
				const std::string key = NormalizePathKey(incPath);
				if (modifiedHeaderKeys.contains(key)) return true;

				std::error_code ec;
				if (std::filesystem::exists(incPath, ec) && visited.insert(key).second)
				{
					queue.push_back(incPath);
				}
			}
		}
		return false;
	}

	class ShaderFileListener : public efsw::FileWatchListener
	{
	public:
		ShaderFileListener(ShaderHotReloadManager* manager) : m_Manager(manager) {}
		void handleFileAction(efsw::WatchID watchid, const std::string& dir, const std::string& filename,
			efsw::Action action, std::string oldFilename) override
		{
			if (action == efsw::Actions::Modified || action == efsw::Actions::Add)
			{
				std::filesystem::path fullPath = std::filesystem::path(reinterpret_cast<const char8_t*>(dir.c_str())) / reinterpret_cast<const char8_t*>(filename.c_str());
				std::string ext = ToLowerCopy(fullPath.extension().string());
				if (ext == ".hlsl" || ext == ".hlsli")
				{
					m_Manager->NotifyFileModified(fullPath);
				}
			}
		}
	private:
		ShaderHotReloadManager* m_Manager = nullptr;
	};
}

void ShaderHotReloadManager::RegisterDefaultCandidates()
{
	// 1. 엔진 시스템 빌트인 셰이더 경로 (Engine://Shader/hlsl)
	AddWatchPath(L"Engine://Shader/hlsl");

	// 2. 사용자 프로젝트 애셋 셰이더 경로 (Project://Shader/hlsl 또는 Project://Shader)
	AddWatchPath(L"Project://Shader/hlsl");
	AddWatchPath(L"Project://Shader");

	// 3. 환경 변수 기반 셰이더 경로 등록 (예: BAM_SHADER_PATH="C:/CustomShaders;D:/Plugins/Shader")
	AddWatchPathsFromEnvironment("BAM_SHADER_PATH");
}

#pragma region Constructor&Destrcutor
EResult ShaderHotReloadManager::Initialize(void* arg)
{
	m_FileWatcher = new efsw::FileWatcher();
	m_FileListener = new ShaderFileListener(this);

	RegisterDefaultCandidates();

	m_FileWatcher->watch();
	return EResult::Success;
}

void ShaderHotReloadManager::OnWatchPathAdded(const std::filesystem::path& resolvedPath)
{
	if (!m_FileWatcher || !m_FileListener) return;

	const efsw::WatchID watchId = m_FileWatcher->addWatch(resolvedPath.string(), m_FileListener, true);
	if (watchId > 0)
	{
		m_WatchIdMap[resolvedPath] = watchId;
		EDITOR_LOG_INFO("[Shader Hot Reload] Watching directory: {}", resolvedPath.string());
	}
}

void ShaderHotReloadManager::OnWatchPathRemoved(const std::filesystem::path& resolvedPath)
{
	if (!m_FileWatcher) return;

	auto it = m_WatchIdMap.find(resolvedPath);
	if (it != m_WatchIdMap.end())
	{
		m_FileWatcher->removeWatch(it->second);
		m_WatchIdMap.erase(it);
		EDITOR_LOG_INFO("[Shader Hot Reload] Stopped watching directory: {}", resolvedPath.string());
	}
}

ShaderHotReloadManager* ShaderHotReloadManager::Create()
{
	ShaderHotReloadManager* instance = new ShaderHotReloadManager();
	if (instance->Initialize() != EResult::Success)
	{
		instance->Free();
		delete instance;
		return nullptr;
	}
	return instance;
}

void ShaderHotReloadManager::Free()
{
	if (m_FileWatcher)
	{
		for (const auto& [path, watchId] : m_WatchIdMap)
		{
			if (watchId != 0) m_FileWatcher->removeWatch(watchId);
		}
		m_WatchIdMap.clear();
		delete m_FileWatcher;
		m_FileWatcher = nullptr;
	}
	delete m_FileListener;
	m_FileListener = nullptr;
	__super::Free();
}
#pragma endregion

void ShaderHotReloadManager::Update()
{
	if (m_PendingRecompileAll.exchange(false))
	{
		EDITOR_LOG_INFO("[Shader Hot Reload] Safe tick: Recompiling all shaders...");
		RecompileAll();
		return;
	}

	const auto readyFiles = CollectReadyFiles(std::chrono::milliseconds(300));
	if (readyFiles.empty())
		return;

	std::vector<std::filesystem::path> targetHlslFiles;
	std::unordered_set<std::string> targetHlslKeys;
	std::unordered_set<std::string> modifiedHeaderKeys;

	for (const auto& file : readyFiles)
	{
		const std::string ext = ToLowerCopy(file.extension().string());

		if (ext == ".hlsli")
		{
			modifiedHeaderKeys.insert(NormalizePathKey(file));
		}
		else if (ext == ".hlsl")
		{
			if (targetHlslKeys.insert(NormalizePathKey(file)).second)
				targetHlslFiles.push_back(file);
		}
	}

	if (!modifiedHeaderKeys.empty())
	{
		std::error_code ec;
		for (const auto& watchDir : GetWatchPaths())
		{
			if (!std::filesystem::exists(watchDir, ec)) continue;

			for (const auto& entry : std::filesystem::recursive_directory_iterator(watchDir, ec))
			{
				if (!entry.is_regular_file(ec) || ToLowerCopy(entry.path().extension().string()) != ".hlsl")
					continue;

				const std::string hlslKey = NormalizePathKey(entry.path());
				if (targetHlslKeys.contains(hlslKey)) continue;

				if (DependsOnAnyHeader(entry.path(), modifiedHeaderKeys))
				{
					targetHlslKeys.insert(hlslKey);
					targetHlslFiles.push_back(entry.path());
				}
			}
		}
		EDITOR_LOG_INFO("[Shader Hot Reload] Shared header modified (.hlsli). Recompiling {} affected shader(s)...", targetHlslFiles.size());
	}

	for (const auto& file : targetHlslFiles)
	{
		ProcessModifiedFile(file);
	}
}

void ShaderHotReloadManager::ProcessModifiedFile(const std::filesystem::path& hlslPath)
{
	const EShaderType shaderType = Engine::ShaderCompiler::DeduceShaderTypeFromPath(hlslPath);
	if (shaderType == EShaderType::Unknown) return;

	const Engine::ERHIType rhiType = Engine::Renderer::IsCreated()
		? Engine::Renderer::Get().GetRHIType()
		: Engine::ERHIType::DirectX12;

	const Engine::ShaderCompileResult result = Engine::ShaderCompiler::Compile(hlslPath, shaderType, rhiType);
	if (!result.Success)
	{
		EDITOR_LOG_ERROR("[Shader Hot Reload] Failed to compile '{}':\n{}",
			hlslPath.filename().string(), result.ErrorMessage);
		NotificationManager::Get().AddNotification(
			LOCAL("Notification.ShaderCompileError"),
			hlslPath.filename().string() + ":\n" + result.ErrorMessage,
			ELogLevel::Error, 8.0f);
		return;
	}

	Engine::ResourceManager& rm = Engine::ResourceManager::Get();
	auto shaderHandles = rm.GetResourceHandles<Engine::Shader>();

	std::string targetStem = hlslPath.stem().string();
	for (char& c : targetStem) c = static_cast<char>(tolower(c));

	bool reloadedAny = false;
	for (const auto& handle : shaderHandles)
	{
		Engine::Shader* shader = static_cast<Engine::Shader*>(rm.GetResource(handle));
		if (!shader) continue;

		std::filesystem::path resKeyPath = shader->GetKey();
		std::filesystem::path resFilePath = rhiType == Engine::ERHIType::DirectX12
			? shader->GetDxilPath()
			: shader->GetSpirvPath();

		std::string keyStem = resKeyPath.stem().string();
		for (char& c : keyStem) c = static_cast<char>(tolower(c));

		std::string fileStem = resFilePath.stem().string();
		for (char& c : fileStem) c = static_cast<char>(tolower(c));

		// 현재 백엔드의 바이너리 파일명을 우선 사용한다.
		const bool isMatch = !fileStem.empty()
			? (fileStem == targetStem)
			: (keyStem == targetStem);

		if (isMatch)
		{
			if (IsSuccess(shader->UpdateBytecode(result.Bytecode)))
			{
				reloadedAny = true;
				EDITOR_LOG_INFO("[Shader Hot Reload] Hot-swapped: {} (Key: {}, {} bytes)",
					hlslPath.filename().string(), Engine::WStrToStr(shader->GetKey()), result.Bytecode.size());
			}
			else
			{
				EDITOR_LOG_ERROR("[Shader Hot Reload] Failed to update bytecode for shader '{}'",
					Engine::WStrToStr(shader->GetKey()));
			}
		}
	}

	if (reloadedAny)
	{
		NotificationManager::Get().AddNotification(
			LOCAL("Notification.ShaderHotReloaded"),
			LOCAL_FMT("Notification.ReloadedFile", hlslPath.filename().string()),
			ELogLevel::Info, 3.0f);
	}
	else
	{
		ShaderDesc desc;
		const std::wstring stemW = hlslPath.stem().wstring();
		desc.Key = Engine::PlatformPath::ToVirtualUri(hlslPath);
		if (desc.Key.empty())
		{
			desc.Key = L"Engine://Shader/" + stemW;
		}
		desc.shaderType = shaderType;
		if (rhiType == Engine::ERHIType::DirectX12)
		{
			desc.dxilPath = result.OutputPath.wstring();
		}
		else
		{
			desc.spirvPath = result.OutputPath.wstring();
		}
		desc.Path = result.OutputPath.wstring();
		auto newHandle = rm.LoadResource<Engine::Shader>(&desc);
		if (newHandle.IsValid())
		{
			EDITOR_LOG_INFO("[Shader Hot Reload] Compiled and registered new shader: {} (Key: {}, {} bytes)",
				hlslPath.filename().string(), Engine::WStrToStr(desc.Key), result.Bytecode.size());
			NotificationManager::Get().AddNotification(
				LOCAL("Notification.ShaderHotReloaded"),
				LOCAL_FMT("Notification.ReloadedFile", hlslPath.filename().string()),
				ELogLevel::Info, 3.0f);
		}
	}
}

void ShaderHotReloadManager::RecompileAll()
{
	std::error_code ec;
	for (const auto& dir : GetWatchPaths())
	{
		if (!std::filesystem::exists(dir, ec)) continue;

		for (const auto& entry : std::filesystem::recursive_directory_iterator(dir, ec))
		{
			if (entry.is_regular_file(ec) && entry.path().extension() == ".hlsl")
			{
				ProcessModifiedFile(entry.path());
			}
		}
	}
}
