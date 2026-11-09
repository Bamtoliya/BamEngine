#include "RecoveryManager.h"
#include "EditorUI.h"
#include "Application.h"
#include "SceneDocument.h"
#include "SceneManager.h"
#include "Scene.h"
#include <archive/JsonArchive.h>
#include "Logger.h"
#include "CrashHandler.h"
#include "LocalizationManager.h"
#include "IconsFontAwesome7.h"
#include <imgui.h>
#include <fstream>
#include <chrono>
#include <sstream>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

BEGIN(Editor)

IMPLEMENT_SINGLETON(RecoveryManager)

namespace
{
	std::string GetCurrentTimestampString()
	{
		const auto now = std::chrono::system_clock::now();
		const auto time = std::chrono::system_clock::to_time_t(now);
		std::tm tm{};
		::localtime_s(&tm, &time);

		char buf[64];
		std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec);
		return std::string(buf);
	}
}

Engine::EResult RecoveryManager::Initialize(void* arg)
{
	if (!arg) return Engine::EResult::InvalidArgument;
	CAST_DESC;
	m_ProjectRoot = desc->ProjectRoot.empty() ? std::filesystem::current_path() : desc->ProjectRoot;
	m_RecoveryDir = m_ProjectRoot / "Temp/Recovery";
	m_SessionLockPath = m_ProjectRoot / "Temp/Session.lock";
	m_MetadataPath = m_RecoveryDir / "RecoveryMetadata.json";

	std::error_code ec;
	std::filesystem::create_directories(m_RecoveryDir, ec);

	// CrashHandler에 비상 스냅샷 콜백 등록 (Tier 2 Recovery)
	Engine::CrashHandler::RegisterEmergencyCallback([]() {
		if (RecoveryManager::IsCreated())
		{
			RecoveryManager::Get().ExecuteEmergencySnapshot();
		}
	});

	OnStartup();

	return Engine::EResult::Success;
}

void RecoveryManager::Free()
{
	OnShutdown();
	__super::Free();
}

void RecoveryManager::OnStartup()
{
	CheckPreviousSessionAndRecovery();
	CreateSessionLock();
}

void RecoveryManager::OnShutdown()
{
	RemoveSessionLock();
}

void RecoveryManager::CheckPreviousSessionAndRecovery()
{
	std::error_code ec;
	const bool sessionLockExists = std::filesystem::exists(m_SessionLockPath, ec);

	if (sessionLockExists)
	{
		BAM_LOG(Warn, "Editor", "Recovery", "Previous session lock found at: {}. Investigating recovery candidates...",
			m_SessionLockPath.string());

		RecoverySnapshotInfo info;
		if (LoadRecoveryMetadata(info) && std::filesystem::exists(info.SnapshotFilePath, ec))
		{
			m_RecoveryInfo = info;
			m_RecoveryInfo.IsValid = true;
			m_ShowRecoveryModal = true;
			m_ModalTriggered = false;

			BAM_LOG(Info, "Editor", "Recovery", "Valid recovery snapshot detected! Scene: {}, Saved at: {}",
				info.SceneName, info.Timestamp);
		}
		else
		{
			BAM_LOG(Info, "Editor", "Recovery", "Previous session did not leave a recoverable snapshot.");
		}
	}
}

void RecoveryManager::CreateSessionLock()
{
	std::error_code ec;
	std::filesystem::create_directories(m_SessionLockPath.parent_path(), ec);

	std::ofstream lockFile(m_SessionLockPath);
	if (lockFile.is_open())
	{
		lockFile << "{\n";
		lockFile << "  \"Status\": \"Running\",\n";
		lockFile << "  \"PID\": " << GetCurrentProcessId() << ",\n";
		lockFile << "  \"StartTime\": \"" << GetCurrentTimestampString() << "\"\n";
		lockFile << "}\n";
		lockFile.flush();
	}
}

void RecoveryManager::RemoveSessionLock()
{
	std::error_code ec;
	if (std::filesystem::exists(m_SessionLockPath, ec))
	{
		std::filesystem::remove(m_SessionLockPath, ec);
	}
}

void RecoveryManager::MarkSessionCrashed()
{
	std::ofstream lockFile(m_SessionLockPath, std::ios::trunc);
	if (lockFile.is_open())
	{
		lockFile << "{\n";
		lockFile << "  \"Status\": \"Crashed\",\n";
		lockFile << "  \"PID\": " << GetCurrentProcessId() << ",\n";
		lockFile << "  \"CrashTime\": \"" << GetCurrentTimestampString() << "\"\n";
		lockFile << "}\n";
		lockFile.flush();
	}
}

