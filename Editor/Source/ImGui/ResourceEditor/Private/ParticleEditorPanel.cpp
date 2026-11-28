#include "ParticleEditorPanel.h"
#include <utility>
#include "EffectDocumentFile.h"
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
#include "ReflectionPropertyDrawer.h"
#include "ReflectionRegistry.h"
#include "ReflectionEditState.h"
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
		const auto name = Engine::ToLowerStr(Engine::Unicode::WideToUtf8(path.filename().wstring()));
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
	ReleaseResourceDocument();
	m_PreviewWidget.Free();
}

bool ParticleEditorPanel::IsSupported(const std::filesystem::path& assetPath) const
{
	return IsParticleEffectFile(assetPath);
}

void ParticleEditorPanel::SetTargetResource(const std::filesystem::path& assetPath)
{
	const auto path = Engine::PlatformPath::ToProjectRelative(assetPath).wstring();
	auto& manager = Engine::ResourceManager::Get();
	auto candidate = manager.GetResourceHandle<Engine::ParticleEffect>(path);
	if (!candidate.IsValid())
		candidate = Engine::ResourceHandle<Engine::ParticleEffect>(manager.LoadFile(path));
	if (!candidate.Get() || !RestoreSavedChanges()) return;

	ReleaseResourceDocument();
	m_TargetResourcePath = assetPath;
	m_EffectHandle = std::move(candidate);
	m_SelectedEmitterIndex = m_EffectHandle.Get()->GetEmitterCount() > 0 ? 0 : -1;
	SetupPreview();
}

static void ApplySavedParticleEffect(Engine::ParticleEffect& effect, Engine::ParticleEffect& saved)
{
	effect.GetEmitters().swap(saved.GetEmitters());
	effect.SetPlaybackDuration(saved.GetPlaybackDuration());
	effect.IncreaseVersion();
}

bool ParticleEditorPanel::RestoreSavedResource()
{
	if (m_TargetResourcePath.empty()) return true;
	auto* effect = m_EffectHandle.Get();
	auto saved = MakeEffectDocument<Engine::ParticleEffect>();
	if (!effect || !saved || !LoadEffectDocument(*saved, m_TargetResourcePath)) return false;

	ApplySavedParticleEffect(*effect, *saved);
	return true;
}

void ParticleEditorPanel::ReleaseResourceDocument()
{
	m_History.Clear();
	m_SavedStateId = m_History.GetCurrentStateId();
	m_PreEditSnapshot.clear();
	m_HasPreEditSnapshot = false;
	m_SelectedEmitterIndex = -1;
	const auto handle = m_EffectHandle.GetRawHandle();
	m_EffectHandle.Reset();
	SetupPreview();
	if (std::exchange(m_OwnsTemporaryEffect, false) && Engine::ResourceManager::IsCreated())
	{
		Engine::ResourceManager::Get().ReleaseManagerReference(handle);
	}
	RestartSimulation();
	m_Timeline.Drag.reset();
	m_Timeline.Fit();
	m_TimelineSeek.reset();
	m_StatusMessage.clear();
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
		[this]() { SetupPreview(); });
	m_History.Record(std::move(cmd));
	m_HasPreEditSnapshot = false;
}

void ParticleEditorPanel::Undo()
{
	// 아직 확정하지 않은 인스펙터 편집을 먼저 이력에 넣어야 그 편집부터 되돌린다.
	if (m_HasPreEditSnapshot) PushSnapshot(LOCAL("History.Command.EditProperty"));
	if (!m_History.Undo()) return;
	if (const auto* effect = m_EffectHandle.Get())
		m_SelectedEmitterIndex = effect->GetEmitterCount() > 0 ?
			std::clamp(m_SelectedEmitterIndex, 0, effect->GetEmitterCount() - 1) : -1;
}

void ParticleEditorPanel::Redo()
{
	if (m_HasPreEditSnapshot) PushSnapshot(LOCAL("History.Command.EditProperty"));
	if (!m_History.Redo()) return;
	if (const auto* effect = m_EffectHandle.Get())
		m_SelectedEmitterIndex = effect->GetEmitterCount() > 0 ?
			std::clamp(m_SelectedEmitterIndex, 0, effect->GetEmitterCount() - 1) : -1;
}

void ParticleEditorPanel::NewEffect()
{
	auto newEffect = Engine::ParticleEffect::Create();
	if (!newEffect) return;
	if (!RestoreSavedChanges())
	{
		Engine::ResourceManager::Get().DestroyResource(newEffect);
		return;
	}

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

	ReleaseResourceDocument();
	m_TargetResourcePath.clear();
	static uint64 nextDraft = 0;
	const auto key = L"EditorParticleDraft_" + std::to_wstring(++nextDraft);
	m_EffectHandle = Engine::ResourceManager::Get().AddResource<Engine::ParticleEffect>(key, newEffect);
	m_OwnsTemporaryEffect = true;
	m_SelectedEmitterIndex = 0;
	SetupPreview();
}

