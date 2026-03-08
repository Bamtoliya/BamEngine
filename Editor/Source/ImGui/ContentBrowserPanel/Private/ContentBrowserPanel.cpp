#pragma once
#include "ContentBrowserPanel.h"

void ContentBrowserPanel::Draw()
{
	if (!m_Open) return;
	ImGui::Begin("Content Browser", &m_Open);

	ImGui::End();
}
