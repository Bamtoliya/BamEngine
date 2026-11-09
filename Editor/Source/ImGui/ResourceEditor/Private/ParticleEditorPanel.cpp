#include "ParticleEditorPanel.h"
#include "EditorUI.h"
#include "Functions.h"
#include "ResourcePicker.h"
#include "IconsFontAwesome7.h"
#include "WorldComponents.h"
#include "ResourceManager.h"
#include "InputManager.h"
#include "ShortcutManager.h"
#include "Scene.h"
#include "PlatformPath.h"
#include "ParticleSystem.h"
#include "PostProcessPipeline.h"
#include "FileDialogs.h"
#include <algorithm>
#include <cmath>
#include <fmt/format.h>
#include "EditorSettings.h"

namespace
{
	class ParticleEffectCommand final : public EditorCommand
	{
	public:
		ParticleEffectCommand(Engine::ParticleEffect* effect,
			std::vector<Engine::ParticleEmitterData> before,
			std::vector<Engine::ParticleEmitterData> after,
			std::string label,
			std::function<void()> onStateChanged)
			: EditorCommand(std::move(label)), m_Effect(effect),
				m_Before(std::move(before)), m_After(std::move(after)),
				m_OnStateChanged(std::move(onStateChanged)) {}

		bool Execute() override
		{
			if (!m_Effect) return false;
			m_Effect->GetEmitters() = m_After;
			if (m_OnStateChanged) m_OnStateChanged();
			return true;
		}

		bool Undo() override
		{
			if (!m_Effect) return false;
			m_Effect->GetEmitters() = m_Before;
			if (m_OnStateChanged) m_OnStateChanged();
			return true;
		}

	private:
		Engine::ParticleEffect* m_Effect;
		std::vector<Engine::ParticleEmitterData> m_Before;
		std::vector<Engine::ParticleEmitterData> m_After;
		std::function<void()> m_OnStateChanged;
	};

	bool IsParticleEffectFile(const std::filesystem::path& path)
	{
		const auto name = Engine::ToLowerStr(Engine::WStrToStr(path.filename().wstring()));
		return name.ends_with(".bameffect") || name.ends_with(".bameffect.json") ||
				name.ends_with(".effect") || name.ends_with(".effect.json");
	}
}

ParticleEditorPanel::ParticleEditorPanel()
	: m_History(100)
{
	m_Name = L"Particle Editor";
	m_DisplayNameKey = "ResourceEditor.ParticleTitle";
	m_PreviewWidget.Initialize(L"ParticleEditorPreview", 512, 512, true);
	m_PreviewWidget.SetupDefaultStudioLighting();
	m_PreviewWidget.FocusTarget(Engine::vec3(0.0f, 1.0f, 0.0f), 4.5f);

	// 파티클의 emissive / HDR 발광 효과를 위해 Bloom 활성화
	for (auto* effect : m_PreviewWidget.GetPostProcessChain().GetPipeline().GetEffects())
	{
		if (effect && effect->GetName() == L"Bloom")
		{
			effect->SetEnabled(true);
		}
	}
}

ParticleEditorPanel::~ParticleEditorPanel()
{
	m_PreviewWidget.Free();
}

bool ParticleEditorPanel::IsSupported(const std::filesystem::path& assetPath) const
{
	return IsParticleEffectFile(assetPath);
}

void ParticleEditorPanel::SetTargetResource(const std::filesystem::path& assetPath)
{
	ResourceEditorInterface::SetTargetResource(assetPath);
	m_IsDirty = false;
	m_SelectedEmitterIndex = 0;
	m_History.Clear();
	m_Timeline.Drag.reset();
	m_Timeline.Fit();
	m_HasPreEditSnapshot = false;

	const std::wstring relPath = Engine::PlatformPath::ToProjectRelative(assetPath).wstring();
	auto& rm = Engine::ResourceManager::Get();

	m_EffectHandle = rm.GetResourceHandle<Engine::ParticleEffect>(relPath);
	if (!m_EffectHandle.IsValid())
	{
		auto handle = rm.LoadFile(relPath);
		if (handle.IsValid())
			m_EffectHandle = Engine::ResourceHandle<Engine::ParticleEffect>(handle);
	}

	SetupPreview();
}

void ParticleEditorPanel::CaptureState(EditorWorkspace& workspace) const
{
	IImGui::CaptureState(workspace);
	workspace.SetSplitter("ParticleEditor.LeftPaneWidth", m_LeftPaneWidth);
	workspace.SetSplitter("ParticleEditor.RightPaneWidth", m_RightPaneWidth);
	workspace.SetSplitter("ParticleEditor.TimelineHeight", m_TimelineHeight);
	workspace.SetSplitter("ParticleEditor.TimelineHeaderWidth", m_Timeline.HeaderWidth);
}

void ParticleEditorPanel::RestoreState(const EditorWorkspace& workspace)
{
	IImGui::RestoreState(workspace);
	m_LeftPaneWidth = workspace.GetSplitter("ParticleEditor.LeftPaneWidth", m_LeftPaneWidth);
	m_RightPaneWidth = workspace.GetSplitter("ParticleEditor.RightPaneWidth", m_RightPaneWidth);
	m_TimelineHeight = workspace.GetSplitter("ParticleEditor.TimelineHeight", m_TimelineHeight);
	m_Timeline.HeaderWidth = workspace.GetSplitter("ParticleEditor.TimelineHeaderWidth", m_Timeline.HeaderWidth);
}

std::vector<Engine::ParticleEmitterData> ParticleEditorPanel::CaptureSnapshot() const
{
	auto* effect = m_EffectHandle.Get();
	return effect ? effect->GetEmitters() : std::vector<Engine::ParticleEmitterData>{};
}

void ParticleEditorPanel::PushSnapshot(const std::string& label)
{
	auto* effect = m_EffectHandle.Get();
	if (!effect || !m_HasPreEditSnapshot) return;

	auto after = CaptureSnapshot();
	auto cmd = std::make_unique<ParticleEffectCommand>(
		effect, std::move(m_PreEditSnapshot), std::move(after), label,
		[this]() { SetupPreview(); m_IsDirty = true; });
	m_History.Record(std::move(cmd));
	m_HasPreEditSnapshot = false;
	m_IsDirty = true;
}

void ParticleEditorPanel::Undo()
{
	if (!m_History.Undo()) return;
	if (const auto* effect = m_EffectHandle.Get())
		m_SelectedEmitterIndex = effect->GetEmitterCount() > 0 ?
			std::clamp(m_SelectedEmitterIndex, 0, effect->GetEmitterCount() - 1) : -1;
}