void ParticleEditorPanel::OpenEffect()
{
	std::wstring openPath;
	if (FileDialogs::OpenFileDialog(openPath, { { Engine::Unicode::Utf8ToWide(LOCAL("ParticleEditor.FileDialogFilter")), L"*.bameffect.json;*.bameffect" } }, L"", EFileDialogContext::ParticleOpen))
	{
		SetTargetResource(std::filesystem::path(openPath));
	}
}

void ParticleEditorPanel::SaveEffectAs()
{
	std::wstring savePath;
	if (FileDialogs::SaveFileDialog(savePath, { { Engine::Unicode::Utf8ToWide(LOCAL("ParticleEditor.FileDialogFilter")), L"*.bameffect.json" } }, L"NewEffect.bameffect.json", L"bameffect.json", EFileDialogContext::ParticleSave))
	{
		SaveEffectTo(std::filesystem::path(savePath));
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
}

void ParticleEditorPanel::SaveEffect()
{
	if (m_TargetResourcePath.empty()) SaveEffectAs();
	else SaveEffectTo(m_TargetResourcePath);
}

void ParticleEditorPanel::SaveEffectTo(const std::filesystem::path& path)
{
	auto* effect = m_EffectHandle.Get();
	if (!effect) return;
	if (m_HasPreEditSnapshot) PushSnapshot(LOCAL("History.Command.EditProperty"));

	const bool copyToNewDocument = !m_OwnsTemporaryEffect && !m_TargetResourcePath.empty() &&
		!IsSameEffectDocument(m_TargetResourcePath, path);
	if (copyToNewDocument)
	{
		auto copy = SaveEffectDocumentCopy(*effect, m_TargetResourcePath, path, ApplySavedParticleEffect);
		if (!copy.Get())
		{
			m_StatusMessage = LOCAL("ResourceEditor.SaveFailed");
			return;
		}
		ReleaseResourceDocument();
		m_EffectHandle = std::move(copy);
		m_SelectedEmitterIndex = m_EffectHandle.Get()->GetEmitterCount() > 0 ? 0 : -1;
		SetupPreview();
	}
	else if (!effect->SaveToFile(Engine::PlatformPath::ResolveProjectRelative(path).wstring()))
	{
		m_StatusMessage = LOCAL("ResourceEditor.SaveFailed");
		return;
	}
	m_TargetResourcePath = path;
	m_SavedStateId = m_History.GetCurrentStateId();
	m_StatusMessage = LOCAL("ResourceEditor.SaveSuccess");
}

void ParticleEditorPanel::DrawCustomContent()
{
	EditorUI::DrawEffectEditorLayout(m_LeftPaneWidth, m_RightPaneWidth, m_TimelineHeight,
		[this] { DrawEmitterList(); }, [this] { DrawViewport(); },
		[this] { DrawEmitterInspector(); }, [this] { DrawTimeline(); },
		[this] { DrawTransportBar(); });
}

void ParticleEditorPanel::DrawCustomToolbar()
{
	const bool hasResource = m_EffectHandle.Get() != nullptr;

	if (ImGui::BeginMenu(LOCAL_CSTR("EditorMenu.File")))
	{
		if (ImGui::MenuItem(LOCAL_CSTR("ParticleEditor.MenuNew")))
			RunWithDiscardConfirmation([this] { NewEffect(); });
		if (ImGui::MenuItem(LOCAL_CSTR("ParticleEditor.MenuOpen")))
			RunWithDiscardConfirmation([this] { OpenEffect(); });
		ImGui::Separator();
		DrawSaveMenuItem();
		if (ImGui::MenuItem(LOCAL_CSTR("ParticleEditor.MenuSaveAs"), nullptr, false, hasResource))
			SaveEffectAs();
		DrawCloseMenuItem();
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu(LOCAL_CSTR("EditorMenu.Edit")))
	{
		DrawHistoryMenuItems("ParticleTimeline");
		ImGui::EndMenu();
	}
}

void ParticleEditorPanel::DrawCustomQuickAccess()
{
	EditorUI::ToolbarLayout toolbar;
	if (toolbar.Icon("New", ICON_FA_FILE, "ParticleEditor.MenuNew"))
		RunWithDiscardConfirmation([this] { NewEffect(); });

	if (toolbar.Icon("Open", ICON_FA_FOLDER_OPEN, "ParticleEditor.MenuOpen"))
		RunWithDiscardConfirmation([this] { OpenEffect(); });

	DrawSaveButton(toolbar);
	DrawHistoryButtons(toolbar, "ParticleTimeline");

	if (toolbar.Icon("Focus", ICON_FA_CROSSHAIRS, "ParticleEditor.TooltipFocus"))
		m_PreviewWidget.FocusTarget(Engine::vec3(0.0f, 1.0f, 0.0f), 4.5f);

	if (!m_StatusMessage.empty())
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
		toolbar.Text(m_StatusMessage.c_str(), false);
		ImGui::PopStyleColor();
	}
}

