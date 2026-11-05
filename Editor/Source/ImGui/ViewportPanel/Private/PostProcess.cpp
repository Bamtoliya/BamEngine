#include "PostProcess.h"
#include "PostProcessMaterialEffect.h"
#include "ImGuiManager.h"

void PostProcessChain::Free()
{
	m_SceneRestoreRequest.reset();
	m_SceneRestoreResult.reset();
	m_Pipeline.Free();
}

void PostProcessChain::DrawImGuiMenu()
{
	if (ImGui::BeginMenu(LOCAL_LABEL("PostProcess.MenuTitle").c_str()))
	{
		// 1. PostProcessPanel 열기 바로가기
		if (ImGui::MenuItem(LOCAL_LABEL("PostProcess.OpenPanel").c_str()))
		{
			if (auto* panel = ImGuiManager::Get().GetImGuiPanel(L"Post Process"))
				panel->Open();
		}

		ImGui::Separator();
		ImGui::SeparatorText(LOCAL_CSTR("PostProcess.Effects"));

		// 2. 빠른 On/Off 토글
		auto& effects = m_Pipeline.GetEffects();
		for (size_t i = 0; i < effects.size(); ++i)
		{
			auto* effect = effects[i];
			if (!effect) continue;

			ImGui::PushID(static_cast<int>(i));
			bool enabled = effect->IsEnabled();

			// 에디터 레벨에서 로캘 번역 처리
			const string& key = effect->GetDisplayNameKey();
			const string effectName = key.empty() ? WStrToStr(effect->GetName()) : LOCAL_LABEL(key);

			if (ImGui::MenuItem(effectName.c_str(), nullptr, &enabled))
			{
				effect->SetEnabled(enabled);
			}
			ImGui::PopID();
		}

		ImGui::EndMenu();
	}
}