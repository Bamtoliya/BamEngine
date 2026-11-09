#include "PostProcessPanel.h"
#include "EditorUI.h"
#include "ImGuiManager.h"
#include "BaseViewportPanel.h"
#include "PostProcessMaterialEffect.h"
#include "IconsFontAwesome7.h"
#include "SceneManager.h"
#include "Entity.h"
#include "EntityFactory.h"
#include "Application.h"
#include "SceneEditHistory.h"
#include "PIEManager.h"
#include "PostProcessComponent.h"
#include "NotificationManager.h"
#include <optional>
#include <string_view>

namespace
{
	string GetEffectDisplayName(const Engine::IPostProcessEffect* effect)
	{
		if (!effect) return "";
		const string& key = effect->GetDisplayNameKey();
		return key.empty() ? WStrToStr(effect->GetName()) : LOCAL(key);
	}

	entt::entity FindActivePostProcessEntity(Engine::Scene& scene)
	{
		auto view = scene.GetRegistry().view<Engine::PostProcessComponent>();
		for (auto [entity, component] : view.each())
		{
			if (component.IsEnabled)
				return entity;
		}
		return entt::null;
	}

	void NotifyPostProcessCaptureFailed()
	{
		NotificationManager::Get().AddNotification(LOCAL("PostProcess.MenuTitle"),
			LOCAL("PostProcess.SceneApplyFailed"), ELogLevel::Error);
	}

	bool ApplyPostProcessToScene(Editor::PostProcessChain& chain, bool merge = false,
		std::optional<std::string_view> presetName = std::nullopt)
	{
		auto* scene = SceneManager::Get().GetCurrentScene();
		auto& history = Application::Get().GetEditHistory();
		if (!scene || !PIEManager::Get().IsEditing() || !history.CanFinalizeEditsFor(scene))
			return true;

		const auto target = FindActivePostProcessEntity(*scene);
		if (target == entt::null)
			return true;

		std::string settings;
		if (!chain.GetPipeline().SaveToString(settings))
		{
			NotifyPostProcessCaptureFailed();
			return false;
		}

		auto& component = scene->GetRegistry().get<PostProcessComponent>(target);
		if (component.PipelineSettings == settings && (!presetName || component.PresetName == *presetName))
			return true;

		const entt::entity targets[] = { target };
		auto before = Editor::SceneEditSnapshot::Capture(*scene, targets);
		if (!before)
		{
			NotifyPostProcessCaptureFailed();
			return false;
		}

		std::string previousPreset;
		if (presetName)
		{
			previousPreset = std::string(*presetName);
			component.PresetName.swap(previousPreset);
		}
		component.PipelineSettings.swap(settings);

		auto after = Editor::SceneEditSnapshot::Capture(*scene, targets);
		if (!after)
		{
			component.PipelineSettings.swap(settings);
			if (presetName)
				component.PresetName.swap(previousPreset);
			NotifyPostProcessCaptureFailed();
			return false;
		}

		history.Record(std::move(before), std::move(after), "PostProcess.ApplyToScene", merge);
		return true;
	}
}

PostProcessPanel::PostProcessPanel()
{
	m_Name = L"Post Process";
	m_DisplayNameKey = "PostProcess.MenuTitle";
}

void PostProcessPanel::Update(f32)
{
	if (!m_Open)
	{
		m_PresetListNeedsRefresh = true;
		return;
	}

	if (!m_PresetListNeedsRefresh)
		return;

	m_PresetListNeedsRefresh = false;
	RefreshPresetList();
}

