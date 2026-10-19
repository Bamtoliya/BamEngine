#include "ShaderHotReloadManager.h"
#include "ResourceManager.h"
#include "Shader.h"
#include "Renderer.h"
#include "Logger.h"
#include <windows.h>

BEGIN(Editor)

IMPLEMENT_SINGLETON(ShaderHotReloadManager)

namespace
{
	class ShaderFileListener : public efsw::FileWatchListener
	{
	public:
		ShaderFileListener(ShaderHotReloadManager* manager) : m_Manager(manager) {}
		void handleFileAction(efsw::WatchID watchid, const std::string& dir, const std::string& filename,
			efsw::Action action, std::string oldFilename) override
		{
			if (action == efsw::Actions::Modified || action == efsw::Actions::Add)
			{
				std::filesystem::path fullPath = std::filesystem::path(dir) / filename;
				std::string ext = fullPath.extension().string();
				for (char& c : ext) c = static_cast<char>(tolower(c));
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

std::filesystem::path ShaderHotReloadManager::ResolveShaderSourceDirectory()
{
	std::error_code ec;
	std::filesystem::path candidate = std::filesystem::current_path() / "Resources" / "Shader" / "hlsl";
	if (std::filesystem::exists(candidate, ec)) return candidate;

	wchar_t exePathBuf[MAX_PATH] = {};
	GetModuleFileNameW(nullptr, exePathBuf, MAX_PATH);
	std::filesystem::path cur = std::filesystem::path(exePathBuf).parent_path();
	for (int i = 0; i < 5; ++i)
	{
		candidate = cur / "Resources" / "Shader" / "hlsl";
		if (std::filesystem::exists(candidate, ec)) return candidate;
		if (!cur.has_parent_path()) break;
		cur = cur.parent_path();
	}

	return "Resources/Shader/hlsl";
}

EResult ShaderHotReloadManager::Initialize(void* arg)
{
	m_WatchDirectory = ResolveShaderSourceDirectory();
	std::error_code ec;
	if (!std::filesystem::exists(m_WatchDirectory, ec))
	{
		ENGINE_LOG_WARN("[Shader Hot Reload] Shader directory does not exist: {}", m_WatchDirectory.string());
		return EResult::Success;
	}

	m_FileWatcher = new efsw::FileWatcher();
	m_FileListener = new ShaderFileListener(this);
	m_WatchID = m_FileWatcher->addWatch(m_WatchDirectory.string(), m_FileListener, true);
	m_FileWatcher->watch();

	ENGINE_LOG_INFO("[Shader Hot Reload] Watching directory: {}", m_WatchDirectory.string());
	return EResult::Success;
}

void ShaderHotReloadManager::Free()
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

void ShaderHotReloadManager::NotifyFileModified(const std::filesystem::path& filePath)
{
	if (!m_AutoReload) return;
	std::lock_guard<std::mutex> lock(m_QueueMutex);
	m_PendingFiles[filePath.string()] = std::chrono::steady_clock::now();
}

void ShaderHotReloadManager::Update()
{
	// UI 버튼 등에서 요청된 전체 재컴파일을 렌더링 전 안전한 유휴 시점에 실행
	if (m_PendingRecompileAll.exchange(false))
	{
		ENGINE_LOG_INFO("[Shader Hot Reload] Safe tick: Recompiling all shaders...");
		RecompileAll();
		return;
	}

	if (m_PendingFiles.empty()) return;

	const auto now = std::chrono::steady_clock::now();
	std::vector<std::filesystem::path> readyFiles;
	bool hasHeaderModified = false;

	{
		std::lock_guard<std::mutex> lock(m_QueueMutex);
		for (auto it = m_PendingFiles.begin(); it != m_PendingFiles.end(); )
		{
			if (std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second).count() >= 300)
			{
				std::filesystem::path p(it->first);
				std::string ext = p.extension().string();
				for (char& c : ext) c = static_cast<char>(tolower(c));
				if (ext == ".hlsli") hasHeaderModified = true;
				readyFiles.push_back(p);
				it = m_PendingFiles.erase(it);
			}
			else
			{
				++it;
			}
		}
	}

	if (hasHeaderModified)
	{
		ENGINE_LOG_INFO("[Shader Hot Reload] Shared header modified (.hlsli). Recompiling all shaders...");
		RecompileAll();
		return;
	}

	for (const auto& file : readyFiles)
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
		ENGINE_LOG_ERROR("[Shader Hot Reload] Failed to compile '{}':\n{}",
			hlslPath.filename().string(), result.ErrorMessage);
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
		std::filesystem::path resFilePath = shader->GetPath();

		std::string keyStem = resKeyPath.stem().string();
		for (char& c : keyStem) c = static_cast<char>(tolower(c));

		std::string fileStem = resFilePath.stem().string();
		for (char& c : fileStem) c = static_cast<char>(tolower(c));

		// 실제 파일 경로(Path)가 지정되어 있으면 파일명 매칭을 최우선으로 검사
		const bool isMatch = !fileStem.empty()
			? (fileStem == targetStem)
			: (keyStem == targetStem);

		if (isMatch)
		{
			if (IsSuccess(shader->UpdateBytecode(result.Bytecode)))
			{
				reloadedAny = true;
				ENGINE_LOG_INFO("[Shader Hot Reload] Hot-swapped: {} (Key: {}, {} bytes)",
					hlslPath.filename().string(), WStrToStr(shader->GetKey()), result.Bytecode.size());
			}
		}
	}

	if (!reloadedAny)
	{
		ENGINE_LOG_INFO("[Shader Hot Reload] Compiled {} ({} bytes), output saved to disk.",
			hlslPath.filename().string(), result.Bytecode.size());
	}
}

void ShaderHotReloadManager::RecompileAll()
{
	std::error_code ec;
	if (!std::filesystem::exists(m_WatchDirectory, ec)) return;

	for (const auto& entry : std::filesystem::directory_iterator(m_WatchDirectory, ec))
	{
		if (entry.is_regular_file() && entry.path().extension() == ".hlsl")
		{
			ProcessModifiedFile(entry.path());
		}
	}
}

END