void ParticleEditorPanel::Redo()
{
	if (!m_History.Redo()) return;
	if (const auto* effect = m_EffectHandle.Get())
		m_SelectedEmitterIndex = effect->GetEmitterCount() > 0 ?
			std::clamp(m_SelectedEmitterIndex, 0, effect->GetEmitterCount() - 1) : -1;
}

void ParticleEditorPanel::NewEffect()
{
	auto newEffect = Engine::ParticleEffect::Create();
	if (!newEffect) return;

	Engine::ParticleEmitterData defEmitter;
	defEmitter.name = LOCAL("ParticleEditor.NewEmitterName");
	defEmitter.emissionRate = 30.0f;
	defEmitter.lifetimeMin = 1.0f;
	defEmitter.lifetimeMax = 2.0f;
	defEmitter.speedMin = 2.0f;
	defEmitter.speedMax = 5.0f;
	defEmitter.direction = Engine::vec3(0.0f, 1.0f, 0.0f);
	defEmitter.spreadAngleDegrees = 30.0f;
	defEmitter.colorStart = Engine::vec4(1.0f, 0.8f, 0.2f, 1.0f);
	defEmitter.colorEnd = Engine::vec4(1.0f, 0.1f, 0.0f, 0.0f);
	defEmitter.sizeStart = Engine::vec2(0.4f, 0.4f);
	defEmitter.sizeEnd = Engine::vec2(0.1f, 0.1f);
	newEffect->AddEmitter(defEmitter);

	m_TargetResourcePath = "Resources/Effect/NewEffect.bameffect.json";
	const std::wstring relKey = L"Resources/Effect/NewEffect.bameffect.json";
	m_EffectHandle = Engine::ResourceManager::Get().AddResource<Engine::ParticleEffect>(relKey, newEffect);
	m_SelectedEmitterIndex = 0;
	m_IsDirty = true;
	m_History.Clear();
	m_Timeline.Drag.reset();
	m_Timeline.Fit();
	m_HasPreEditSnapshot = false;
	SetupPreview();
}

void ParticleEditorPanel::OpenEffect()
{
	std::wstring openPath;
	if (FileDialogs::OpenFileDialog(openPath, { { StrToWStr(LOCAL("ParticleEditor.FileDialogFilter")), L"*.bameffect.json;*.bameffect" } }, L"", EFileDialogContext::ParticleOpen))
	{
		SetTargetResource(std::filesystem::path(openPath));
	}
}

void ParticleEditorPanel::SaveEffectAs()
{
	std::wstring savePath;
	if (FileDialogs::SaveFileDialog(savePath, { { StrToWStr(LOCAL("ParticleEditor.FileDialogFilter")), L"*.bameffect.json" } }, L"NewEffect.bameffect.json", L"bameffect.json", EFileDialogContext::ParticleSave))
	{
		m_TargetResourcePath = std::filesystem::path(savePath);
		SaveEffect();
	}
}

float ParticleEditorPanel::GetTimelineDuration() const
{
	const auto* effect = m_EffectHandle.Get();
	if (!effect) return 5.0f;
	const float explicitDuration = effect->GetPlaybackDuration();
	if (explicitDuration > 0.0f) return std::max(explicitDuration, EditorUI::MinimumTimelineDuration);
	float duration = 5.0f;
	for (const auto& emitter : effect->GetEmitters())
	{
		const float end = emitter.emissionStartTime + std::max(0.0f, emitter.emissionDuration);
		if (std::isfinite(end)) duration = std::max(duration, end);
	}
	return duration;
}

void ParticleEditorPanel::Update(f32 dt)
{
	if (!m_Open) return;
	auto* scene = m_PreviewWidget.GetScene();
	if (m_PreviewEntity && scene && m_EffectHandle.IsValid())
	{
		auto& comp = scene->GetComponent<Engine::ParticleEffectComponent>(*m_PreviewEntity);
		if (m_TimelineSeek)
		{
			const float target = *m_TimelineSeek;
			comp.Play();
			comp.playRate = 1.0f;
			for (int i = 0; i < 32 && m_SimulatedTime + 0.00001f < target; ++i)
			{
				const float step = std::min(1.0f / EditorUI::EffectTimelineFPS, target - m_SimulatedTime);
				Engine::ParticleSystem::SimulateSceneParticles(scene, step);
				m_SimulatedTime += step;
			}
			if (m_SimulatedTime + 0.00001f >= target) m_TimelineSeek.reset();
			comp.Pause();
		}
		else if (m_IsPlaying)
		{
			comp.Play();
			comp.playRate = m_PlayRate;
			m_FixedPreview = false;
			const float duration = GetTimelineDuration();
			const float delta = std::clamp(duration - m_SimulatedTime, 0.0f, std::max(0.0f, dt * m_PlayRate));
			if (delta > 0.0f)
			{
				Engine::ParticleSystem::SimulateSceneParticles(scene, delta / m_PlayRate);
				m_SimulatedTime += delta;
			}
			if (m_SimulatedTime + 0.00001f >= duration)
			{
				if (m_IsLooping) RestartSimulation();
				else { m_IsPlaying = false; comp.Pause(); }
			}
		}
		else comp.Pause();
	}

	m_PreviewWidget.BeginFrame(dt);
	m_PreviewWidget.Submit(dt);
}

void ParticleEditorPanel::SetupPreview()
{
	auto* scene = m_PreviewWidget.GetScene();
	if (!scene) return;

	if (m_PreviewEntity)
	{
		(void)scene->DestroyEntity(*m_PreviewEntity);
		m_PreviewEntity = nullptr;
	}

	auto* effect = m_EffectHandle.Get();
	if (!effect) return;

	m_PreviewEntity = &m_PreviewWidget.CreateEntity(L"ParticleEffectPreview");
	(void)scene->AddComponent<Engine::FlagComponent>(*m_PreviewEntity);
	auto& comp = scene->AddComponent<Engine::ParticleEffectComponent>(*m_PreviewEntity);
	comp.effectHandle = m_EffectHandle;
	comp.playState = m_IsPlaying ? Engine::EParticlePlayState::Playing : Engine::EParticlePlayState::Paused;
	comp.playRate = m_PlayRate;
	comp.previewSeed = 1;
	RestartSimulation();
}

void ParticleEditorPanel::RestartSimulation()
{
	auto* scene = m_PreviewWidget.GetScene();
	if (m_PreviewEntity && scene)
	{
		auto& comp = scene->GetComponent<Engine::ParticleEffectComponent>(*m_PreviewEntity);
		comp.instances.clear();
		comp.isInitialized = false;
	}
	m_SimulatedTime = 0.0f;
	m_TimelineSeek.reset();
	m_FixedPreview = true;
}