void PostProcessPanel::Draw()
{
	if (!m_Open)
		return;

	ImGui::SetNextWindowSize(ImVec2(340.f, 480.f), ImGuiCond_FirstUseEver);

	const string windowTitle = LOCAL("PostProcess.MenuTitle") + "###PostProcessPanel";
	if (!BeginPanelWindow(windowTitle.c_str(), m_Open))
	{
		ImGui::End();
		return;
	}

	Application::Get().GetEditHistory().AllowShortcuts(ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows));

	if (!SceneManager::Get().GetCurrentScene())
	{
		m_SelectedEffectIndex = 0;
		m_SelectedPresetName.clear();

		ImGui::TextDisabled("%s", LOCAL_CSTR("EditorDocument.NoSceneLoaded"));
		ImGui::End();
		return;
	}

	// 1. 활성 뷰포트 탐색
	const auto& panels = ImGuiManager::Get().GetImGuiPanels();
	vector<BaseViewportPanel*> viewportsWithPP;
	for (auto* panel : panels)
	{
		if (auto* vp = dynamic_cast<BaseViewportPanel*>(panel))
		{
			if (vp->GetPostProcessChain() != nullptr)
				viewportsWithPP.push_back(vp);
		}
	}

	if (viewportsWithPP.empty())
	{
		ImGui::TextDisabled("%s", LOCAL_CSTR("PostProcess.NoViewport"));
		ImGui::End();
		return;
	}

	if (m_SelectedViewportIndex >= static_cast<int32>(viewportsWithPP.size()))
		m_SelectedViewportIndex = 0;

	// 2. 뷰포트 선택 콤보
	if (viewportsWithPP.size() > 1)
	{
		string currentVPName = viewportsWithPP[m_SelectedViewportIndex]->GetDisplayName();
		if (ImGui::BeginCombo(LOCAL_CSTR("PostProcess.Viewport"), currentVPName.c_str()))
		{
			for (int32 i = 0; i < static_cast<int32>(viewportsWithPP.size()); ++i)
			{
				const bool isSelected = (m_SelectedViewportIndex == i);
				if (ImGui::Selectable(viewportsWithPP[i]->GetDisplayName().c_str(), isSelected))
					m_SelectedViewportIndex = i;
				if (isSelected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		ImGui::Separator();
	}

	PostProcessChain* chain = viewportsWithPP[m_SelectedViewportIndex]->GetPostProcessChain();

	if (!chain)
	{
		ImGui::End();
		return;
	}

	if (const auto restored = chain->TakeSceneRestoreResult(); restored && *restored)
	{
		m_SelectedPresetName.clear();
		m_SelectedEffectIndex = 0;
	}

	// 2.5 상단: 프리셋 저장 및 불러오기 툴바
	DrawPresetToolbar(chain);
	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	// 3. 중단: 파이프라인 체인 순서/활성화 테이블
	DrawPipelineList(chain);

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	// 4. 하단: 선택된 효과의 세부 파라미터 인스펙터
	auto& effects = chain->GetPipeline().GetEffects();
	if (m_SelectedEffectIndex >= 0 && m_SelectedEffectIndex < static_cast<int32>(effects.size()))
	{
		DrawEffectInspector(*chain, effects[m_SelectedEffectIndex]);
	}
	else
	{
		ImGui::TextDisabled("%s", LOCAL_CSTR("PostProcess.SelectEffectPrompt"));
	}

	DrawSceneApplyButton(chain);

	ImGui::End();
}

void PostProcessPanel::RefreshPresetList()
{
	std::error_code error;
	std::filesystem::directory_iterator iterator(PresetDirectory, error);
	const std::filesystem::directory_iterator end;

	if (error == std::errc::no_such_file_or_directory)
	{
		m_AvailablePresets.clear();
		return;
	}

	std::vector<std::string> presets;

	while (!error && iterator != end)
	{
		const bool isFile = iterator->is_regular_file(error);
		if (error)
			break;

		if (isFile && iterator->path().extension() == ".json")
		{
			const auto name = iterator->path().stem().u8string();
			presets.emplace_back(name.begin(), name.end());
		}

		iterator.increment(error);
	}

	if (error)
	{
		EDITOR_LOG_ERROR("Could not refresh post-process preset list: {}", error.message());

		NotificationManager::Get().AddNotification(LOCAL("PostProcess.MenuTitle"),
			LOCAL("PostProcess.PresetListFailed"), ELogLevel::Error);
		return;
	}

	std::sort(presets.begin(), presets.end());
	m_AvailablePresets.swap(presets);
}

void PostProcessPanel::DrawPipelineList(PostProcessChain* chain)
{
	ImGui::TextUnformatted(LOCAL_CSTR("PostProcess.Effects"));

	auto& effects = chain->GetPipeline().GetEffects();
	bool changed = false;
	if (effects.empty())
	{
		ImGui::TextDisabled("%s", LOCAL_CSTR("PostProcess.NoEffects"));
		return;
	}

	const float listHeight = ImMin(180.f, ImGui::GetTextLineHeightWithSpacing() * (effects.size() + 2) + 10.f);
	if (!ImGui::BeginChild("##PipelineListFrame", ImVec2(0.f, listHeight), true))
	{
		ImGui::EndChild();
		return;
	}

	static const ImGuiTableFlags tableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_Resizable;
	if (ImGui::BeginTable("##PipelineTable", 2, tableFlags))
	{
		const std::string colActive = LOCAL("PostProcess.Active") + "##Active";
		const std::string colName = LOCAL("PostProcess.Name") + "##Name";
		ImGui::TableSetupColumn(colActive.c_str(), ImGuiTableColumnFlags_WidthFixed, ImGui::GetFrameHeight());
		ImGui::TableSetupColumn(colName.c_str(), ImGuiTableColumnFlags_WidthStretch);

		int32 dragSourceIdx = -1;
		int32 dragTargetIdx = -1;

		for (int32 i = 0; i < static_cast<int32>(effects.size()); ++i)
		{
			auto* effect = effects[i];
			if (!effect) continue;

			ImGui::PushID(i);
			ImGui::TableNextRow();

			// ── Column 0: On/Off 체크박스 ──
			ImGui::TableSetColumnIndex(0);
			bool enabled = effect->IsEnabled();
			if (ImGui::Checkbox("##enabled", &enabled))
			{
				effect->SetEnabled(enabled);
				changed = true;
			}

			// ── Column 1: 효과 이름 (선택 및 드래그 앤 드롭) ──
			ImGui::TableSetColumnIndex(1);
			const bool isSelected = (m_SelectedEffectIndex == i);
			const string effectName = GetEffectDisplayName(effect);

			if (ImGui::Selectable(effectName.c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns))
			{
				m_SelectedEffectIndex = i;
			}

			// 드래그 소스
			if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoDisableHover))
			{
				ImGui::SetDragDropPayload("PP_EFFECT_REORDER", &i, sizeof(int32));
				ImGui::Text("%s", effectName.c_str());
				ImGui::EndDragDropSource();
			}

			// 드롭 타깃
			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PP_EFFECT_REORDER"))
				{
					dragSourceIdx = *reinterpret_cast<const int32*>(payload->Data);
					dragTargetIdx = i;
				}
				ImGui::EndDragDropTarget();
			}

			ImGui::PopID();
		}

		// 드래그 앤 드롭 순서 변경 반영
		if (dragSourceIdx >= 0 && dragTargetIdx >= 0 && dragSourceIdx != dragTargetIdx)
		{
			auto* movingEffect = effects[dragSourceIdx];
			effects.erase(effects.begin() + dragSourceIdx);
			effects.insert(effects.begin() + dragTargetIdx, movingEffect);
			m_SelectedEffectIndex = dragTargetIdx;
			changed = true;
		}

		ImGui::EndTable();
	}

	ImGui::EndChild();

	// ── 순서 조정 버튼 ──
	const bool canMoveUp = (m_SelectedEffectIndex > 0 && m_SelectedEffectIndex < static_cast<int32>(effects.size()));
	const bool canMoveDown = (m_SelectedEffectIndex >= 0 && m_SelectedEffectIndex < static_cast<int32>(effects.size()) - 1);
	const ImVec2 btnSize(ImGui::GetContentRegionAvail().x * 0.5f - 4.f, 0.f);

	if (!canMoveUp) ImGui::BeginDisabled(true);
	const string moveUpText = string(ICON_FA_ARROW_UP " ") + LOCAL_LABEL("PostProcess.MoveEffectUp");
	if (ImGui::Button(moveUpText.c_str(), btnSize))
	{
		std::swap(effects[m_SelectedEffectIndex], effects[m_SelectedEffectIndex - 1]);
		m_SelectedEffectIndex--;
		changed = true;
	}
	if (!canMoveUp) ImGui::EndDisabled();

	ImGui::SameLine();

	if (!canMoveDown) ImGui::BeginDisabled(true);
	const string moveDownText = string(ICON_FA_ARROW_DOWN " ") + LOCAL_LABEL("PostProcess.MoveEffectDown");
	if (ImGui::Button(moveDownText.c_str(), btnSize))
	{
		std::swap(effects[m_SelectedEffectIndex], effects[m_SelectedEffectIndex + 1]);
		m_SelectedEffectIndex++;
		changed = true;
	}
	if (!canMoveDown) ImGui::EndDisabled();

	if (changed)
		ApplyPostProcessToScene(*chain);
}