bool RecoveryManager::WriteSnapshotInternal(const std::filesystem::path& targetPath, const std::string& reason)
{
	auto* doc = Application::Get().GetSceneDocument();
	if (!doc)
		return false;

	auto* scene = doc->GetScene();
	if (!scene)
		return false;

	std::error_code ec;
	std::filesystem::create_directories(targetPath.parent_path(), ec);

	archive::JsonArchive writer;
	if (!scene->WriteArchive(writer))
		return false;

	const std::filesystem::path tmpPath = targetPath.string() + ".tmp";
	if (!writer.SaveToFile(tmpPath))
		return false;

	std::filesystem::copy_file(tmpPath, targetPath, std::filesystem::copy_options::overwrite_existing, ec);
	std::filesystem::remove(tmpPath, ec);

	if (ec)
	{
		BAM_LOG(Error, "Editor", "Recovery", "Failed to finalize snapshot file: {}", ec.message());
		return false;
	}

	// 메타데이터 갱신
	RecoverySnapshotInfo info;
	info.IsValid = true;
	info.SnapshotFilePath = targetPath;
	info.OriginalScenePath = doc->GetPath();
	info.SceneName = doc->GetPath().empty() ? "Untitled" : doc->GetPath().filename().string();
	info.Timestamp = GetCurrentTimestampString();
	info.SaveReason = reason;

	SaveRecoveryMetadata(info);
	return true;
}

void RecoveryManager::SaveRecoveryMetadata(const RecoverySnapshotInfo& info)
{
	std::error_code ec;
	std::filesystem::create_directories(m_MetadataPath.parent_path(), ec);

	std::ofstream metaFile(m_MetadataPath);
	if (metaFile.is_open())
	{
		metaFile << "{\n";
		metaFile << "  \"SnapshotFilePath\": \"" << info.SnapshotFilePath.generic_string() << "\",\n";
		metaFile << "  \"OriginalScenePath\": \"" << info.OriginalScenePath.generic_string() << "\",\n";
		metaFile << "  \"SceneName\": \"" << info.SceneName << "\",\n";
		metaFile << "  \"Timestamp\": \"" << info.Timestamp << "\",\n";
		metaFile << "  \"SaveReason\": \"" << info.SaveReason << "\"\n";
		metaFile << "}\n";
		metaFile.flush();
	}
}

bool RecoveryManager::LoadRecoveryMetadata(RecoverySnapshotInfo& outInfo)
{
	std::ifstream metaFile(m_MetadataPath);
	if (!metaFile.is_open())
		return false;

	std::string line;
	while (std::getline(metaFile, line))
	{
		auto parseProp = [](const std::string& l, const std::string& key) -> std::string {
			size_t keyPos = l.find("\"" + key + "\"");
			if (keyPos == std::string::npos) return "";
			size_t colonPos = l.find(':', keyPos);
			if (colonPos == std::string::npos) return "";
			size_t firstQuote = l.find('\"', colonPos);
			if (firstQuote == std::string::npos) return "";
			size_t secondQuote = l.find('\"', firstQuote + 1);
			if (secondQuote == std::string::npos) return "";
			return l.substr(firstQuote + 1, secondQuote - firstQuote - 1);
		};

		std::string val = parseProp(line, "SnapshotFilePath");
		if (!val.empty()) outInfo.SnapshotFilePath = val;

		val = parseProp(line, "OriginalScenePath");
		if (!val.empty()) outInfo.OriginalScenePath = val;

		val = parseProp(line, "SceneName");
		if (!val.empty()) outInfo.SceneName = val;

		val = parseProp(line, "Timestamp");
		if (!val.empty()) outInfo.Timestamp = val;

		val = parseProp(line, "SaveReason");
		if (!val.empty()) outInfo.SaveReason = val;
	}

	return !outInfo.SnapshotFilePath.empty();
}

void RecoveryManager::Update(float dt)
{
	if (!m_AutoSaveEnabled)
		return;

	auto* doc = Application::Get().GetSceneDocument();
	if (!doc || !doc->HasUnsavedChanges())
	{
		m_ElapsedTime = 0.0f;
		return;
	}

	m_ElapsedTime += dt;
	if (m_ElapsedTime >= m_AutoSaveIntervalSeconds)
	{
		m_ElapsedTime = 0.0f;
		ExecuteAutoSave();
	}
}

bool RecoveryManager::ExecuteAutoSave()
{
	auto* doc = Application::Get().GetSceneDocument();
	if (!doc || !doc->HasUnsavedChanges())
		return false;

	const std::string sceneStem = doc->GetPath().empty() ? "Untitled" : doc->GetPath().stem().string();
	const std::filesystem::path target = m_RecoveryDir / ("AutoSave_" + sceneStem + ".bamscene.json");

	const bool success = WriteSnapshotInternal(target, "AutoSave");
	if (success)
	{
		BAM_LOG(Info, "Editor", "Recovery", "Heartbeat Auto-Save snapshot saved: {}", target.string());
	}
	return success;
}

void RecoveryManager::ExecuteEmergencySnapshot()
{
	MarkSessionCrashed();

	const std::filesystem::path emergencyPath = m_RecoveryDir / "Emergency_Snapshot.bamscene.json";
	WriteSnapshotInternal(emergencyPath, "EmergencyCrash");
}

