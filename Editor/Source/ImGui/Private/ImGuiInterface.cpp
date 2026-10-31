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