void PostProcessPanel::DrawEffectInspector(PostProcessChain& chain, Engine::IPostProcessEffect* effect)
{
	if (!effect) return;

	const string effectTitle = GetEffectDisplayName(effect);
	const string resetAllLabel = string(ICON_FA_ROTATE_LEFT " ") + LOCAL_LABEL("EditorCommon.Reset");
	const float resetAllBtnWidth = ImGui::CalcTextSize(resetAllLabel.c_str()).x + ImGui::GetStyle().FramePadding.x * 2.f + 10.f;

	// 상단 헤더: 효과명 + Reset All 버튼
	ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", effectTitle.c_str());
	ImGui::SameLine(ImGui::GetContentRegionAvail().x - resetAllBtnWidth);

	if (ImGui::Button(resetAllLabel.c_str(), ImVec2(resetAllBtnWidth, 0.f)))
	{
		effect->ResetAllParameters();
		ApplyPostProcessToScene(chain);
	}
	ImGui::Separator();

	const auto& params = effect->GetParameters();
	if (params.empty())
	{
		ImGui::TextDisabled("%s", LOCAL_CSTR("PostProcess.NoParameters"));
		return;
	}

	// ── 3열 테이블: [0: 라벨(35%)] | [1: 컨트롤(65%)] | [2: 리셋 버튼(고정)] ──
	static const ImGuiTableFlags paramTableFlags = ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoBordersInBody;
	if (ImGui::BeginTable("##ParamTable", 3, paramTableFlags))
	{
		const std::string colLabel = LOCAL("PostProcess.Label") + "##Label";
		const std::string colValue = LOCAL("PostProcess.Value") + "##Value";
		const std::string colReset = LOCAL("PostProcess.Reset") + "##Reset";
		ImGui::TableSetupColumn(colLabel.c_str(), ImGuiTableColumnFlags_WidthStretch, 0.35f);
		ImGui::TableSetupColumn(colValue.c_str(), ImGuiTableColumnFlags_WidthStretch, 0.65f);
		ImGui::TableSetupColumn(colReset.c_str(), ImGuiTableColumnFlags_WidthFixed, ImGui::GetFrameHeight());

		const auto applyParameter = [&](bool changed, auto setValue, bool continuous = true)
		{
			auto& history = Application::Get().GetEditHistory();
			const bool finished = continuous && ImGui::IsItemDeactivatedAfterEdit();
			if (continuous && ImGui::IsItemActivated())
				history.Flush();
			if (changed)
			{
				setValue();
				ApplyPostProcessToScene(chain, continuous && (ImGui::IsItemActive() || finished));
			}
			if (finished)
				history.Flush();
		};

		for (const auto& [name, param] : params)
		{
			ImGui::PushID(name.c_str());
			ImGui::TableNextRow();

			const string label = param.displayNameKey.empty() ? name : LOCAL(param.displayNameKey);

			// ── Col 0: 파라미터 라벨 ──
			ImGui::TableSetColumnIndex(0);
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted(label.c_str());

			// ── Col 1: 입력 컨트롤 ──
			ImGui::TableSetColumnIndex(1);
			ImGui::SetNextItemWidth(-1.f);

			switch (param.type)
			{
			case Engine::EMaterialParameterType::Float:
			{
				float val = param.data.size() >= sizeof(float) ? *reinterpret_cast<const float*>(param.data.data()) : 0.0f;
				applyParameter(ImGui::SliderFloat("##val", &val, param.minValue, param.maxValue, "%.2f"),
					[&] { effect->SetFloat(name, val); });
				break;
			}
			case Engine::EMaterialParameterType::Float2:
			{
				vec2 val = param.data.size() >= sizeof(vec2) ? *reinterpret_cast<const vec2*>(param.data.data()) : vec2(0.0f, 0.0f);
				applyParameter(ImGui::SliderFloat2("##val", &val.x, param.minValue, param.maxValue, "%.2f"),
					[&] { effect->SetFloat2(name, val); });
				break;
			}
			case Engine::EMaterialParameterType::Float3:
			{
				vec3 val = param.data.size() >= sizeof(vec3) ? *reinterpret_cast<const vec3*>(param.data.data()) : vec3(0.0f, 0.0f, 0.0f);
				if (param.isColor)
				{
					applyParameter(ImGui::ColorEdit3("##val", &val.x, ImGuiColorEditFlags_NoInputs),
						[&] { effect->SetFloat3(name, val); });
				}
				else
				{
					applyParameter(ImGui::SliderFloat3("##val", &val.x, param.minValue, param.maxValue, "%.2f"),
						[&] { effect->SetFloat3(name, val); });
				}
				break;
			}
			case Engine::EMaterialParameterType::Float4:
			{
				vec4 val = param.data.size() >= sizeof(vec4) ? *reinterpret_cast<const vec4*>(param.data.data()) : vec4(0.0f, 0.0f, 0.0f, 0.0f);
				if (param.isColor)
				{
					applyParameter(ImGui::ColorEdit4("##val", &val.x, ImGuiColorEditFlags_NoInputs),
						[&] { effect->SetFloat4(name, val); });
				}
				else
				{
					applyParameter(ImGui::SliderFloat4("##val", &val.x, param.minValue, param.maxValue, "%.2f"),
						[&] { effect->SetFloat4(name, val); });
				}
				break;
			}
			case Engine::EMaterialParameterType::Int:
			{
				int val = param.data.size() >= sizeof(int) ? *reinterpret_cast<const int*>(param.data.data()) : 0;
				applyParameter(ImGui::SliderInt("##val", &val, static_cast<int>(param.minValue), static_cast<int>(param.maxValue)),
					[&] { effect->SetInt(name, val); });
				break;
			}
			case Engine::EMaterialParameterType::Bool:
			{
				bool val = param.data.size() >= sizeof(bool) ? *reinterpret_cast<const bool*>(param.data.data()) : false;
				applyParameter(ImGui::Checkbox("##val", &val), [&] { effect->SetBool(name, val); }, false);
				break;
			}
			default:
				ImGui::TextDisabled("%s", LOCAL_CSTR("PostProcess.Unsupported"));
				break;
			}

			// ── Col 2: 개별 리셋 버튼 ──
			ImGui::TableSetColumnIndex(2);
			const bool isDefault = (param.data == param.defaultValue);
			if (isDefault) ImGui::BeginDisabled(true);

			if (ImGui::Button(ICON_FA_ROTATE_LEFT, ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight())))
			{
				effect->ResetParameter(name);
				ApplyPostProcessToScene(chain);
			}
			if (isDefault) ImGui::EndDisabled();

			ImGui::PopID();
		}

		ImGui::EndTable();
	}
}