void ParticleEditorPanel::DrawTransportBar()
{
	const auto* effect = m_EffectHandle.Get();
	EditorUI::ToolbarLayout toolbar;

	EditorUI::DrawTimelineAction(toolbar, "ParticleTimeline", "FirstFrame", ICON_FA_BACKWARD_FAST, "Action.Timeline.FirstFrame");
	EditorUI::DrawTimelineAction(toolbar, "ParticleTimeline", "PrevFrame", ICON_FA_BACKWARD_STEP, "Action.Timeline.PrevFrame");
	EditorUI::DrawTimelineAction(toolbar, "ParticleTimeline", "PlayPause", m_IsPlaying ? ICON_FA_PAUSE : ICON_FA_PLAY, "Action.Timeline.PlayPause");
	EditorUI::DrawTimelineAction(toolbar, "ParticleTimeline", "NextFrame", ICON_FA_FORWARD_STEP, "Action.Timeline.NextFrame");
	EditorUI::DrawTimelineAction(toolbar, "ParticleTimeline", "LastFrame", ICON_FA_FORWARD_FAST, "Action.Timeline.LastFrame");
	EditorUI::DrawTimelineAction(toolbar, "ParticleTimeline", "ToggleLoop", ICON_FA_REPEAT, "Action.Timeline.ToggleLoop", m_IsLooping);

	if (effect)
	{
		const float currentTime = m_TimelineSeek.value_or(m_SimulatedTime);
		const float duration = GetTimelineDuration();
		const int currentFrame = static_cast<int>(currentTime * EditorUI::EffectTimelineFPS);
		const int totalFrames = static_cast<int>(duration * EditorUI::EffectTimelineFPS);

		const std::string timeLabel = LOCAL_FMT("Timeline.TimeCounter", currentTime, duration);
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.9f, 0.5f, 1.0f));
		toolbar.Text(timeLabel.c_str(), false);
		ImGui::PopStyleColor();

		const std::string frameLabel = LOCAL_FMT("Timeline.FrameCounter", currentFrame, totalFrames);
		toolbar.Text(frameLabel.c_str(), false);
	}

	ImGui::BeginDisabled(!effect);
	toolbar.SetNextItemWidth(ImGui::GetFontSize() * 4.5f);
	if (ImGui::SliderFloat("##TransportPlayRate", &m_PlayRate, 0.1f, 3.0f, "%.1fx"))
		m_PlayRate = std::clamp(m_PlayRate, 0.1f, 3.0f);
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		ImGui::SetTooltip("%s", LOCAL_CSTR("ParticleEditor.TooltipPlayRate"));
	ImGui::EndDisabled();

	const std::string zoomLabel = LOCAL_FMT("Timeline.ZoomFactor", m_Timeline.Zoom);
	toolbar.Text(zoomLabel.c_str(), false);
	EditorUI::DrawTimelineAction(toolbar, "ParticleTimeline", "ZoomReset", ICON_FA_MAXIMIZE, "Action.Timeline.ZoomReset");

	uint32 totalActiveParticles = 0;
	auto* scene = m_PreviewWidget.GetScene();
	if (m_PreviewEntity && scene && effect)
	{
		auto& comp = scene->GetComponent<Engine::ParticleEffectComponent>(*m_PreviewEntity);
		for (const auto& inst : comp.instances)
			totalActiveParticles += static_cast<uint32>(inst.particles.size());
	}

	if (effect)
	{
		const std::string activeCountLabel = fmt::format("{}: {}", LOCAL("Particle.ActiveCount"), totalActiveParticles);
		toolbar.Text(activeCountLabel.c_str(), false);
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
			[this]() { SetupPreview(); });
		m_History.Record(std::move(cmd));

		SetupPreview();
	}
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", LOCAL_CSTR("ParticleEditor.TooltipAddEmitter"));

	ImGui::Separator();
	// 표준 검색 바 (EditorUI::DrawSearchBar) 도입
	EditorUI::DrawSearchBar("ParticleEmitterSearch", m_EmitterSearch, sizeof(m_EmitterSearch), "ParticleEditor.SearchEmitters");
	ImGui::Spacing();
	const std::string filterLower = Engine::ToLowerStr(m_EmitterSearch);
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
			// 이미터 이름 검색 필터링
			if (!filterLower.empty() && Engine::ToLowerStr(em.name).find(filterLower) == std::string::npos)
				continue;
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
					[this]() { SetupPreview(); });
				m_History.Record(std::move(cmd));
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
	if (m_HasPreEditSnapshot) PushSnapshot(LOCAL("History.Command.EditProperty"));
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
		rows.back().ParticleTiming = EditorUI::ParticleTimelineTiming{
			emitter.lifetimeMin, emitter.lifetimeMax, effect->GetPlaybackDuration(), !continuous, emitter.enabled,
			emitter.triggerMode == Engine::EParticleEmitterTrigger::Continuous ||
				emitter.triggerMode == Engine::EParticleEmitterTrigger::OnParentCollision };
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
	if (action == Undo) return m_HasPreEditSnapshot || m_History.CanUndo();
	if (action == Redo) return !m_HasPreEditSnapshot && m_History.CanRedo();
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
	// 입력이 끝나면 이번 편집 세션을 이력 한 건으로 확정한다.
	if (m_HasPreEditSnapshot && !ImGui::IsAnyItemActive())
		PushSnapshot(LOCAL("History.Command.EditProperty"));

	auto* effect = m_EffectHandle.Get();
	auto* emitter = effect ? effect->GetEmitter(m_SelectedEmitterIndex) : nullptr;
	if (!emitter)
	{
		EditorUI::EmptyState("ParticleEditor.NoEmitterSelected");
		return;
	}

	const auto& registry = Engine::ReflectionRegistry::Get().GetRegistry();
	const auto object = reflection::ObjectView::From(*emitter);
	const auto* type = registry.FindType(object);
	if (!type)
	{
		EditorUI::EmptyState("Inspector.ValueUnavailable");
		return;
	}

	ImGui::PushID(m_SelectedEmitterIndex);
	ImGui::Text(ICON_FA_SLIDERS " %s [%s]", LOCAL_CSTR("CATEGORY_RENDER"), emitter->name.c_str());
	ImGui::Separator();

	// 그리기 전 상태. 이번 프레임에 처음 바뀌면 편집 세션의 시작 상태가 된다.
	auto before = CaptureSnapshot();
	bool changed = false;

	// 이름 목록이 필요한 부모 이미터는 공용 표에서 빼고 직접 그린다.
	const bool continuous = emitter->triggerMode == Engine::EParticleEmitterTrigger::Continuous;
	ReflectionObjectEditSettings settings;
	settings.HiddenProperties.insert("parentEmitterIndex");

	if (!continuous && EditorUI::BeginPropertyTable("ParticleParentEmitter", EditorUI::InspectorPropertyTableStyle()))
	{
		EditorUI::PropertyRow("PROP_PARENT_EMITTER_INDEX");
		ImGui::SetNextItemWidth(-FLT_MIN);
		const int parent = emitter->parentEmitterIndex;
		const std::string none = LOCAL("EditorCommon.None");
		const auto* parentEmitter = parent != m_SelectedEmitterIndex ? effect->GetEmitter(parent) : nullptr;
		const std::string preview = parentEmitter ? fmt::format("{}: {}", parent, parentEmitter->name) : none;
		if (ImGui::BeginCombo("##ParentEmitter", preview.c_str()))
		{
			if (ImGui::Selectable(none.c_str(), parentEmitter == nullptr) && parent != -1)
			{
				emitter->parentEmitterIndex = -1;
				changed = true;
			}
			for (int i = 0; i < effect->GetEmitterCount(); ++i)
			{
				if (i == m_SelectedEmitterIndex) continue; // 자기 자신은 부모가 될 수 없다
				const std::string label = fmt::format("{}: {}###ParentEmitter{}", i, effect->GetEmitter(i)->name, i);
				if (ImGui::Selectable(label.c_str(), parent == i) && parent != i)
				{
					emitter->parentEmitterIndex = i;
					changed = true;
				}
			}
			ImGui::EndCombo();
		}
		EditorUI::EndPropertyTable();
	}

	changed |= ReflectionPropertyDrawer::DrawPropertyTable(*type, object, &registry, &settings);

	if (changed)
	{
		if (!m_HasPreEditSnapshot)
		{
			m_PreEditSnapshot = std::move(before);
			m_HasPreEditSnapshot = true;
		}
	}

	ImGui::PopID();
}