void ParticleEditorPanel::ScrubTime(f32 targetTime)
{
	const float duration = GetTimelineDuration();
	if (!std::isfinite(targetTime)) return;
	if (targetTime < duration)
		targetTime = std::round(targetTime * EditorUI::EffectTimelineFPS) / EditorUI::EffectTimelineFPS;
	targetTime = std::clamp(targetTime, 0.0f, duration);
	m_IsPlaying = false;
	if (!m_FixedPreview || targetTime + 0.0001f < m_SimulatedTime) RestartSimulation();
	m_TimelineSeek = targetTime;
}

void ParticleEditorPanel::InvalidateTimelinePreview()
{
	m_FixedPreview = false;
	ScrubTime(m_TimelineSeek.value_or(m_SimulatedTime));
	m_IsDirty = true;
}

void ParticleEditorPanel::SaveEffect()
{
	auto* effect = m_EffectHandle.Get();
	if (!effect) return;

	if (m_TargetResourcePath.empty())
	{
		SaveEffectAs();
		return;
	}

	auto fullPath = Engine::PlatformPath::ResolveProjectRelative(m_TargetResourcePath);
	if (effect->SaveToFile(fullPath.wstring()))
	{
		m_IsDirty = false;
		m_StatusMessage = LOCAL("ResourceEditor.SaveSuccess");
	}
	else
	{
		m_StatusMessage = LOCAL("ResourceEditor.SaveFailed");
	}
}

void ParticleEditorPanel::DrawCustomContent()
{
	EditorUI::DrawEffectEditorLayout(m_LeftPaneWidth, m_RightPaneWidth, m_TimelineHeight,
		[this] { DrawEmitterList(); }, [this] { DrawViewport(); },
		[this] { DrawEmitterInspector(); }, [this] { DrawTimeline(); });
}

void ParticleEditorPanel::DrawCustomToolbar()
{
	const bool hasResource = m_EffectHandle.Get() != nullptr;

	if (ImGui::BeginMenu(LOCAL_CSTR("EditorMenu.File")))
	{
		if (ImGui::MenuItem(LOCAL_CSTR("ParticleEditor.MenuNew")))
			NewEffect();
		if (ImGui::MenuItem(LOCAL_CSTR("ParticleEditor.MenuOpen")))
			OpenEffect();
		ImGui::Separator();
		if (ImGui::MenuItem(LOCAL_LABEL("EditorCommon.Save").c_str(), ShortcutManager::Get().GetShortcutHint("ParticleTimeline.Save"), false, hasResource && m_IsDirty))
			SaveEffect();
		if (ImGui::MenuItem(LOCAL_CSTR("ParticleEditor.MenuSaveAs"), nullptr, false, hasResource))
			SaveEffectAs();
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu(LOCAL_CSTR("EditorMenu.Edit")))
	{
		if (ImGui::MenuItem(LOCAL_LABEL("ParticleEditor.MenuUndo").c_str(), ShortcutManager::Get().GetShortcutHint("ParticleTimeline.Undo"), false, m_History.CanUndo()))
			Undo();
		if (ImGui::MenuItem(LOCAL_LABEL("ParticleEditor.MenuRedo").c_str(), ShortcutManager::Get().GetShortcutHint("ParticleTimeline.Redo"), false, m_History.CanRedo()))
			Redo();
		ImGui::EndMenu();
	}
}

void ParticleEditorPanel::DrawCustomQuickAccess()
{
	const bool hasResource = m_EffectHandle.IsValid() && (m_EffectHandle.Get() != nullptr);

	EditorUI::ToolbarLayout toolbar;
	// ── 파일 조작 ──
	if (toolbar.Icon("New", ICON_FA_FILE, "ParticleEditor.MenuNew"))
		NewEffect();

	if (toolbar.Icon("Open", ICON_FA_FOLDER_OPEN, "ParticleEditor.MenuOpen"))
		OpenEffect();

	EditorUI::DrawTimelineAction(toolbar, "ParticleTimeline", "Save", ICON_FA_FLOPPY_DISK, "EditorCommon.Save");
	EditorUI::DrawTimelineAction(toolbar, "ParticleTimeline", "Undo", ICON_FA_ROTATE_LEFT, "EditorCommon.Undo");
	EditorUI::DrawTimelineAction(toolbar, "ParticleTimeline", "Redo", ICON_FA_ROTATE_RIGHT, "EditorCommon.Redo");
	EditorUI::DrawTimelineTransport(toolbar, "ParticleTimeline", m_IsPlaying, m_IsLooping);
	ImGui::BeginDisabled(!hasResource);
	toolbar.SetNextItemWidth(ImGui::GetFontSize() * 6.0f);
	ImGui::SliderFloat("##PlayRate", &m_PlayRate, 0.1f, 3.0f, "%.1fx");
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("%s", LOCAL_CSTR("ParticleEditor.TooltipPlayRate"));

	ImGui::EndDisabled();

	if (toolbar.Icon("Focus", ICON_FA_CROSSHAIRS, "ParticleEditor.TooltipFocus"))
		m_PreviewWidget.FocusTarget(Engine::vec3(0.0f, 1.0f, 0.0f), 4.5f);

	// ── 통계 출력 ──
	uint32 totalActiveParticles = 0;
	auto* scene = m_PreviewWidget.GetScene();
	if (m_PreviewEntity && scene && hasResource)
	{
		auto& comp = scene->GetComponent<Engine::ParticleEffectComponent>(*m_PreviewEntity);
		for (const auto& inst : comp.instances)
			totalActiveParticles += static_cast<uint32>(inst.particles.size());
	}

	const std::string statsText = fmt::format(fmt::runtime(LOCAL("ParticleEditor.StatsFormat")), totalActiveParticles, m_SimulatedTime);
	toolbar.Text(statsText.c_str());
	toolbar.Text(LOCAL_FMT("Timeline.TimeCounter", m_TimelineSeek.value_or(m_SimulatedTime), GetTimelineDuration()).c_str());

	if (!m_StatusMessage.empty())
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
		toolbar.Text(m_StatusMessage.c_str(), false);
		ImGui::PopStyleColor();
	}
}