void PostProcessPanel::DrawPresetToolbar(PostProcessChain* chain)
{
	if (!chain) return;
	const string savePopupTitle = LOCAL("PostProcess.SavePresetTitle") + "###PostProcessSavePreset";
	const string deletePopupTitle = LOCAL("PostProcess.DeletePreset") + "###DeletePresetModal";

	const std::string presetDir = PresetDirectory;

	EditorUI::ToolbarLayout heading;
	heading.Text(LOCAL_CSTR("PostProcess.PreviewPreset"), false);
	if (heading.Icon("RefreshPresets", ICON_FA_ARROWS_ROTATE, "EditorCommon.Refresh"))
		m_PresetListNeedsRefresh = true;

	EditorUI::ToolbarLayout toolbar;
	const float buttonWidth = EditorUI::GetIconButtonSize(ICON_FA_FLOPPY_DISK).x +
		EditorUI::GetIconButtonSize(ICON_FA_TRASH_CAN).x + ImGui::GetStyle().ItemSpacing.x * 2.0f;
	toolbar.SetNextItemWidth(std::max(ImGui::GetFontSize() * 8.0f, ImGui::GetContentRegionAvail().x - buttonWidth));
	const char* currentPresetName = m_SelectedPresetName.empty()
		? LOCAL_CSTR("PostProcess.SelectPresetPrompt") : m_SelectedPresetName.c_str();

	if (ImGui::BeginCombo("##PresetCombo", currentPresetName))
	{
		for (const auto& presetName : m_AvailablePresets)
		{
			const bool isSelected = m_SelectedPresetName == presetName;

			if (ImGui::Selectable(presetName.c_str(), isSelected))
			{
				const auto fullPath = StrToWStr(presetDir + "/" + presetName + ".json");

				if (chain->GetPipeline().LoadFromFile(fullPath))
				{
					if (ApplyPostProcessToScene(*chain, false, std::string_view(presetName)))
						m_SelectedPresetName = presetName;
				}
				else
				{
					EDITOR_LOG_ERROR("Could not load post-process preset: {}", presetName);

					NotificationManager::Get().AddNotification(LOCAL("PostProcess.MenuTitle"),
						LOCAL_FMT("PostProcess.PresetLoadFailed", presetName), ELogLevel::Error);
				}
			}

			if (isSelected)
				ImGui::SetItemDefaultFocus();
		}

		ImGui::EndCombo();
	}

	// 저장 버튼
	if (toolbar.Icon("SavePresetBtn", ICON_FA_FLOPPY_DISK, "PostProcess.SavePresetTitle"))
	{
		ImGui::OpenPopup(savePopupTitle.c_str());
	}

	// 삭제 버튼 (선택된 프리셋이 있고, 기본 템플릿인 "Default"가 아닌 경우만 활성화)
	const auto selected = std::find(m_AvailablePresets.begin(), m_AvailablePresets.end(), m_SelectedPresetName);
	const bool canDelete = selected != m_AvailablePresets.end() && m_SelectedPresetName != "Default";

	EditorUI::IconButtonOptions deleteOptions;
	deleteOptions.Enabled = canDelete;
	if (toolbar.Icon("DeletePresetBtn", ICON_FA_TRASH_CAN, "PostProcess.DeletePreset", deleteOptions))
	{
		ImGui::OpenPopup(deletePopupTitle.c_str());
	}

	// ── 2열: 기본값으로 전체 초기화 버튼 ──
	ImGui::Spacing();
	const string resetPipelineLabel = string(ICON_FA_ROTATE_LEFT " ") + LOCAL_LABEL("PostProcess.ResetPipeline");
	if (ImGui::Button(resetPipelineLabel.c_str(), ImVec2(ImGui::GetContentRegionAvail().x, 0.f)))
	{
		const std::wstring defaultPath = StrToWStr(presetDir + "/Default.json");
		std::error_code error;
		const bool hasDefault = std::filesystem::exists(defaultPath, error);
		bool reset = !error;
		if (hasDefault)
		{
			reset = chain->GetPipeline().LoadFromFile(defaultPath);
		}
		else if (reset)
		{
			// Default.json이 없으면 모든 효과 파라미터를 개별 리셋
			for (auto* effect : chain->GetPipeline().GetEffects())
			{
				if (effect) effect->ResetAllParameters();
			}
		}
		if (!reset)
		{
			NotificationManager::Get().AddNotification(LOCAL("PostProcess.MenuTitle"),
				LOCAL_FMT("PostProcess.PresetLoadFailed", "Default"), ELogLevel::Error);
		}
		else if (ApplyPostProcessToScene(*chain, false, std::string_view{}))
			m_SelectedPresetName.clear();
	}

	// ── 프리셋 저장 모달 팝업 ──
	if (ImGui::BeginPopupModal(savePopupTitle.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextUnformatted(LOCAL_CSTR("PostProcess.EnterPresetName"));
		ImGui::InputText("##PresetNameInput", m_NewPresetName, sizeof(m_NewPresetName));

		const std::string presetName = m_NewPresetName;
		const bool validName = !presetName.empty() && presetName != "." && presetName != ".." &&
			presetName.find_first_of("<>:\"/\\|?*") == std::string::npos &&
			presetName.back() != '.' && presetName.back() != ' ' &&
			std::none_of(presetName.begin(), presetName.end(), [](unsigned char value) { return value < 32; });

		if (!validName)
			ImGui::TextWrapped("%s", LOCAL_CSTR("PostProcess.InvalidPresetName"));

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		const EditorUI::DialogAction actions[] = {
			{ "EditorCommon.Cancel" }, { "EditorCommon.Save", validName }
		};
		const int action = EditorUI::DialogButtonBar("SavePresetActions", actions, IM_ARRAYSIZE(actions));
		if (action == 1)
		{
			const auto fullPath = StrToWStr(presetDir + "/" + presetName + ".json");

			std::error_code error;
			std::filesystem::create_directories(std::filesystem::path(presetDir), error);

			if (!error && chain->GetPipeline().SaveToFile(fullPath))
			{
				m_SelectedPresetName = presetName;
				m_PresetListNeedsRefresh = true;

				NotificationManager::Get().AddNotification(LOCAL("PostProcess.MenuTitle"),
					LOCAL_FMT("PostProcess.PresetSaved", presetName), ELogLevel::Info);

				ImGui::CloseCurrentPopup();
			}
			else
			{
				if (error)
					EDITOR_LOG_ERROR("Could not create post-process preset directory: {}", error.message());

				EDITOR_LOG_ERROR("Could not save post-process preset: {}", presetName);

				NotificationManager::Get().AddNotification(LOCAL("PostProcess.MenuTitle"),
					LOCAL_FMT("PostProcess.PresetSaveFailed", presetName), ELogLevel::Error);
			}
		}


		if (action == 0)
			ImGui::CloseCurrentPopup();

		ImGui::EndPopup();
	}

	// ── 프리셋 삭제 확인 모달 팝업 ──
	if (ImGui::BeginPopupModal(deletePopupTitle.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		if (canDelete)
		{
			const string presetToDelete = m_SelectedPresetName;
			ImGui::Text(LOCAL_CSTR("PostProcess.ConfirmDelete"), presetToDelete.c_str());

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			const EditorUI::DialogAction actions[] = {
				{ "EditorCommon.Cancel" }, { "EditorCommon.Delete", true, true }
			};
			const int action = EditorUI::DialogButtonBar("DeletePresetActions", actions, IM_ARRAYSIZE(actions));
			if (action == 1)
			{
				const std::filesystem::path delPath = StrToWStr(presetDir + "/" + presetToDelete + ".json");
				std::error_code error;
				std::filesystem::remove(delPath, error);

				if (error)
				{
					EDITOR_LOG_ERROR("Could not delete post-process preset '{}': {}", presetToDelete, error.message());

					NotificationManager::Get().AddNotification(LOCAL("PostProcess.MenuTitle"),
						LOCAL_FMT("PostProcess.PresetDeleteFailed", presetToDelete), ELogLevel::Error);
				}
				else
				{
					m_SelectedPresetName.clear();
					m_PresetListNeedsRefresh = true;
					ImGui::CloseCurrentPopup();
				}
			}
			if (action == 0)
			{
				ImGui::CloseCurrentPopup();
			}
		}
		else
		{
			m_SelectedPresetName.clear();
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

void PostProcessPanel::DrawSceneApplyButton(PostProcessChain* chain)
{
	Scene* scene = SceneManager::Get().GetCurrentScene();

	if (!scene || !chain)
		return;

	auto& registry = scene->GetRegistry();
	const auto target = FindActivePostProcessEntity(*scene);
	auto& history = Application::Get().GetEditHistory();
	const bool canEdit = PIEManager::Get().IsEditing() && history.CanFinalizeEditsFor(scene);
	const bool canApply = target != entt::null && canEdit;

	ImGui::Separator();

	if (target != entt::null)
	{
		const auto& component = registry.get<PostProcessComponent>(target);

		if (!component.PipelineSettings.empty())
		{
			ImGui::TextWrapped("%s", LOCAL_CSTR("PostProcess.SceneSourceInline"));
		}
		else if (!component.PresetName.empty())
		{
			ImGui::TextWrapped("%s",
				LOCAL_FMT("PostProcess.SceneSourcePreset", component.PresetName).c_str());
		}
		else
		{
			ImGui::TextWrapped("%s", LOCAL_CSTR("PostProcess.SceneSourceDefault"));
		}
	}
	else
	{
		ImGui::TextDisabled("%s", LOCAL_CSTR("PostProcess.NoActiveComponent"));
		if (canEdit)
		{
			ImGui::Spacing();
			if (ImGui::Button(LOCAL_LABEL("PostProcess.CreateVolume").c_str()))
			{
				std::string settings;
				if (chain->GetPipeline().SaveToString(settings))
				{
					SceneEditScope edit(history, *scene, "PostProcess.CreateVolume");
					Entity& newEntity = EntityFactory::CreateEmptyEntity(scene, L"PostProcessVolume");
					auto& ppComp = scene->AddComponent<PostProcessComponent>(newEntity);
					ppComp.IsEnabled = true;
					ppComp.IsUnbound = true;
					ppComp.PipelineSettings = std::move(settings);

					NotificationManager::Get().AddNotification(LOCAL("PostProcess.MenuTitle"),
						LOCAL("PostProcess.VolumeCreated"), ELogLevel::Info);
				}
				else
					NotifyPostProcessCaptureFailed();
			}
		}
	}

	if (history.HasUnsavedChanges())
		ImGui::TextUnformatted(LOCAL_CSTR("EditorDocument.Modified"));

	ImGui::TextWrapped("%s", LOCAL_CSTR("PostProcess.ApplyToSceneHint"));

	ImGui::BeginDisabled(!canApply);

	if (ImGui::Button(LOCAL_LABEL("PostProcess.ApplyToScene").c_str()) && ApplyPostProcessToScene(*chain))
	{
		NotificationManager::Get().AddNotification(LOCAL("PostProcess.MenuTitle"),
			LOCAL("PostProcess.AppliedToScene"), ELogLevel::Info);
	}

	ImGui::SameLine();

	if (ImGui::Button(LOCAL_LABEL("PostProcess.RestoreSceneSettings").c_str()))
		chain->RequestSceneRestore(*scene, target);

	ImGui::EndDisabled();
}