bool RecoveryManager::RestoreSnapshot()
{
	if (!m_RecoveryInfo.IsValid || m_RecoveryInfo.SnapshotFilePath.empty())
		return false;

	archive::JsonArchive reader;
	if (!reader.LoadFromFile(m_RecoveryInfo.SnapshotFilePath))
	{
		BAM_LOG(Error, "Editor", "Recovery", "Failed to load recovery snapshot JSON: {}",
			m_RecoveryInfo.SnapshotFilePath.string());
		return false;
	}

	if (Engine::SceneManager::Get().LoadScene(reader, m_RecoveryInfo.SnapshotFilePath.wstring()) != Engine::EResult::Success)
	{
		BAM_LOG(Error, "Editor", "Recovery", "Failed to parse scene from recovery snapshot.");
		return false;
	}

	// 원본 씬 문서 경로로 재채택하여 사용자가 바로 이어서 저장/작업할 수 있도록 연결
	Application::Get().AdoptSceneDocument(m_RecoveryInfo.OriginalScenePath);

	BAM_LOG(Info, "Editor", "Recovery", "Work restored successfully from: {}. Document adopted as: {}",
		m_RecoveryInfo.SnapshotFilePath.string(), m_RecoveryInfo.OriginalScenePath.string());

	DiscardSnapshot();
	return true;
}

void RecoveryManager::DiscardSnapshot()
{
	m_RecoveryInfo.IsValid = false;
	m_ShowRecoveryModal = false;

	std::error_code ec;
	std::filesystem::remove(m_MetadataPath, ec);
	if (!m_RecoveryInfo.SnapshotFilePath.empty())
	{
		std::filesystem::remove(m_RecoveryInfo.SnapshotFilePath, ec);
	}
}

void RecoveryManager::DrawRecoveryModal()
{
	if (!m_ShowRecoveryModal || !m_RecoveryInfo.IsValid)
		return;

	const char* popupId = "##CrashRecoveryWizardModal";

	if (!m_ModalTriggered)
	{
		ImGui::OpenPopup(popupId);
		m_ModalTriggered = true;
	}

	ImGui::SetNextWindowSize(ImVec2(ImGui::GetFontSize() * 30.0f, 0.0f), ImGuiCond_Appearing);
	if (ImGui::BeginPopupModal(popupId, nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
	{
		// 제목 및 경고 아이콘
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.75f, 0.2f, 1.0f));
		ImGui::Text("%s  %s", ICON_FA_TRIANGLE_EXCLAMATION, LOCAL_CSTR("Recovery.ModalTitle"));
		ImGui::PopStyleColor();

		ImGui::Separator();
		ImGui::Spacing();

		ImGui::TextWrapped("%s", LOCAL_CSTR("Recovery.CrashDetectedMessage"));
		ImGui::Spacing();

		// 스냅샷 정보 표
		if (EditorUI::BeginSettingsTable("##RecoveryInfoTable", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg))
		{
			ImGui::TableSetupColumn(LOCAL_CSTR("EditorCommon.Property"), ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 8.0f);
			ImGui::TableSetupColumn(LOCAL_CSTR("EditorCommon.Value"), ImGuiTableColumnFlags_WidthStretch);

			// 씬 이름
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::TextUnformatted(LOCAL_CSTR("Recovery.PropSceneName"));
			ImGui::TableSetColumnIndex(1);
			ImGui::TextUnformatted(m_RecoveryInfo.SceneName.c_str());

			// 스냅샷 시각
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::TextUnformatted(LOCAL_CSTR("Recovery.PropSavedTime"));
			ImGui::TableSetColumnIndex(1);
			ImGui::TextUnformatted(m_RecoveryInfo.Timestamp.c_str());

			// 저장 사유
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::TextUnformatted(LOCAL_CSTR("Recovery.PropReason"));
			ImGui::TableSetColumnIndex(1);
			const char* reasonText = (m_RecoveryInfo.SaveReason == "EmergencyCrash") ?
				LOCAL_CSTR("Recovery.ReasonEmergency") : LOCAL_CSTR("Recovery.ReasonAutoSave");
			ImGui::TextUnformatted(reasonText);

			EditorUI::EndSettingsTable();
		}

		ImGui::Spacing();
		ImGui::TextWrapped("%s", LOCAL_CSTR("Recovery.PromptQuestion"));
		ImGui::Spacing();

		// 하단 버튼 바 (우측 정렬: [무시하고 새 작업] [복원하여 열기])
		ImGui::Separator();
		ImGui::Spacing();

		const EditorUI::DialogAction actions[] = {
			{ "Recovery.DiscardButton", true, true }, { "Recovery.RestoreButton" }
		};
		EditorUI::DialogButtonBarOptions options;
		options.CancelIndex = -1; // 복구 파일 폐기는 명시적인 버튼 선택으로만 수행한다.
		const int action = EditorUI::DialogButtonBar("RecoveryActions", actions, IM_ARRAYSIZE(actions), options);
		if (action == 0) DiscardSnapshot();
		else if (action == 1) RestoreSnapshot();
		if (action >= 0) ImGui::CloseCurrentPopup();

		ImGui::EndPopup();
	}
}

END