void ParticleEditorPanel::DrawEmitterList()
{
	auto* effect = m_EffectHandle.Get();
	if (!effect)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", LOCAL_CSTR("ResourceEditor.NoResource"));
		if (ImGui::Button(LOCAL_CSTR("MaterialEditor.Retry")))
		{
			SetTargetResource(m_TargetResourcePath);
		}
		return;
	}

	// ── 헤더: 타이틀 및 추가 (+) 아이콘 버튼 ──
	ImGui::AlignTextToFramePadding();
	ImGui::Text(ICON_FA_LAYER_GROUP " %s", LOCAL_CSTR("PROP_EMITTERS"));

	EditorUI::SameLineIfFits(EditorUI::GetIconButtonSize(ICON_FA_PLUS, true).x);
	if (EditorUI::SmallIconButton("AddEmitter", ICON_FA_PLUS, "EditorCommon.Add"))
	{
		auto beforeSnap = CaptureSnapshot();
		Engine::ParticleEmitterData newEmitter;
		newEmitter.name = "Emitter_" + std::to_string(effect->GetEmitterCount());
		effect->AddEmitter(newEmitter);
		m_SelectedEmitterIndex = effect->GetEmitterCount() - 1;

		auto afterSnap = CaptureSnapshot();
		auto cmd = std::make_unique<ParticleEffectCommand>(
			effect, std::move(beforeSnap), std::move(afterSnap), "Add Emitter",
			[this]() { SetupPreview(); m_IsDirty = true; });
		m_History.Record(std::move(cmd));

		m_IsDirty = true;
		SetupPreview();
	}
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", LOCAL_CSTR("ParticleEditor.TooltipAddEmitter"));

	ImGui::Separator();

	// ── 이미터 목록: 3열 테이블 (체크박스 | 이름 | 삭제) ──
	auto& emitters = effect->GetEmitters();
	int deleteIndex = -1;

	ImGuiTableFlags tableFlags = ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings;
	if (ImGui::BeginTable("##EmitterListTable", 3, tableFlags))
	{
		ImGui::TableSetupColumn("##Enabled", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFrameHeight());
		ImGui::TableSetupColumn("##Name", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("##Action", ImGuiTableColumnFlags_WidthFixed, EditorUI::GetIconButtonSize(ICON_FA_TRASH, true).x);

		for (int i = 0; i < static_cast<int>(emitters.size()); ++i)
		{
			auto& em = emitters[i];
			ImGui::PushID(i);
			ImGui::TableNextRow();

			// 컬럼 0: 활성화 체크박스
			ImGui::TableSetColumnIndex(0);
			bool en = em.enabled;
			if (ImGui::Checkbox("##Enabled", &en))
			{
				auto beforeSnap = CaptureSnapshot();
				em.enabled = en;
				auto afterSnap = CaptureSnapshot();
				auto cmd = std::make_unique<ParticleEffectCommand>(
					effect, std::move(beforeSnap), std::move(afterSnap), "Toggle Emitter",
					[this]() { SetupPreview(); m_IsDirty = true; });
				m_History.Record(std::move(cmd));
				m_IsDirty = true;
			}

			// 컬럼 1: 이미터 이름 및 선택 (텍스트가 길어도 컬럼 2로 넘치지 않음)
			ImGui::TableSetColumnIndex(1);
			const bool continuous = em.triggerMode == Engine::EParticleEmitterTrigger::Continuous;
			const char* icon = continuous ? ICON_FA_WAND_MAGIC_SPARKLES : ICON_FA_CIRCLE_NODES;
			std::string label = std::string(icon) + " " + (em.name.empty() ? LOCAL("EffectTimeline.Emitter") : em.name);
			if (em.triggerMode == Engine::EParticleEmitterTrigger::OnParentCollision)
				label += " (" + LOCAL("ParticleEditor.TriggerOnCollision") + ")";
			else if (em.triggerMode == Engine::EParticleEmitterTrigger::OnParentDeath)
				label += " (" + LOCAL("ParticleEditor.TriggerOnDeath") + ")";

			bool isSelected = (m_SelectedEmitterIndex == i);
			label += "###Emitter";
			ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(
				continuous ? IM_COL32(230, 130, 40, 220) : IM_COL32(130, 118, 235, 220)));
			if (ImGui::Selectable(label.c_str(), isSelected, ImGuiSelectableFlags_None))
			{
				m_SelectedEmitterIndex = i;
				m_PreviewWidget.FocusTarget(Engine::vec3(0.0f, 1.0f, 0.0f), 4.5f);
			}
			ImGui::PopStyleColor();

			// 컬럼 2: 삭제 아이콘 버튼 (투명 배경 + 호버 시 붉은색 강조, 우측 패딩 자동 확보)
			ImGui::TableSetColumnIndex(2);
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.2f, 0.2f, 0.4f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.9f, 0.1f, 0.1f, 0.7f));
			if (EditorUI::SmallIconButton("Delete", ICON_FA_TRASH, "EditorCommon.Delete"))
			{
				deleteIndex = i;
			}
			ImGui::PopStyleColor(3);
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", LOCAL_CSTR("EditorCommon.Delete"));

			ImGui::PopID();
		}
		ImGui::EndTable();
	}

	if (deleteIndex >= 0)
	{
		m_SelectedEmitterIndex = deleteIndex;
		DeleteSelectedEmitter();
	}
}

void ParticleEditorPanel::DeleteSelectedEmitter()
{
	auto* effect = m_EffectHandle.Get();
	if (!effect || !effect->GetEmitter(m_SelectedEmitterIndex)) return;
	const int index = m_SelectedEmitterIndex;
	auto before = CaptureSnapshot();
	effect->RemoveEmitter(index);
	for (auto& emitter : effect->GetEmitters())
	{
		if (emitter.parentEmitterIndex == index) emitter.parentEmitterIndex = -1;
		else if (emitter.parentEmitterIndex > index) --emitter.parentEmitterIndex;
	}
	m_SelectedEmitterIndex = std::min(index, effect->GetEmitterCount() - 1);
	m_History.Record(std::make_unique<ParticleEffectCommand>(effect, std::move(before), CaptureSnapshot(),
		LOCAL("Action.Timeline.DeleteSelected"), [this] { InvalidateTimelinePreview(); }));
	m_Timeline.Drag.reset();
	InvalidateTimelinePreview();
}

