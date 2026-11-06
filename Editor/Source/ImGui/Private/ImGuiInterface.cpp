#pragma once
#include "ImGuiInterface.h"
#include "EditorSettings.h"

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
	const bool visible = ImGui::Begin(title, &open, flags);
	m_PanelWindowId = ImGui::GetCurrentWindowRead()->ID;

	if (m_CloseRequestFrame == ImGui::GetFrameCount())
		open = false;

	m_CloseRequestFrame = -1;
	return visible;
}