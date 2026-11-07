#pragma once
#include "ImGuiInterface.h"
#include "EditorSettings.h"
#include "ImGuiManager.h"

EResult IImGui::Initialize(void* arg)
{
	if (m_Registered)
		return EResult::AlreadyInitialized;

	if (!ImGuiManager::IsCreated())
		return EResult::Fail;

	const EResult initResult = OnInitialize(arg);
	if (initResult != EResult::Success)
		return initResult;

	const EResult registerResult = ImGuiManager::Get().AddImGuiPanel(this);
	m_Registered = (registerResult == EResult::Success);
	return registerResult;
}

void IImGui::CaptureState(EditorWorkspace& workspace) const
{
	workspace.PanelOpenStates[GetName()] = IsOpen();
}

void IImGui::RestoreState(const EditorWorkspace& workspace)
{
	const auto found = workspace.PanelOpenStates.find(GetName());

	if (found != workspace.PanelOpenStates.end())
		SetOpen(found->second);
}

bool IImGui::BeginPanelWindow(const char* title, bool& open, ImGuiWindowFlags flags)
{
	if (m_FocusRequested)
	{
		ImGui::SetNextWindowFocus();
		m_FocusRequested = false;
	}

	const bool visible = ImGui::Begin(title, &open, flags);
	m_PanelWindowId = ImGui::GetCurrentWindowRead()->ID;

	if (m_CloseRequestFrame == ImGui::GetFrameCount())
		open = false;

	m_CloseRequestFrame = -1;
	return visible;
}