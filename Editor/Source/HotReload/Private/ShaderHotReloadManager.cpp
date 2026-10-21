#include "ShaderHotReloadManager.h"
#include "ResourceManager.h"
#include "Shader.h"
#include "Renderer.h"
#include "Logger.h"
#include <windows.h>
#include <fstream>
#include <unordered_set>

IMPLEMENT_SINGLETON(ShaderHotReloadManager)

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
				std::filesystem::path fullPath = std::filesystem::path(dir) / filename;
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
	std::vector<std::filesystem::path> targetHlslFiles;
	std::unordered_set<std::string> targetHlslKeys;
	std::unordered_set<std::string> modifiedHeaderKeys;

	{
		std::lock_guard<std::mutex> lock(m_QueueMutex);
		for (auto it = m_PendingFiles.begin(); it != m_PendingFiles.end(); )
		{
			if (std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second).count() >= 300)
			{
				std::filesystem::path p(it->first);
				const std::string ext = ToLowerCopy(p.extension().string());
				if (ext == ".hlsli")
				{
					modifiedHeaderKeys.insert(NormalizePathKey(p));
				}
				else if (ext == ".hlsl")
				{
					if (targetHlslKeys.insert(NormalizePathKey(p)).second)
						targetHlslFiles.push_back(p);
				}
				it = m_PendingFiles.erase(it);
			}
			else
			{
				++it;
			}
		}
	}

	if (!modifiedHeaderKeys.empty())
	{
		std::error_code ec;
		if (std::filesystem::exists(m_WatchDirectory, ec))
		{
			for (const auto& entry : std::filesystem::directory_iterator(m_WatchDirectory, ec))
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
		ENGINE_LOG_INFO("[Shader Hot Reload] Shared header modified (.hlsli). Recompiling {} affected shader(s)...", targetHlslFiles.size());
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
		ENGINE_LOG_ERROR("[Shader Hot Reload] Failed to compile '{}':\n{}",
			hlslPath.filename().string(), result.ErrorMessage);
		// 빨간색 에러 토스트 등록 (8초간 유지)
		AddNotification(LOCAL("Notification.ShaderCompileError"), hlslPath.filename().string() + ":\n" + result.ErrorMessage, true, 8.0f);
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
				// 초록색 성공 토스트 등록 (3초간 유지)
				AddNotification(LOCAL("Notification.ShaderHotReloaded"), LOCAL_FMT("Notification.ReloadedFile", hlslPath.filename().string()), false, 3.0f);
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

void ShaderHotReloadManager::AddNotification(const std::string& title, const std::string& msg, bool isError, float duration)
{
	m_Notifications.push_back({ title, msg, isError, duration });
}

void ShaderHotReloadManager::DrawNotificationUI()
{
	if (m_Notifications.empty()) return;

	const float dt = ImGui::GetIO().DeltaTime;
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	const ImVec2 workPos = viewport->WorkPos;
	const ImVec2 workSize = viewport->WorkSize;

	float currentY = workPos.y + workSize.y - 30.0f;

	for (int i = static_cast<int>(m_Notifications.size()) - 1; i >= 0; --i)
	{
		auto& notif = m_Notifications[i];
		notif.RemainingTime -= dt;
		if (notif.RemainingTime <= 0.0f)
		{
			m_Notifications.erase(m_Notifications.begin() + i);
			continue;
		}

		// 창 위치: 화면 우측 하단에 차곡차곡 배치
		ImGui::SetNextWindowPos(ImVec2(workPos.x + workSize.x - 380.0f, currentY), ImGuiCond_Always, ImVec2(0.0f, 1.0f));
		ImGui::SetNextWindowSize(ImVec2(360.0f, 0.0f));

		// 배경 및 테두리 색상 (에러: 짙은 적색, 성공: 짙은 녹색)
		ImVec4 bgColor = notif.IsError ? ImVec4(0.20f, 0.05f, 0.05f, 0.92f) : ImVec4(0.05f, 0.18f, 0.05f, 0.92f);
		ImVec4 borderColor = notif.IsError ? ImVec4(0.95f, 0.25f, 0.25f, 1.0f) : ImVec4(0.25f, 0.85f, 0.25f, 1.0f);

		ImGui::PushStyleColor(ImGuiCol_WindowBg, bgColor);
		ImGui::PushStyleColor(ImGuiCol_Border, borderColor);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.5f);

		std::string windowName = "##ShaderToast_" + std::to_string(i);
		const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
			ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;

		if (ImGui::Begin(windowName.c_str(), nullptr, flags))
		{
			// 타이틀 및 닫기 [X] 버튼
			ImVec4 titleColor = notif.IsError ? ImVec4(1.0f, 0.4f, 0.4f, 1.0f) : ImVec4(0.4f, 1.0f, 0.4f, 1.0f);
			ImGui::TextColored(titleColor, "%s %s", notif.IsError ? "[!]" : "[V]", notif.Title.c_str());

			ImGui::SameLine(ImGui::GetWindowWidth() - 30.0f);
			if (ImGui::SmallButton("X"))
			{
				notif.RemainingTime = 0.0f;
			}

			ImGui::Separator();
			ImGui::TextWrapped("%s", notif.Message.c_str());

			currentY -= (ImGui::GetWindowHeight() + 10.0f);
		}
		ImGui::End();

		ImGui::PopStyleVar(2);
		ImGui::PopStyleColor(2);
	}
}