void ParticleEditorPanel::DrawTimeline()
{
	auto* effect = m_EffectHandle.Get();
	if (!effect)
	{
		EditorUI::EmptyState("VFXEditor.TimelineUnavailable");
		return;
	}
	const float duration = GetTimelineDuration();
	float minDuration = EditorUI::MinimumTimelineDuration;
	std::vector<EditorUI::TimelineRow> rows;
	const auto& emitters = effect->GetEmitters();
	rows.reserve(emitters.size());
	for (std::size_t i = 0; i < emitters.size(); ++i)
	{
		const auto& emitter = emitters[i];
		const float start = std::isfinite(emitter.emissionStartTime) ? std::max(0.0f, emitter.emissionStartTime) : 0.0f;
		const bool bounded = std::isfinite(emitter.emissionDuration) && emitter.emissionDuration > 0.0f;
		const float end = bounded ? start + emitter.emissionDuration : std::max(start, duration);
		minDuration = std::max(minDuration, bounded ? end : start + EditorUI::MinimumTimelineDuration);
		const bool continuous = emitter.triggerMode == Engine::EParticleEmitterTrigger::Continuous;
		rows.push_back({ i, emitter.name, continuous ? ICON_FA_WAND_MAGIC_SPARKLES : ICON_FA_CIRCLE_NODES,
			continuous ? IM_COL32(230, 130, 40, 220) : IM_COL32(130, 118, 235, 220), start, end,
			m_SelectedEmitterIndex == static_cast<int>(i),
			bounded ? EditorUI::TimelineRowKind::Interval : EditorUI::TimelineRowKind::OpenEnd });
	}
	const auto result = EditorUI::DrawEffectTimeline(m_Timeline, rows, duration,
		m_TimelineSeek.value_or(m_SimulatedTime), minDuration);
	if (result.Selection)
	{
		m_SelectedEmitterIndex = static_cast<int>(*result.Selection);
		m_PreviewWidget.FocusTarget(Engine::vec3(0.0f, 1.0f, 0.0f), 4.5f);
	}
	if (result.Seek) ScrubTime(*result.Seek);
	if (!result.Commit) return;

	const auto edit = *result.Commit;
	const auto handle = m_EffectHandle;
	const float previousTotal = effect->GetPlaybackDuration();
	if (edit.Mode == EditorUI::TimelineDragMode::Duration)
	{
		const auto apply = [this, handle](float value)
		{
			auto* target = handle.Get();
			if (!target) return false;
			target->SetPlaybackDuration(value);
			InvalidateTimelinePreview();
			return true;
		};
		m_History.Execute(std::make_unique<CallbackEditorCommand>(LOCAL("Timeline.Duration"),
			[apply, edit] { return apply(edit.End); }, [apply, previousTotal] { return apply(previousTotal); }));
		return;
	}

	const int index = static_cast<int>(edit.Row);
	const auto* emitter = effect->GetEmitter(index);
	if (!emitter) return;
	const float oldStart = emitter->emissionStartTime;
	const float oldLength = emitter->emissionDuration;
	const bool resize = edit.Mode == EditorUI::TimelineDragMode::End ||
		(edit.Mode == EditorUI::TimelineDragMode::Start && oldLength > 0.0f);
	const float length = resize ? edit.End - edit.Start : oldLength;
	const float end = edit.Start + std::max(length, EditorUI::MinimumTimelineDuration);
	const float total = previousTotal > 0.0f ? std::max(previousTotal, end) : 0.0f;
	const auto apply = [this, handle, index](float start, float duration, float playbackDuration)
	{
		auto* target = handle.Get();
		auto* emitter = target ? target->GetEmitter(index) : nullptr;
		if (!emitter) return false;
		emitter->emissionStartTime = start;
		emitter->emissionDuration = duration;
		target->SetPlaybackDuration(playbackDuration);
		InvalidateTimelinePreview();
		return true;
	};
	m_History.Execute(std::make_unique<CallbackEditorCommand>(LOCAL("EffectTimeline.EditRange"),
		[apply, edit, length, total] { return apply(edit.Start, length, total); },
		[apply, oldStart, oldLength, previousTotal] { return apply(oldStart, oldLength, previousTotal); }));
}

bool ParticleEditorPanel::CanExecuteTimelineAction(EditorUI::TimelineAction action) const
{
	using enum EditorUI::TimelineAction;
	const auto* effect = m_EffectHandle.Get();
	if (!m_Open || !effect || m_Timeline.Drag) return false;
	if (action == Undo) return m_History.CanUndo();
	if (action == Redo) return m_History.CanRedo();
	if (action == Delete) return effect->GetEmitter(m_SelectedEmitterIndex) != nullptr;
	return true;
}

void ParticleEditorPanel::ExecuteTimelineAction(EditorUI::TimelineAction action)
{
	using enum EditorUI::TimelineAction;
	if (!CanExecuteTimelineAction(action)) return;
	const float time = m_TimelineSeek.value_or(m_SimulatedTime);
	switch (action)
	{
	case PlayPause:
		if (!m_IsPlaying && time >= GetTimelineDuration()) RestartSimulation();
		m_IsPlaying = !m_IsPlaying;
		break;
	case Previous: ScrubTime(time - 1.0f / EditorUI::EffectTimelineFPS); break;
	case Next: ScrubTime(time + 1.0f / EditorUI::EffectTimelineFPS); break;
	case First: ScrubTime(0.0f); break;
	case Last: ScrubTime(GetTimelineDuration()); break;
	case Loop: m_IsLooping = !m_IsLooping; break;
	case Fit: m_Timeline.Fit(); break;
	case Delete: DeleteSelectedEmitter(); break;
	case Undo: this->Undo(); break;
	case Redo: this->Redo(); break;
	case Save: SaveEffect(); break;
	}
}

void ParticleEditorPanel::DrawViewport()
{
	const ImVec2 viewSize = ImGui::GetContentRegionAvail();
	m_PreviewWidget.Draw(viewSize);
}

void ParticleEditorPanel::DrawEmitterInspector()
{
	auto* effect = m_EffectHandle.Get();
	if (!effect)
	{
		ImGui::TextDisabled("%s", LOCAL_CSTR("ParticleEditor.NoEmitterSelected"));
		return;
	}

	auto* emitter = effect->GetEmitter(m_SelectedEmitterIndex);
	if (!emitter)
	{
		ImGui::TextDisabled("%s", LOCAL_CSTR("ParticleEditor.NoEmitterSelected"));
		return;
	}

	ImGui::PushID(m_SelectedEmitterIndex);

	ImGui::Text(ICON_FA_SLIDERS " %s [%s]", LOCAL_CSTR("CATEGORY_RENDER"), emitter->name.c_str());
	ImGui::Separator();

	auto DrawRow = [this, effect](const char* labelKey, auto drawWidget)
	{
		EditorUI::PropertyRow(labelKey);
		ImGui::SetNextItemWidth(-FLT_MIN);

		drawWidget();

		if (ImGui::IsItemActivated())
		{
			m_PreEditSnapshot = CaptureSnapshot();
			m_HasPreEditSnapshot = true;
		}
		if (ImGui::IsItemDeactivatedAfterEdit() && m_HasPreEditSnapshot)
		{
			auto postSnapshot = CaptureSnapshot();
			auto cmd = std::make_unique<ParticleEffectCommand>(
				effect, std::move(m_PreEditSnapshot), std::move(postSnapshot),
				"Edit " + std::string(labelKey),
				[this]() { SetupPreview(); m_IsDirty = true; });
			m_History.Record(std::move(cmd));
			m_HasPreEditSnapshot = false;
			m_IsDirty = true;
		}
	};

	constexpr ImGuiTableFlags tableFlags = ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg;

	// 1. 식별 및 트리거
	if (ImGui::CollapsingHeader(LOCAL_CSTR("ParticleEditor.CategoryIdentification"), ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (ImGui::BeginTable("##TableIdentification", 2, tableFlags))
		{
			ImGui::TableSetupColumn("##Label", ImGuiTableColumnFlags_WidthFixed, 140.0f);
			ImGui::TableSetupColumn("##Widget", ImGuiTableColumnFlags_WidthStretch);

			DrawRow("ParticleEditor.PropName", [&]() {
				char nameBuf[64];
				strncpy_s(nameBuf, emitter->name.c_str(), sizeof(nameBuf));
				if (ImGui::InputText("##Name", nameBuf, sizeof(nameBuf)))
				{
					emitter->name = nameBuf;
					m_IsDirty = true;
				}
			});

			DrawRow("ParticleEditor.PropTriggerMode", [&]() {
				int trig = static_cast<int>(emitter->triggerMode);
				const std::string triggerModes[] = {
					LOCAL("ParticleEditor.TriggerContinuous"),
					LOCAL("ParticleEditor.TriggerOnCollision"),
					LOCAL("ParticleEditor.TriggerOnDeath")
				};
				const char* triggerCstrs[] = { triggerModes[0].c_str(), triggerModes[1].c_str(), triggerModes[2].c_str() };
				if (ImGui::Combo("##TriggerMode", &trig, triggerCstrs, IM_ARRAYSIZE(triggerCstrs)))
				{
					auto beforeSnap = CaptureSnapshot();
					emitter->triggerMode = static_cast<Engine::EParticleEmitterTrigger>(trig);
					auto afterSnap = CaptureSnapshot();
					auto cmd = std::make_unique<ParticleEffectCommand>(
						effect, std::move(beforeSnap), std::move(afterSnap), "Change Trigger Mode",
						[this]() { SetupPreview(); m_IsDirty = true; });
					m_History.Record(std::move(cmd));
					m_IsDirty = true;
				}
			});

			if (emitter->triggerMode != Engine::EParticleEmitterTrigger::Continuous)
			{
				DrawRow("ParticleEditor.PropParentEmitter", [&]() {
					int pIdx = emitter->parentEmitterIndex;
					std::vector<std::string> parentNames;
					for (int i = 0; i < effect->GetEmitterCount(); ++i)
					{
						if (i == m_SelectedEmitterIndex)
							parentNames.push_back(std::to_string(i) + ": (Self - Invalid)");
						else
							parentNames.push_back(std::to_string(i) + ": " + effect->GetEmitter(i)->name);
					}

					std::vector<const char*> parentCstrs;
					for (const auto& s : parentNames) parentCstrs.push_back(s.c_str());

					if (ImGui::Combo("##ParentEmitter", &pIdx, parentCstrs.data(), static_cast<int>(parentCstrs.size())))
					{
						auto beforeSnap = CaptureSnapshot();
						emitter->parentEmitterIndex = pIdx;
						auto afterSnap = CaptureSnapshot();
						auto cmd = std::make_unique<ParticleEffectCommand>(
							effect, std::move(beforeSnap), std::move(afterSnap), "Change Parent Emitter",
							[this]() { SetupPreview(); m_IsDirty = true; });
						m_History.Record(std::move(cmd));
						m_IsDirty = true;
					}
				});

				DrawRow("ParticleEditor.PropTriggerBurstCount", [&]() {
					int burst = static_cast<int>(emitter->triggerBurstCount);
					if (ImGui::SliderInt("##TriggerBurstCount", &burst, 1, 100))
					{
						emitter->triggerBurstCount = static_cast<uint32>(burst);
						m_IsDirty = true;
					}
				});

				DrawRow("ParticleEditor.PropAlignToSurfaceNormal", [&]() {
					if (ImGui::Checkbox("##AlignNormal", &emitter->alignToSurfaceNormal))
					{
						auto beforeSnap = CaptureSnapshot();
						auto afterSnap = CaptureSnapshot();
						auto cmd = std::make_unique<ParticleEffectCommand>(
							effect, std::move(beforeSnap), std::move(afterSnap), "Toggle Align Normal",
							[this]() { SetupPreview(); m_IsDirty = true; });
						m_History.Record(std::move(cmd));
						m_IsDirty = true;
					}
				});

				DrawRow("ParticleEditor.PropInheritParentVelocity", [&]() {
					if (ImGui::Checkbox("##InheritVelocity", &emitter->inheritParentVelocity))
					{
						auto beforeSnap = CaptureSnapshot();
						auto afterSnap = CaptureSnapshot();
						auto cmd = std::make_unique<ParticleEffectCommand>(
							effect, std::move(beforeSnap), std::move(afterSnap), "Toggle Inherit Velocity",
							[this]() { SetupPreview(); m_IsDirty = true; });
						m_History.Record(std::move(cmd));
						m_IsDirty = true;
					}
				});

				if (emitter->inheritParentVelocity)
				{
					DrawRow("ParticleEditor.PropInheritRatio", [&]() {
						if (ImGui::SliderFloat("##InheritRatio", &emitter->inheritVelocityRatio, 0.0f, 1.0f, "%.2f"))
							m_IsDirty = true;
					});
				}
			}
			else
			{
				DrawRow("ParticleEditor.PropEmissionRate", [&]() {
					if (ImGui::DragFloat("##EmissionRate", &emitter->emissionRate, 1.0f, 0.0f, 2000.0f, "%.1f /s"))
						m_IsDirty = true;
				});

				DrawRow("ParticleEditor.PropBurstCount", [&]() {
					int bCount = static_cast<int>(emitter->burstCount);
					if (ImGui::DragInt("##BurstCount", &bCount, 1, 0, 500))
					{
						emitter->burstCount = static_cast<uint32>(bCount);
						m_IsDirty = true;
					}
				});

				if (emitter->burstCount > 0)
				{
					DrawRow("ParticleEditor.PropBurstInterval", [&]() {
						if (ImGui::DragFloat("##BurstInterval", &emitter->burstInterval, 0.05f, 0.1f, 10.0f, "%.2fs"))
							m_IsDirty = true;
					});
				}
			}

			DrawRow("ParticleEditor.PropMaxParticles", [&]() {
				int maxP = static_cast<int>(emitter->maxParticles);
				if (ImGui::DragInt("##MaxParticles", &maxP, 10, 10, 5000))
				{
					emitter->maxParticles = static_cast<uint32>(maxP);
					m_IsDirty = true;
				}
			});

			ImGui::EndTable();
		}
	}

	// 2. 수명 및 속도
	if (ImGui::CollapsingHeader(LOCAL_CSTR("ParticleEditor.CategoryLifetime"), ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (ImGui::BeginTable("##TableLifetime", 2, tableFlags))
		{
			ImGui::TableSetupColumn("##Label", ImGuiTableColumnFlags_WidthFixed, 140.0f);
			ImGui::TableSetupColumn("##Widget", ImGuiTableColumnFlags_WidthStretch);

			DrawRow("ParticleEditor.PropLifetimeRange", [&]() {
				if (ImGui::DragFloatRange2("##Lifetime", &emitter->lifetimeMin, &emitter->lifetimeMax, 0.05f, 0.01f, 30.0f, "%.2fs"))
					m_IsDirty = true;
			});

			DrawRow("ParticleEditor.PropSpeedRange", [&]() {
				if (ImGui::DragFloatRange2("##Speed", &emitter->speedMin, &emitter->speedMax, 0.1f, 0.0f, 100.0f, "%.1f m/s"))
					m_IsDirty = true;
			});

			DrawRow("ParticleEditor.PropDirection", [&]() {
				if (ImGui::DragFloat3("##Direction", &emitter->direction.x, 0.05f, -1.0f, 1.0f, "%.2f"))
					m_IsDirty = true;
			});

			DrawRow("ParticleEditor.PropSpreadAngle", [&]() {
				if (ImGui::SliderFloat("##SpreadAngle", &emitter->spreadAngleDegrees, 0.0f, 180.0f, "%.1f deg"))
					m_IsDirty = true;
			});

			ImGui::EndTable();
		}
	}

	// 3. 환경 외력 및 난류
	if (ImGui::CollapsingHeader(LOCAL_CSTR("ParticleEditor.CategoryForces")))
	{
		if (ImGui::BeginTable("##TableForces", 2, tableFlags))
		{
			ImGui::TableSetupColumn("##Label", ImGuiTableColumnFlags_WidthFixed, 140.0f);
			ImGui::TableSetupColumn("##Widget", ImGuiTableColumnFlags_WidthStretch);

			DrawRow("ParticleEditor.PropGravity", [&]() {
				if (ImGui::DragFloat3("##Gravity", &emitter->gravity.x, 0.1f, -50.0f, 50.0f, "%.1f"))
					m_IsDirty = true;
			});

			DrawRow("ParticleEditor.PropBuoyancy", [&]() {
				if (ImGui::DragFloat("##Buoyancy", &emitter->buoyancy, 0.1f, -50.0f, 50.0f, "%.1f"))
					m_IsDirty = true;
			});

			DrawRow("ParticleEditor.PropDrag", [&]() {
				if (ImGui::SliderFloat("##Drag", &emitter->drag, 0.0f, 5.0f, "%.2f"))
					m_IsDirty = true;
			});

			DrawRow("ParticleEditor.PropWind", [&]() {
				if (ImGui::DragFloat3("##Wind", &emitter->wind.x, 0.1f, -50.0f, 50.0f, "%.1f"))
					m_IsDirty = true;
			});

			DrawRow("ParticleEditor.PropEnableCurlNoise", [&]() {
				if (ImGui::Checkbox("##EnableCurlNoise", &emitter->enableCurlNoise))
				{
					auto beforeSnap = CaptureSnapshot();
					auto afterSnap = CaptureSnapshot();
					auto cmd = std::make_unique<ParticleEffectCommand>(
						effect, std::move(beforeSnap), std::move(afterSnap), "Toggle Curl Noise",
						[this]() { SetupPreview(); m_IsDirty = true; });
					m_History.Record(std::move(cmd));
					m_IsDirty = true;
				}
			});

			if (emitter->enableCurlNoise)
			{
				DrawRow("ParticleEditor.PropNoiseStrength", [&]() {
					if (ImGui::SliderFloat("##NoiseStrength", &emitter->curlNoiseStrength, 0.0f, 10.0f, "%.2f"))
						m_IsDirty = true;
				});

				DrawRow("ParticleEditor.PropNoiseFrequency", [&]() {
					if (ImGui::SliderFloat("##NoiseFrequency", &emitter->curlNoiseFrequency, 0.01f, 5.0f, "%.2f"))
						m_IsDirty = true;
				});

				DrawRow("ParticleEditor.PropNoiseSpeed", [&]() {
					if (ImGui::SliderFloat("##NoiseSpeed", &emitter->curlNoiseScrollSpeed, 0.0f, 5.0f, "%.2f"))
						m_IsDirty = true;
				});
			}

			ImGui::EndTable();
		}
	}

	// 4. 물리 충돌 (Collision)
	if (ImGui::CollapsingHeader(LOCAL_CSTR("ParticleEditor.CategoryCollision")))
	{
		if (ImGui::BeginTable("##TableCollision", 2, tableFlags))
		{
			ImGui::TableSetupColumn("##Label", ImGuiTableColumnFlags_WidthFixed, 140.0f);
			ImGui::TableSetupColumn("##Widget", ImGuiTableColumnFlags_WidthStretch);

			DrawRow("ParticleEditor.PropCollisionMode", [&]() {
				int cMode = static_cast<int>(emitter->collisionMode);
				const std::string colModes[] = {
					LOCAL("ParticleEditor.CollisionNone"),
					LOCAL("ParticleEditor.CollisionFloorPlane"),
					LOCAL("ParticleEditor.CollisionPhysicsRaycast")
				};
				const char* colModeCstrs[] = { colModes[0].c_str(), colModes[1].c_str(), colModes[2].c_str() };
				if (ImGui::Combo("##CollisionMode", &cMode, colModeCstrs, IM_ARRAYSIZE(colModeCstrs)))
				{
					auto beforeSnap = CaptureSnapshot();
					emitter->collisionMode = static_cast<Engine::EParticleCollisionMode>(cMode);
					auto afterSnap = CaptureSnapshot();
					auto cmd = std::make_unique<ParticleEffectCommand>(
						effect, std::move(beforeSnap), std::move(afterSnap), "Change Collision Mode",
						[this]() { SetupPreview(); m_IsDirty = true; });
					m_History.Record(std::move(cmd));
					m_IsDirty = true;
				}
			});

			if (emitter->collisionMode != Engine::EParticleCollisionMode::None)
			{
				DrawRow("ParticleEditor.PropCollisionResponse", [&]() {
					int cResp = static_cast<int>(emitter->collisionResponse);
					const std::string colResponses[] = {
						LOCAL("ParticleEditor.ResponseBounce"),
						LOCAL("ParticleEditor.ResponseSlide"),
						LOCAL("ParticleEditor.ResponseDie"),
						LOCAL("ParticleEditor.ResponseStop")
					};
					const char* colRespCstrs[] = { colResponses[0].c_str(), colResponses[1].c_str(), colResponses[2].c_str(), colResponses[3].c_str() };
					if (ImGui::Combo("##CollisionResponse", &cResp, colRespCstrs, IM_ARRAYSIZE(colRespCstrs)))
					{
						auto beforeSnap = CaptureSnapshot();
						emitter->collisionResponse = static_cast<Engine::EParticleCollisionResponse>(cResp);
						auto afterSnap = CaptureSnapshot();
						auto cmd = std::make_unique<ParticleEffectCommand>(
							effect, std::move(beforeSnap), std::move(afterSnap), "Change Collision Response",
							[this]() { SetupPreview(); m_IsDirty = true; });
						m_History.Record(std::move(cmd));
						m_IsDirty = true;
					}
				});

				if (emitter->collisionResponse == Engine::EParticleCollisionResponse::Bounce)
				{
					DrawRow("ParticleEditor.PropRestitution", [&]() {
						if (ImGui::SliderFloat("##Restitution", &emitter->restitution, 0.0f, 1.0f, "%.2f"))
							m_IsDirty = true;
					});

					DrawRow("ParticleEditor.PropMinBounceSpeed", [&]() {
						if (ImGui::SliderFloat("##MinBounceSpeed", &emitter->minBounceSpeed, 0.0f, 2.0f, "%.2f"))
							m_IsDirty = true;
					});
				}

				DrawRow("ParticleEditor.PropFriction", [&]() {
					if (ImGui::SliderFloat("##Friction", &emitter->friction, 0.0f, 1.0f, "%.2f"))
						m_IsDirty = true;
				});

				if (emitter->collisionMode == Engine::EParticleCollisionMode::FloorPlane)
				{
					DrawRow("ParticleEditor.PropFloorHeight", [&]() {
						if (ImGui::DragFloat("##FloorHeight", &emitter->floorHeight, 0.05f, -100.0f, 100.0f, "%.2f m"))
							m_IsDirty = true;
					});
				}

				DrawRow("ParticleEditor.PropCollisionRadius", [&]() {
					if (ImGui::DragFloat("##CollisionRadius", &emitter->collisionRadius, 0.01f, 0.001f, 2.0f, "%.3f m"))
						m_IsDirty = true;
				});
			}

			ImGui::EndTable();
		}
	}

	// 5. 크기 및 회전
	if (ImGui::CollapsingHeader(LOCAL_CSTR("ParticleEditor.CategorySizeRotation")))
	{
		if (ImGui::BeginTable("##TableSizeRotation", 2, tableFlags))
		{
			ImGui::TableSetupColumn("##Label", ImGuiTableColumnFlags_WidthFixed, 140.0f);
			ImGui::TableSetupColumn("##Widget", ImGuiTableColumnFlags_WidthStretch);

			DrawRow("ParticleEditor.PropSizeStart", [&]() {
				if (ImGui::DragFloat2("##SizeStart", &emitter->sizeStart.x, 0.02f, 0.01f, 20.0f, "%.2f"))
					m_IsDirty = true;
			});

			DrawRow("ParticleEditor.PropSizeEnd", [&]() {
				if (ImGui::DragFloat2("##SizeEnd", &emitter->sizeEnd.x, 0.02f, 0.01f, 20.0f, "%.2f"))
					m_IsDirty = true;
			});

			DrawRow("ParticleEditor.PropRotationSpeedRange", [&]() {
				if (ImGui::DragFloatRange2("##RotationSpeed", &emitter->rotationSpeedMin, &emitter->rotationSpeedMax, 0.1f, -20.0f, 20.0f, "%.1f rad/s"))
					m_IsDirty = true;
			});

			ImGui::EndTable();
		}
	}

	// 6. 색상 및 머티리얼
	if (ImGui::CollapsingHeader(LOCAL_CSTR("ParticleEditor.CategoryColorMaterial"), ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (ImGui::BeginTable("##TableColorMaterial", 2, tableFlags))
		{
			ImGui::TableSetupColumn("##Label", ImGuiTableColumnFlags_WidthFixed, 140.0f);
			ImGui::TableSetupColumn("##Widget", ImGuiTableColumnFlags_WidthStretch);

			DrawRow("ParticleEditor.PropColorStart", [&]() {
				if (ImGui::ColorEdit4("##ColorStart", &emitter->colorStart.x, ImGuiColorEditFlags_Float))
					m_IsDirty = true;
			});

			DrawRow("ParticleEditor.PropColorEnd", [&]() {
				if (ImGui::ColorEdit4("##ColorEnd", &emitter->colorEnd.x, ImGuiColorEditFlags_Float))
					m_IsDirty = true;
			});

			DrawRow("ParticleEditor.PropTexture", [&]() {
				ImGui::PushID("TexturePicker");
				auto beforeSnap = CaptureSnapshot();
				if (Editor::DrawInspectorResourcePicker(emitter->textureHandle, true, true))
				{
					auto afterSnap = CaptureSnapshot();
					auto cmd = std::make_unique<ParticleEffectCommand>(
						effect, std::move(beforeSnap), std::move(afterSnap), "Change Texture",
						[this]() { SetupPreview(); m_IsDirty = true; });
					m_History.Record(std::move(cmd));
					m_IsDirty = true;
				}
				ImGui::PopID();
			});

			DrawRow("ParticleEditor.PropMaterial", [&]() {
				ImGui::PushID("MaterialPicker");
				auto beforeSnap = CaptureSnapshot();
				if (Editor::DrawInspectorResourcePicker(emitter->materialHandle, true, true))
				{
					auto afterSnap = CaptureSnapshot();
					auto cmd = std::make_unique<ParticleEffectCommand>(
						effect, std::move(beforeSnap), std::move(afterSnap), "Change Material",
						[this]() { SetupPreview(); m_IsDirty = true; });
					m_History.Record(std::move(cmd));
					m_IsDirty = true;
				}
				ImGui::PopID();
			});

			DrawRow("ParticleEditor.PropAlignToSurfaceDisk", [&]() {
				if (ImGui::Checkbox("##AlignToSurfaceDisk", &emitter->alignToSurfaceDisk))
				{
					auto beforeSnap = CaptureSnapshot();
					auto afterSnap = CaptureSnapshot();
					auto cmd = std::make_unique<ParticleEffectCommand>(
						effect, std::move(beforeSnap), std::move(afterSnap), "Toggle Surface Disk Align",
						[this]() { SetupPreview(); m_IsDirty = true; });
					m_History.Record(std::move(cmd));
					m_IsDirty = true;
				}
			});

			ImGui::EndTable();
		}
	}

	ImGui::PopID();
}