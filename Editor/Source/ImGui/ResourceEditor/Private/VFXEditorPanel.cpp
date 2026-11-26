#include "VFXEditorPanel.h"
#include "AsyncResourceOpenService.h"
#include "NotificationManager.h"
#include <utility>
#include "EffectDocumentFile.h"
#include "EditorUI.h"
#include "ResourcePicker.h"
#include "IconsFontAwesome7.h"
#include "LocalizationManager.h"
#include "InputManager.h"
#include "ShortcutManager.h"
#include "FileDialogs.h"
#include "Functions.h"
#include "PlatformPath.h"
#include "Scene.h"
#include "WorldComponents.h"
#include "CompositeVFXComponent.h"
#include "LightComponent.h"
#include "CompositeVFXSystem.h"
#include "DecalProjection.h"
#include "ResourceManager.h"
#include "SpriteAnimation.h"
#include "Sprite.h"
#include "ParticleEffect.h"
#include "Mesh.h"
#include "Texture.h"
#include "MaterialInterface.h"
#include "AudioClip.h"
#include "ReflectionPropertyDrawer.h"
#include "ReflectionRegistry.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cmath>
#include <type_traits>
#include "RenderComponents.h"
#include "MaterialInstance.h"
#include "EditorCameraController.h"
#include "EditorSettings.h"

namespace
{
	struct CurvePreset
	{
		const char* LabelKey;
		Engine::FloatCurve(*Create)();
	};

	constexpr CurvePreset IntensityCurvePresets[] = {
		{ "VFXEditor.CurvePresetSpike", [] { return Engine::FloatCurve::CreateSpike(1.0f, 0.1f); } },
		{ "VFXEditor.CurvePresetEaseOut", [] { return Engine::FloatCurve::CreateEaseOut(1.0f, 0.0f); } },
		{ "VFXEditor.CurvePresetLinear", [] { return Engine::FloatCurve::CreateLinear(1.0f, 0.0f); } },
	};

	constexpr CurvePreset ScaleCurvePresets[] = {
		{ "VFXEditor.CurvePresetEaseOut", [] { return Engine::FloatCurve::CreateEaseOut(0.0f, 1.0f); } },
		{ "VFXEditor.CurvePresetEaseIn", [] { return Engine::FloatCurve::CreateEaseIn(0.0f, 1.0f); } },
		{ "VFXEditor.CurvePresetLinear", [] { return Engine::FloatCurve::CreateLinear(0.0f, 1.0f); } },
	};

	constexpr CurvePreset AlphaCurvePresets[] = {
		{ "VFXEditor.CurvePresetEaseOut", [] { return Engine::FloatCurve::CreateEaseOut(1.0f, 0.0f); } },
		{ "VFXEditor.CurvePresetLinear", [] { return Engine::FloatCurve::CreateLinear(1.0f, 0.0f); } },
		{ "VFXEditor.CurvePresetSpike", [] { return Engine::FloatCurve::CreateSpike(1.0f, 0.2f); } },
	};

	// ── 자유 함수: 커브 프리셋 행 (모달 호출 및 프리셋 버튼) ──
	bool DrawCurvePresetRow(const char* labelKey, Engine::FloatCurve& curve, std::span<const CurvePreset> presets,
		Editor::CurveEditorModal& modal, const std::function<std::function<void()>()>& captureEdit)
	{
		EditorUI::PropertyRow(labelKey);
		ImGui::PushID(labelKey);
		bool changed = false;
		// 1. [커브 모달 에디터 열기] 베지어 아이콘 버튼
		if (EditorUI::SmallIconButton("EditCurve", ICON_FA_BEZIER_CURVE, "VFXEditor.EditCurveModal"))
		{
			const std::string title = LOCAL_FMT("VFXEditor.CurveModalTitle", LOCAL_CSTR(labelKey));
			modal.Open(title, &curve, captureEdit());
		}
		ImGui::SameLine();
		// 2. 빠른 프리셋 버튼들
		for (std::size_t i = 0; i < presets.size(); ++i)
		{
			const std::string label = LOCAL_LABEL(presets[i].LabelKey);
			if (i > 0) EditorUI::SameLineIfFits(ImGui::CalcTextSize(label.c_str(), nullptr, true).x + ImGui::GetStyle().FramePadding.x * 2.0f);
			if (ImGui::SmallButton(label.c_str())) { curve = presets[i].Create(); changed = true; }
		}
		ImGui::PopID();
		return changed;
	}

	struct VFXTrackPresentation { const char* Icon; ImU32 Color; };
	constexpr VFXTrackPresentation g_VFXTrackPresentation[] = {
		{ ICON_FA_IMAGES, IM_COL32(40, 180, 160, 220) },
		{ ICON_FA_WAND_MAGIC_SPARKLES, IM_COL32(230, 130, 40, 220) },
		{ ICON_FA_LIGHTBULB, IM_COL32(230, 200, 50, 220) },
		{ ICON_FA_CUBE, IM_COL32(60, 130, 230, 220) },
		{ ICON_FA_VOLUME_HIGH, IM_COL32(180, 70, 220, 220) },
		{ ICON_FA_WIND, IM_COL32(50, 220, 190, 220) },
		{ ICON_FA_IMAGE, IM_COL32(170, 90, 190, 220) },
		{ ICON_FA_CAMERA, IM_COL32(235, 80, 80, 220) }
	};

	template<class Fn>
	void ForEachVFXTrackList(Engine::CompositeEffect& effect, Fn&& fn)
	{
		fn(ESelectedVFXTrackType::Sprite, effect.GetSpriteTracks());
		fn(ESelectedVFXTrackType::Particle, effect.GetParticleTracks());
		fn(ESelectedVFXTrackType::Light, effect.GetLightTracks());
		fn(ESelectedVFXTrackType::Mesh, effect.GetMeshTracks());
		fn(ESelectedVFXTrackType::Sound, effect.GetSoundTracks());
		fn(ESelectedVFXTrackType::Trail, effect.GetTrailTracks());
		fn(ESelectedVFXTrackType::Decal, effect.GetDecalTracks());
		fn(ESelectedVFXTrackType::CameraShake, effect.GetCameraShakeTracks());
	}

	template<class Fn>
	bool VisitVFXTrack(Engine::CompositeEffect& effect, SelectedVFXTrack selection, Fn&& fn)
	{
		bool found = false;
		ForEachVFXTrackList(effect, [&](ESelectedVFXTrackType type, auto& tracks)
		{
			if (type == selection.type && selection.index >= 0 && static_cast<std::size_t>(selection.index) < tracks.size())
			{
				found = true;
				fn(tracks, static_cast<std::size_t>(selection.index));
			}
		});
		return found;
	}

	float VFXTrackEnd(const auto& track)
	{
		if constexpr (requires { track.duration; })
		{
			if (track.duration > 0.0f) return track.startTime + track.duration;
			if constexpr (requires { track.animationHandle; })
				if (const auto* animation = track.animationHandle.Get()) return track.startTime + animation->GetTotalDuration();
			return track.startTime + EditorUI::MinimumTimelineDuration;
		}
		return track.startTime;
	}

	std::uint64_t VFXRowId(ESelectedVFXTrackType type, std::size_t index)
	{
		return (static_cast<std::uint64_t>(type) << 32) | static_cast<std::uint32_t>(index);
	}
}

VFXEditorPanel::VFXEditorPanel()
{
	m_Name = L"Composite VFX Editor";
	m_DisplayNameKey = "VFXEditor.Title";
	m_PreviewWidget.Initialize(L"VFXEditorPreview", 512, 512, true);
	m_PreviewWidget.SetupDefaultStudioLighting();
	m_PreviewWidget.FocusTarget(Engine::vec3(0.0f, 0.5f, 0.0f), 3.5f);

	for (auto* effect : m_PreviewWidget.GetPostProcessChain().GetPipeline().GetEffects())
	{
		if (effect && effect->GetName() == L"Bloom")
		{
			effect->SetEnabled(true);
		}
	}

	// 기본 배경 프리셋 (18% 스튜디오 뉴트럴 그레이) 적용
	ApplyBackgroundPreset();
}

VFXEditorPanel::~VFXEditorPanel()
{
	CancelAsyncOpen();
	ReleaseResourceDocument();
	m_PreviewWidget.Free();
}

bool VFXEditorPanel::IsSupported(const std::filesystem::path& assetPath) const
{
	const std::string ext = assetPath.extension().string();
	const std::string name = WStrToStr(assetPath.filename().wstring());
	return (ext == ".bamvfx" || ext == ".vfx" || name.ends_with(".bamvfx.json") || name.ends_with(".vfx.json"));
}

void VFXEditorPanel::SetTargetResource(const std::filesystem::path& assetPath)
{
	CancelAsyncOpen();
	const auto path = Engine::PlatformPath::ToProjectRelative(assetPath).wstring();
	auto& manager = Engine::ResourceManager::Get();
	auto candidate = manager.GetResourceHandle<Engine::CompositeEffect>(path);
	if (!candidate.IsValid())
		candidate = Engine::ResourceHandle<Engine::CompositeEffect>(manager.LoadFile(path));
	if (!candidate.Get())
	{
		auto* effect = Engine::CompositeEffect::Create();
		if (!effect) return;
		const auto resolved = Engine::PlatformPath::ResolveProjectRelative(assetPath);
		if (!effect->LoadFromFile(resolved.wstring()))
		{
			manager.DestroyResource(effect);
			return;
		}
		candidate = manager.AddResource<Engine::CompositeEffect>(path, effect);
	}
	if (!candidate.Get() || !RestoreSavedChanges()) return;

	ReleaseResourceDocument();
	m_TargetResourcePath = assetPath;
	m_EffectHandle = std::move(candidate);
	m_IsLooping = m_EffectHandle.Get()->IsLooping();
	SetupPreview();
}

bool VFXEditorPanel::BeginAsyncOpen(const std::filesystem::path& assetPath)
{
	if (!IsSupported(assetPath))
		return false;

	if (!RestoreSavedChanges())
		return false;

	CancelAsyncOpen();

	m_AsyncTargetPath = assetPath;
	m_AsyncStartTime = std::chrono::steady_clock::now();
	m_AsyncLoadStart = m_AsyncStartTime;
	m_AsyncLoadDurationMs = 0.0;
	m_AsyncStage = EAsyncStage::WaitingBackground;
	m_AsyncProgress = 0.08f;

	Open();
	Focus();
	return true;
}

bool VFXEditorPanel::StepAsyncOpen(float maxTimeSeconds)
{
	using Clock = std::chrono::steady_clock;
	using Milliseconds = std::chrono::duration<double, std::milli>;
	const auto stepDeadline = Clock::now() + std::chrono::duration_cast<Clock::duration>(
		std::chrono::duration<float>(maxTimeSeconds));

	if (m_AsyncStage == EAsyncStage::WaitingBackground)
	{
		m_AsyncStage = EAsyncStage::LoadingResource;
		m_AsyncProgress = 0.35f;
		m_AsyncLoadStart = Clock::now();
	}

	if (m_AsyncStage == EAsyncStage::LoadingResource)
	{
		const auto path = Engine::PlatformPath::ToProjectRelative(m_AsyncTargetPath).wstring();
		auto& manager = Engine::ResourceManager::Get();
		auto candidate = manager.GetResourceHandle<Engine::CompositeEffect>(path);
		if (!candidate.IsValid())
			candidate = Engine::ResourceHandle<Engine::CompositeEffect>(manager.LoadFile(path));
		if (!candidate.Get())
		{
			auto* effect = Engine::CompositeEffect::Create();
			if (effect)
			{
				const auto resolved = Engine::PlatformPath::ResolveProjectRelative(m_AsyncTargetPath);
				if (effect->LoadFromFile(resolved.wstring()))
					candidate = manager.AddResource<Engine::CompositeEffect>(path, effect);
				else
					manager.DestroyResource(effect);
			}
		}

		if (!candidate.Get())
		{
			NotificationManager::Get().AddNotification(GetDisplayName(),
				LOCAL_FMT("Notification.ResourceLoadFailed", WStrToStr(m_AsyncTargetPath.filename().wstring())), ELogLevel::Error);
			m_AsyncPendingEffect.Reset();
			m_AsyncTargetPath.clear();
			m_AsyncProgress = 0.0f;
			m_AsyncStage = m_EffectHandle.Get() ? EAsyncStage::None : EAsyncStage::Failed;
			return true;
		}

		m_AsyncPendingEffect = std::move(candidate);
		m_AsyncLoadDurationMs = Milliseconds(Clock::now() - m_AsyncLoadStart).count();
		m_AsyncStage = EAsyncStage::SettingUpPreview;
		m_AsyncProgress = 0.70f;

		if (Clock::now() >= stepDeadline)
			return false;
	}

	if (m_AsyncStage == EAsyncStage::SettingUpPreview)
	{
		if (!RestoreSavedChanges())
		{
			m_AsyncPendingEffect.Reset();
			m_AsyncTargetPath.clear();
			m_AsyncProgress = 0.0f;
			m_AsyncStage = EAsyncStage::None;
			return true;
		}

		const auto previewStart = Clock::now();
		auto targetPath = std::move(m_AsyncTargetPath);
		auto pendingEffect = std::move(m_AsyncPendingEffect);
		ReleaseResourceDocument();
		m_TargetResourcePath = targetPath;
		m_EffectHandle = std::move(pendingEffect);
		m_IsLooping = m_EffectHandle.Get()->IsLooping();
		SetupPreview();
		const auto previewEnd = Clock::now();
		const double previewMs = Milliseconds(previewEnd - previewStart).count();
		const double totalMs = Milliseconds(previewEnd - m_AsyncStartTime).count();

		EDITOR_LOG_INFO("[ResourceOpenTiming] VFX {}: resource={:.2f}ms, preview={:.2f}ms, total={:.2f}ms (async)",
			WStrToStr(targetPath.wstring()), m_AsyncLoadDurationMs, previewMs, totalMs);

		m_AsyncProgress = 1.0f;
		m_AsyncStage = EAsyncStage::None;
		return true;
	}

	return true;
}

void VFXEditorPanel::CancelAsyncOpen()
{
	AsyncResourceOpenService::Get().CancelRequestsForEditor(this);
	m_AsyncStage = EAsyncStage::None;
	m_AsyncProgress = 0.0f;
	m_AsyncTargetPath.clear();
	m_AsyncPendingEffect.Reset();
}

void VFXEditorPanel::DrawAsyncLoadingOverlay()
{
	DrawStandardAsyncLoadingOverlay(m_AsyncTargetPath, m_AsyncProgress,
		m_AsyncStage == EAsyncStage::WaitingBackground, m_AsyncStage == EAsyncStage::Failed);
}

static void ApplySavedCompositeEffect(Engine::CompositeEffect& effect, Engine::CompositeEffect& saved)
{
	effect.GetSpriteTracks().swap(saved.GetSpriteTracks());
	effect.GetParticleTracks().swap(saved.GetParticleTracks());
	effect.GetLightTracks().swap(saved.GetLightTracks());
	effect.GetMeshTracks().swap(saved.GetMeshTracks());
	effect.GetSoundTracks().swap(saved.GetSoundTracks());
	effect.GetTrailTracks().swap(saved.GetTrailTracks());
	effect.GetDecalTracks().swap(saved.GetDecalTracks());
	effect.GetCameraShakeTracks().swap(saved.GetCameraShakeTracks());
	effect.SetTotalDuration(saved.GetTotalDuration());
	effect.SetLooping(saved.IsLooping());
	effect.IncreaseVersion();
}

bool VFXEditorPanel::RestoreSavedResource()
{
	if (m_TargetResourcePath.empty()) return true;
	auto* effect = m_EffectHandle.Get();
	auto saved = MakeEffectDocument<Engine::CompositeEffect>();
	if (!effect || !saved || !LoadEffectDocument(*saved, m_TargetResourcePath)) return false;

	ApplySavedCompositeEffect(*effect, *saved);
	return true;
}

void VFXEditorPanel::ReleaseResourceDocument()
{
	CancelAsyncOpen();
	StopPreviewSounds();
	auto* scene = m_PreviewWidget.GetScene();
	if (scene && m_PreviewEntity) (void)scene->DestroyEntity(*m_PreviewEntity);
	m_PreviewEntity = nullptr;
	m_InspectorEdit.reset();
	m_CurveModal = {};
	m_TimelineHistory.Clear();
	MarkSaved();
	m_SelectedTrack.Clear();
	m_IsLooping = false;
	m_CurrentTime = 0.0f;
	const auto handle = m_EffectHandle.GetRawHandle();
	m_EffectHandle.Reset();
	if (std::exchange(m_OwnsTemporaryEffect, false)) Engine::ResourceManager::Get().ReleaseResource(handle);
	m_Timeline.Drag.reset();
	m_Timeline.Fit();
	m_TimelineSeek.reset();
	m_StatusMessage.clear();
}

void VFXEditorPanel::CaptureState(EditorWorkspace& workspace) const
{
	IImGui::CaptureState(workspace);
	workspace.SetSplitter("VFXEditor.LeftPaneWidth", m_LeftPaneWidth);
	workspace.SetSplitter("VFXEditor.RightPaneWidth", m_RightPaneWidth);
	workspace.SetSplitter("VFXEditor.TimelineHeight", m_TimelineHeight);
	workspace.SetSplitter("VFXEditor.TimelineHeaderWidth", m_Timeline.HeaderWidth);
}

void VFXEditorPanel::RestoreState(const EditorWorkspace& workspace)
{
	IImGui::RestoreState(workspace);
	m_LeftPaneWidth = workspace.GetSplitter("VFXEditor.LeftPaneWidth", m_LeftPaneWidth);
	m_RightPaneWidth = workspace.GetSplitter("VFXEditor.RightPaneWidth", m_RightPaneWidth);
	m_TimelineHeight = workspace.GetSplitter("VFXEditor.TimelineHeight", m_TimelineHeight);
	m_Timeline.HeaderWidth = workspace.GetSplitter("VFXEditor.TimelineHeaderWidth", m_Timeline.HeaderWidth);
}

void VFXEditorPanel::ApplyBackgroundPreset()
{
	switch (m_BackgroundMode)
	{
	case EVFXBackgroundMode::StudioGray:
		// 18% 뉴트럴 스튜디오 그레이: 바닥 자체 블룸을 방지하면서 플래시 명암 대비 극대화
		m_PreviewWidget.SetBackgroundColor(Engine::vec4(0.18f, 0.18f, 0.19f, 1.0f));
		m_PreviewWidget.SetStudioLightMultiplier(0.15f);
		break;
	case EVFXBackgroundMode::Dark:
		// 암실/나이트 환경: 외부 조명 완전 소등 -> 오직 이펙트 플래시 라이트만 바닥을 밝힘
		m_PreviewWidget.SetBackgroundColor(Engine::vec4(0.02f, 0.02f, 0.03f, 1.0f));
		m_PreviewWidget.SetStudioLightMultiplier(0.0f);
		break;
	case EVFXBackgroundMode::Bright:
		// 주간 야외 환경: 연기/먼지 실루엣 대비 검증
		m_PreviewWidget.SetBackgroundColor(Engine::vec4(0.72f, 0.72f, 0.74f, 1.0f));
		m_PreviewWidget.SetStudioLightMultiplier(0.6f);
		break;
	}
}

namespace
{
	Engine::ResourceHandle<Engine::Texture> GetOrCreateVFXCheckerTexture(Engine::ResourceManager& rm)
	{
		constexpr const wchar_t* key = L"VFXPreviewCheckerTex";
		if (auto existing = rm.GetResourceHandle<Engine::Texture>(key); existing.IsValid())
			return existing;

		constexpr uint32 size = 64;
		constexpr uint32 half = size / 2;
		std::vector<Engine::uint8> pixels(static_cast<size_t>(size) * size * 4);
		for (uint32 y = 0; y < size; ++y)
		{
			for (uint32 x = 0; x < size; ++x)
			{
				const bool light = ((x < half) == (y < half));
				const Engine::uint8 r = light ? 235 : 95;
				const Engine::uint8 g = light ? 235 : 95;
				const Engine::uint8 b = light ? 242 : 105;
				const size_t idx = (static_cast<size_t>(y) * size + x) * 4;
				pixels[idx + 0] = r;
				pixels[idx + 1] = g;
				pixels[idx + 2] = b;
				pixels[idx + 3] = 255;
			}
		}

		Engine::TextureBinaryHeader header{};
		header.width = size;
		header.height = size;
		header.depth = 1;
		header.mipLevels = 1;
		header.arraySize = 1;
		header.format = Engine::ETextureFormat::R8G8B8A8_UNORM;
		header.dimension = Engine::ETextureDimension::Texture2D;
		header.dataSize = static_cast<uint32>(pixels.size());

		auto* tex = Engine::Texture::Create(header, pixels, key);
		return tex ? rm.AddResource<Engine::Texture>(key, tex) : Engine::ResourceHandle<Engine::Texture>{};
	}

	Engine::ResourceHandle<Engine::Mesh> GetOrCreateVFXTiledPlaneMesh(Engine::ResourceManager& rm)
	{
		constexpr const wchar_t* key = L"VFXPreviewTiledPlaneMesh";
		if (auto existing = rm.GetResourceHandle<Engine::Mesh>(key); existing.IsValid())
			return existing;

		constexpr float uvRepeat = 24.0f; // 48m 바닥에서 2m당 1타일(1m 체커 칸)
		std::vector<Engine::VertexPosition> positions = {
			{ { -0.5f, 0.0f,  0.5f } },
			{ {  0.5f, 0.0f,  0.5f } },
			{ {  0.5f, 0.0f, -0.5f } },
			{ { -0.5f, 0.0f, -0.5f } }
		};
		std::vector<Engine::VertexMaterial> materials = {
			{ { 0, 1, 0 }, { 0.0f,     0.0f     }, { 1, 0, 0 }, { 0, 0, 1 }, Engine::vec4(1.0f) },
			{ { 0, 1, 0 }, { uvRepeat, 0.0f     }, { 1, 0, 0 }, { 0, 0, 1 }, Engine::vec4(1.0f) },
			{ { 0, 1, 0 }, { uvRepeat, uvRepeat }, { 1, 0, 0 }, { 0, 0, 1 }, Engine::vec4(1.0f) },
			{ { 0, 1, 0 }, { 0.0f,     uvRepeat }, { 1, 0, 0 }, { 0, 0, 1 }, Engine::vec4(1.0f) }
		};
		std::vector<uint32> indices = { 0, 1, 2, 0, 2, 3 };

		Engine::MeshCreateDesc planeDesc{};
		planeDesc.Streams[static_cast<uint32>(Engine::EMeshStream::Position)] = { positions.data(), static_cast<uint32>(positions.size()), sizeof(Engine::VertexPosition) };
		planeDesc.Streams[static_cast<uint32>(Engine::EMeshStream::Material)] = { materials.data(), static_cast<uint32>(materials.size()), sizeof(Engine::VertexMaterial) };
		planeDesc.IndexData = indices.data();
		planeDesc.IndexCount = static_cast<uint32>(indices.size());
		planeDesc.IndexStride = sizeof(uint32);
		planeDesc.BoundingBoxMin = Engine::vec3(-0.5f, 0.0f, -0.5f);
		planeDesc.BoundingBoxMax = Engine::vec3(0.5f, 0.0f, 0.5f);

		auto* mesh = Engine::Mesh::Create(&planeDesc);
		return mesh ? rm.AddResource<Engine::Mesh>(key, mesh) : Engine::ResourceHandle<Engine::Mesh>{};
	}
}

void VFXEditorPanel::ApplyGroundBackdrop()
{
	auto& rm = Engine::ResourceManager::Get();
	if (auto groundMatHandle = rm.GetResourceHandle<Engine::MaterialInterface>(L"VFXPreviewGroundMatInst");
		auto* groundMat = groundMatHandle.Get())
	{
		if (m_ShowCheckerBackdrop)
		{
			if (auto checkerTex = GetOrCreateVFXCheckerTexture(rm); checkerTex.IsValid())
			{
				groundMat->SetTextureBinding("Diffuse", 0, checkerTex);
				groundMat->AddColor4("BaseColor", Engine::vec4(0.55f, 0.55f, 0.58f, 1.0f));
				groundMat->IncreaseVersion();
			}
		}
		else
		{
			auto whiteTex = rm.GetResourceHandle<Engine::Texture>(L"Engine://Texture/white1x1.png");
			if (!whiteTex.IsValid())
				if (auto loaded = rm.LoadFile(L"Engine://Texture/white1x1.png"); loaded.IsValid())
					whiteTex = Engine::ResourceHandle<Engine::Texture>(loaded);
			if (whiteTex.IsValid())
			{
				groundMat->SetTextureBinding("Diffuse", 0, whiteTex);
				groundMat->AddColor4("BaseColor", Engine::vec4(0.24f, 0.24f, 0.27f, 1.0f));
				groundMat->IncreaseVersion();
			}
		}
	}

	if (m_PreviewGroundEntity)
	{
		if (auto* scene = m_PreviewWidget.GetScene())
		{
			if (auto* renderer = scene->GetRegistry().try_get<Engine::StaticMeshRendererComponent>(m_PreviewGroundEntity->GetEntityHandle()))
			{
				auto mesh = m_ShowCheckerBackdrop ? GetOrCreateVFXTiledPlaneMesh(rm) : rm.GetResourceHandle<Engine::Mesh>(L"PlaneMesh");
				if (mesh.IsValid())
					renderer->meshHandle = mesh;
			}
		}
	}
}

void VFXEditorPanel::SetupPreview()
{
	auto* scene = m_PreviewWidget.GetScene();
	if (!scene) return;
	ApplyBackgroundPreset();
	if (m_PreviewEntity)
	{
		(void)scene->DestroyEntity(*m_PreviewEntity);
		m_PreviewEntity = nullptr;
	}
	if (m_PreviewGroundEntity)
	{
		(void)scene->DestroyEntity(*m_PreviewGroundEntity);
		m_PreviewGroundEntity = nullptr;
	}
	for (auto* obs : m_ObstacleEntities)
	{
		if (obs) (void)scene->DestroyEntity(*obs);
	}
	m_ObstacleEntities.clear();
	auto& rm = Engine::ResourceManager::Get();
	// 스튜디오 공용 차콜 그레이 PBR 머티리얼 준비
	const wchar_t* defaultMatPath = L"Resources/Material/DefaultMaterial.bammat.json";
	auto baseMat = rm.GetResourceHandle<Engine::Material>(defaultMatPath);
	if (!baseMat.IsValid())
	{
		auto loaded = rm.LoadFile(defaultMatPath);
		if (loaded.IsValid()) baseMat = Engine::ResourceHandle<Engine::Material>(loaded);
	}
	auto groundMatHandle = rm.GetResourceHandle<Engine::MaterialInterface>(L"VFXPreviewGroundMatInst");
	if (!groundMatHandle.IsValid() && baseMat.IsValid())
	{
		auto* groundMatInst = Engine::MaterialInstance::Create(baseMat);
		if (groundMatInst)
		{
			groundMatInst->AddColor4("BaseColor", Engine::vec4(0.24f, 0.24f, 0.27f, 1.0f));
			groundMatInst->AddFloat("Roughness", 0.55f);
			groundMatInst->AddFloat("Metallic", 0.0f);
			groundMatHandle = rm.AddResource<Engine::MaterialInterface>(L"VFXPreviewGroundMatInst", groundMatInst);
		}
	}
	// 1. 기본 지면 평면 생성 (48m Plane)
	m_PreviewGroundEntity = &m_PreviewWidget.CreateEntity(L"VFXPreviewGround");
	(void)scene->AddComponent<Engine::FlagComponent>(*m_PreviewGroundEntity);
	auto& groundTrans = scene->GetComponent<Engine::TransformComponent>(*m_PreviewGroundEntity);
	const float groundScale = m_ShowGroundPlane ? 48.0f : 0.0f;
	(void)groundTrans.SetLocalPosition(Engine::vec3(0.0f, 0.0f, 0.0f));
	(void)groundTrans.SetLocalScale(Engine::vec3(groundScale, 1.0f, groundScale));
	(void)groundTrans.SetLocalRotation(Engine::vec3(0.0f, 0.0f, 0.0f));
	auto& groundRenderer = scene->AddComponent<Engine::StaticMeshRendererComponent>(*m_PreviewGroundEntity);
	groundRenderer.meshHandle = rm.GetResourceHandle<Engine::Mesh>(L"PlaneMesh");
	groundRenderer.materialHandle = groundMatHandle.IsValid() ? groundMatHandle : Engine::ResourceHandle<Engine::MaterialInterface>(baseMat);
	m_BackdropDirty = true;
	// 2. 환경 장애물 프리셋 엔티티 생성
	auto createObstacle = [&](const std::wstring& name, const std::wstring& meshName,
		const Engine::vec3& pos, const Engine::vec3& rot, const Engine::vec3& scale) -> Engine::Entity*
		{
			auto* ent = &m_PreviewWidget.CreateEntity(name);
			(void)scene->AddComponent<Engine::FlagComponent>(*ent);
			auto& tr = scene->GetComponent<Engine::TransformComponent>(*ent);
			(void)tr.SetLocalPosition(pos);
			(void)tr.SetLocalRotation(rot);
			(void)tr.SetLocalScale(scale);
			auto& mr = scene->AddComponent<Engine::StaticMeshRendererComponent>(*ent);
			mr.meshHandle = rm.GetResourceHandle<Engine::Mesh>(meshName);
			mr.materialHandle = groundRenderer.materialHandle;
			return ent;
		};
	if (m_ObstaclePreset == EVFXObstaclePreset::CornerWall)
	{
		// L자형 90도 수직 벽면 2개 (연기/파티클 벽면 충돌 및 반사 검증)
		m_ObstacleEntities.push_back(createObstacle(L"CornerWall_Back", L"PlaneMesh",
			Engine::vec3(0.0f, 6.0f, 6.0f), Engine::vec3(90.0f, 0.0f, 0.0f), Engine::vec3(12.0f, 1.0f, 12.0f)));
		m_ObstacleEntities.push_back(createObstacle(L"CornerWall_Left", L"PlaneMesh",
			Engine::vec3(-6.0f, 6.0f, 0.0f), Engine::vec3(0.0f, 0.0f, -90.0f), Engine::vec3(12.0f, 1.0f, 12.0f)));
	}
	else if (m_ObstaclePreset == EVFXObstaclePreset::Obstacles)
	{
		// 중앙 반사 구체(Sphere) 및 차폐 박스(Cube)
		m_ObstacleEntities.push_back(createObstacle(L"Obstacle_Sphere", L"SphereMesh",
			Engine::vec3(1.2f, 0.75f, 0.0f), Engine::vec3(0.0f), Engine::vec3(1.5f)));
		m_ObstacleEntities.push_back(createObstacle(L"Obstacle_Box", L"CubeMesh",
			Engine::vec3(-1.2f, 0.5f, 0.0f), Engine::vec3(0.0f, 35.0f, 0.0f), Engine::vec3(1.0f)));
	}
	else if (m_ObstaclePreset == EVFXObstaclePreset::DummyCharacter)
	{
		// 1.8m 인간형 더미 (피격 이펙트 및 캐릭터 오라 검증)
		m_ObstacleEntities.push_back(createObstacle(L"Dummy_Head", L"SphereMesh",
			Engine::vec3(0.0f, 1.6f, 0.0f), Engine::vec3(0.0f), Engine::vec3(0.35f)));
		m_ObstacleEntities.push_back(createObstacle(L"Dummy_Body", L"CubeMesh",
			Engine::vec3(0.0f, 0.85f, 0.0f), Engine::vec3(0.0f), Engine::vec3(0.5f, 1.1f, 0.3f)));
	}
	// 3. 복합 이펙트 프리뷰 엔티티 생성
	auto* effect = m_EffectHandle.Get();
	if (!effect) return;
	m_PreviewEntity = &m_PreviewWidget.CreateEntity(L"CompositeVFXPreview");
	(void)scene->AddComponent<Engine::FlagComponent>(*m_PreviewEntity);
	auto& comp = scene->AddComponent<Engine::CompositeVFXComponent>(*m_PreviewEntity);
	comp.effectHandle = m_EffectHandle;
	comp.isPlaying = m_IsPlaying;
	comp.isLooping = m_IsLooping;
	comp.playRate = m_PlayRate;
	comp.externalWind = m_SimWind;
	comp.gravityMultiplier = m_SimGravityMultiplier;
	comp.previewSeed = 1;
	RestartSimulation();
}

void VFXEditorPanel::ToggleAirMode()
{
	m_AirMode = !m_AirMode;
	if (m_AirMode)
	{
		m_SavedGroundPlane = m_ShowGroundPlane;
		m_SavedGrid = m_PreviewWidget.IsGridVisible();
		m_ShowGroundPlane = false;
		m_PreviewWidget.SetGridVisible(false);

		// 주간 야외(Bright) 상태였다면 단일 스튜디오 그레이로 전환하여 수평선 단차를 완전 제거
		if (m_BackgroundMode == EVFXBackgroundMode::Bright)
		{
			m_BackgroundMode = EVFXBackgroundMode::StudioGray;
			ApplyBackgroundPreset();
		}
	}
	else
	{
		m_ShowGroundPlane = m_SavedGroundPlane;
		m_PreviewWidget.SetGridVisible(m_SavedGrid);
	}

	if (m_PreviewGroundEntity)
	{
		if (auto* scene = m_PreviewWidget.GetScene())
		{
			auto* trans = scene->GetRegistry().try_get<Engine::TransformComponent>(m_PreviewGroundEntity->GetEntityHandle());
			if (trans)
			{
				const float s = m_ShowGroundPlane ? 48.0f : 0.0f;
				(void)trans->SetLocalScale(Engine::vec3(s, 1.0f, s));
			}
		}
	}
}

float VFXEditorPanel::GetTimelineDuration() const
{
	const auto* effect = m_EffectHandle.Get();
	const float duration = effect ? effect->GetTotalDuration() : 0.0f;
	return std::isfinite(duration) ? std::max(duration, EditorUI::MinimumTimelineDuration) : 1.0f;
}

void VFXEditorPanel::RestartSimulation()
{
	if (auto* scene = m_PreviewWidget.GetScene(); scene && m_PreviewEntity)
	{
		auto& comp = scene->GetComponent<Engine::CompositeVFXComponent>(*m_PreviewEntity);
		comp.particleRuntimes.clear();
		comp.Restart();
		comp.isPlaying = m_IsPlaying;
	}
	m_CurrentTime = 0.0f;
	m_TimelineSeek.reset();
	m_FixedPreview = true;
}

void VFXEditorPanel::ScrubTime(f32 targetTime)
{
	const float duration = GetTimelineDuration();
	if (!std::isfinite(targetTime)) return;
	if (targetTime < duration)
		targetTime = std::round(targetTime * EditorUI::EffectTimelineFPS) / EditorUI::EffectTimelineFPS;
	targetTime = std::clamp(targetTime, 0.0f, duration);
	m_IsPlaying = false;
	StopPreviewSounds();
	if (!m_FixedPreview || targetTime + 0.0001f < m_CurrentTime) RestartSimulation();
	m_TimelineSeek = targetTime;
}

void VFXEditorPanel::MarkSaved()
{
	m_UntrackedDirty = false;
	m_SavedStateId = m_TimelineHistory.GetCurrentStateId();
}

void VFXEditorPanel::InvalidateTimelinePreview()
{
	m_FixedPreview = false;
	ScrubTime(m_TimelineSeek.value_or(m_CurrentTime));
}

void VFXEditorPanel::Update(f32 dt)
{
	if (!m_Open || IsAsyncLoading()) return;
	auto* scene = m_PreviewWidget.GetScene();
	if (m_PreviewEntity && scene && m_EffectHandle.IsValid())
	{
		auto& comp = scene->GetComponent<Engine::CompositeVFXComponent>(*m_PreviewEntity);
		comp.externalWind = m_SimWind;
		comp.gravityMultiplier = m_SimGravityMultiplier;

		// 프리뷰 모션: 활성화 시 엔티티를 3D 원형 궤도로 비행시켜 트레일 스트립 생성 유도
		if (m_PreviewMotion && (m_IsPlaying || m_TimelineSeek))
		{
			const float curT = m_TimelineSeek.value_or(m_CurrentTime);
			const float angle = curT * 3.14159265f * 2.5f;
			const float rad = 1.2f;
			const Engine::vec3 motionPos = Engine::vec3(
				std::cos(angle) * rad,
				0.5f + std::sin(angle * 2.0f) * 0.35f,
				std::sin(angle) * rad);
			auto* trans = scene->GetRegistry().try_get<Engine::TransformComponent>(m_PreviewEntity->GetEntityHandle());
			if (trans) (void)trans->SetLocalPosition(motionPos);
		}

		if (m_TimelineSeek)
		{
			const float target = *m_TimelineSeek;
			comp.previewSeeking = true;
			comp.isLooping = false;
			comp.playRate = 1.0f;
			comp.isPlaying = true;
			for (int i = 0; i < 32 && comp.currentTime + 0.00001f < target; ++i)
				Engine::CompositeVFXSystem::SimulateSceneVFX(scene,
					std::min(1.0f / EditorUI::EffectTimelineFPS, target - comp.currentTime));
			m_CurrentTime = comp.currentTime;
			if (m_CurrentTime + 0.00001f >= target) m_TimelineSeek.reset();
			comp.previewSeeking = false;
			comp.isPlaying = false;
			Engine::CompositeVFXSystem::RefreshFlashLight(comp);
		}
		else
		{
			comp.isPlaying = m_IsPlaying;
			comp.isLooping = m_IsLooping;
			comp.playRate = m_PlayRate;
			if (m_IsPlaying)
			{
				m_FixedPreview = false;
				Engine::CompositeVFXSystem::SimulateSceneVFX(scene, dt);
				m_CurrentTime = comp.currentTime;
				m_IsPlaying = comp.isPlaying;
			}
			Engine::CompositeVFXSystem::RefreshFlashLight(comp);
		}
	}
	if (m_BackdropDirty)
	{
		ApplyGroundBackdrop();
		m_BackdropDirty = false;
	}
	m_PreviewWidget.BeginFrame(dt);
	m_PreviewWidget.Submit(dt);
}

void VFXEditorPanel::NewEffect()
{
	CancelAsyncOpen();
	Engine::CompositeEffectCreateDesc desc;
	desc.totalDuration = 2.0f;
	desc.isLooping = false;
	auto* newEffect = Engine::CompositeEffect::Create(&desc);
	if (!newEffect) return;
	if (!RestoreSavedChanges())
	{
		Engine::ResourceManager::Get().DestroyResource(newEffect);
		return;
	}

	ReleaseResourceDocument();
	m_TargetResourcePath.clear();
	static uint64 nextDraft = 0;
	const auto key = L"EditorVfxDraft_" + std::to_wstring(++nextDraft);
	m_EffectHandle = Engine::ResourceManager::Get().AddResource<Engine::CompositeEffect>(key, newEffect);
	m_OwnsTemporaryEffect = true;
	m_UntrackedDirty = true;
	SetupPreview();
}

void VFXEditorPanel::OpenEffect()
{
	std::wstring openPath;
	if (FileDialogs::OpenFileDialog(openPath, { { L"Visual Effect (*.bamvfx.json, *.bamvfx)", L"*.bamvfx.json;*.bamvfx;*.vfx" } }, L"", EFileDialogContext::General))
	{
		const std::filesystem::path selectedPath(openPath);
		if (!AsyncResourceOpenService::Get().RequestOpen(this, selectedPath))
			SetTargetResource(selectedPath);
	}
}

void VFXEditorPanel::SaveEffect()
{
	if (m_TargetResourcePath.empty()) SaveEffectAs();
	else SaveEffectTo(m_TargetResourcePath);
}

void VFXEditorPanel::SaveEffectTo(const std::filesystem::path& path)
{
	auto* effect = m_EffectHandle.Get();
	if (!effect) return;
	if (m_InspectorEdit) CommitInspectorEdit();

	const bool copyToNewDocument = !m_OwnsTemporaryEffect && !m_TargetResourcePath.empty() &&
		!IsSameEffectDocument(m_TargetResourcePath, path);
	if (copyToNewDocument)
	{
		auto copy = SaveEffectDocumentCopy(*effect, m_TargetResourcePath, path, ApplySavedCompositeEffect);
		if (!copy.Get())
		{
			m_StatusMessage = LOCAL("ResourceEditor.SaveFailed");
			return;
		}
		ReleaseResourceDocument();
		m_EffectHandle = std::move(copy);
		m_IsLooping = m_EffectHandle.Get()->IsLooping();
		SetupPreview();
	}
	else if (!effect->SaveToFile(Engine::PlatformPath::ResolveProjectRelative(path).wstring()))
	{
		m_StatusMessage = LOCAL("ResourceEditor.SaveFailed");
		return;
	}
	m_TargetResourcePath = path;
	MarkSaved();
	m_StatusMessage = LOCAL("ResourceEditor.SaveSuccess");
}

void VFXEditorPanel::SaveEffectAs()
{
	std::wstring savePath;
	if (FileDialogs::SaveFileDialog(savePath, { { L"Visual Effect (*.bamvfx.json)", L"*.bamvfx.json" } }, L"NewEffect.bamvfx.json", L"bamvfx.json", EFileDialogContext::General))
	{
		SaveEffectTo(std::filesystem::path(savePath));
	}
}

void VFXEditorPanel::DrawCustomContent()
{
	EditorUI::DrawEffectEditorLayout(m_LeftPaneWidth, m_RightPaneWidth, m_TimelineHeight,
		[this] { DrawTrackList(); }, [this] { DrawViewport(); },
		[this] { DrawInspector(); }, [this] { DrawTimeline(); },
		[this] { DrawTransportBar(); });

	// The modal callback records the edit only when Apply changes the curve.
	m_CurveModal.Draw();
}

void VFXEditorPanel::DrawCustomToolbar()
{
	const bool loading = IsAsyncLoading();
	const bool hasResource = !loading && (m_EffectHandle.Get() != nullptr);

	if (ImGui::BeginMenu(LOCAL_CSTR("EditorMenu.File")))
	{
		if (ImGui::MenuItem(LOCAL_LABEL("ParticleEditor.MenuNew").c_str(), nullptr, false, !loading)) RunWithDiscardConfirmation([this] { NewEffect(); });
		if (ImGui::MenuItem(LOCAL_LABEL("ParticleEditor.MenuOpen").c_str(), nullptr, false, !loading)) RunWithDiscardConfirmation([this] { OpenEffect(); });
		DrawSaveMenuItem();
		if (ImGui::MenuItem(LOCAL_LABEL("EditorCommon.SaveAs").c_str(), nullptr, false, hasResource)) SaveEffectAs();
		ImGui::Separator();
		DrawCloseMenuItem();
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu(LOCAL_CSTR("EditorMenu.Edit")))
	{
		DrawHistoryMenuItems("CompositeTimeline");
		ImGui::EndMenu();
	}
}

void VFXEditorPanel::DrawCustomQuickAccess()
{
	if (IsAsyncLoading()) return;
	const bool hasResource = m_EffectHandle.IsValid() && (m_EffectHandle.Get() != nullptr);
	auto* effect = m_EffectHandle.Get();

	EditorUI::ToolbarLayout toolbar;
	if (toolbar.Icon("New", ICON_FA_FILE, "ParticleEditor.MenuNew")) RunWithDiscardConfirmation([this] { NewEffect(); });
	if (toolbar.Icon("Open", ICON_FA_FOLDER_OPEN, "ParticleEditor.MenuOpen")) RunWithDiscardConfirmation([this] { OpenEffect(); });
	DrawSaveButton(toolbar);
	DrawHistoryButtons(toolbar, "CompositeTimeline");
	// 프리뷰 모션 토글 (트레일 / 이동 궤적 확인용)
	if (toolbar.Icon("PreviewMotion", ICON_FA_WIND, "VFXEditor.PreviewMotion", { .Active = m_PreviewMotion }))
	{
		m_PreviewMotion = !m_PreviewMotion;
		if (!m_PreviewMotion && m_PreviewEntity)
		{
			if (auto* scene = m_PreviewWidget.GetScene())
			{
				auto* trans = scene->GetRegistry().try_get<Engine::TransformComponent>(m_PreviewEntity->GetEntityHandle());
				if (trans) (void)trans->SetLocalPosition(Engine::vec3(0.0f, 0.5f, 0.0f));
			}
		}
	}

	// [지면 평면 온/오프 토글] (체스판 아이콘)
	if (toolbar.Icon("GroundPlane", ICON_FA_CHESS_BOARD, "VFXEditor.GroundPlane", { .Active = m_ShowGroundPlane }))
	{
		m_ShowGroundPlane = !m_ShowGroundPlane;
		if (m_PreviewGroundEntity)
		{
			if (auto* scene = m_PreviewWidget.GetScene())
			{
				auto* trans = scene->GetRegistry().try_get<Engine::TransformComponent>(m_PreviewGroundEntity->GetEntityHandle());
				if (trans)
				{
					const float s = m_ShowGroundPlane ? 48.0f : 0.0f;
					(void)trans->SetLocalScale(Engine::vec3(s, 1.0f, s));
				}
			}
		}
	}

	// [체커보드 바닥 패턴 토글] (디스토션 굴절 가시성 확인용)
	if (toolbar.Icon("CheckerBackdrop", ICON_FA_BORDER_ALL, "VFXEditor.CheckerBackdrop", { .Active = m_ShowCheckerBackdrop }))
	{
		m_ShowCheckerBackdrop = !m_ShowCheckerBackdrop;
		m_BackdropDirty = true;
	}

	// [공중(Air) 프리뷰 모드 토글] (비행기 아이콘: 지면/그리드 일괄 숨김 및 수평선 단차 제거)
	if (toolbar.Icon("AirMode", ICON_FA_PLANE, "VFXEditor.AirMode", { .Active = m_AirMode }))
	{
		ToggleAirMode();
	}

	// [3단계 배경 프리셋 순환 버튼] (Studio Gray -> Dark -> Bright)
	const char* bgIcon = ICON_FA_CIRCLE_HALF_STROKE;
	const char* bgTooltip = "VFXEditor.BackgroundStudio";
	if (m_BackgroundMode == EVFXBackgroundMode::Dark)
	{
		bgIcon = ICON_FA_MOON;
		bgTooltip = "VFXEditor.BackgroundDark";
	}
	else if (m_BackgroundMode == EVFXBackgroundMode::Bright)
	{
		bgIcon = ICON_FA_SUN;
		bgTooltip = "VFXEditor.BackgroundBright";
	}

	if (toolbar.Icon("BackgroundMode", bgIcon, bgTooltip))
	{
		m_BackgroundMode = static_cast<EVFXBackgroundMode>((static_cast<uint8>(m_BackgroundMode) + 1) % 3);
		ApplyBackgroundPreset();
	}

	// [환경 장애물 프리셋 드롭다운]
	const char* obsIcon = ICON_FA_CUBES;
	if (m_ObstaclePreset == EVFXObstaclePreset::CornerWall) obsIcon = ICON_FA_BORDER_ALL;
	else if (m_ObstaclePreset == EVFXObstaclePreset::Obstacles) obsIcon = ICON_FA_SHAPES;
	else if (m_ObstaclePreset == EVFXObstaclePreset::DummyCharacter) obsIcon = ICON_FA_USER;
	if (toolbar.Icon("ObstaclePreset", obsIcon, "VFXEditor.ObstaclePreset"))
		ImGui::OpenPopup("ObstaclePresetPopup");
	if (ImGui::BeginPopup("ObstaclePresetPopup"))
	{
		auto drawObsItem = [&](const char* label, EVFXObstaclePreset preset)
			{
				if (ImGui::MenuItem(label, nullptr, m_ObstaclePreset == preset))
				{
					m_ObstaclePreset = preset;
					SetupPreview();
				}
			};
		drawObsItem(LOCAL_CSTR("VFXEditor.PresetFloorOnly"), EVFXObstaclePreset::FloorOnly);
		drawObsItem(LOCAL_CSTR("VFXEditor.PresetCornerWall"), EVFXObstaclePreset::CornerWall);
		drawObsItem(LOCAL_CSTR("VFXEditor.PresetObstacles"), EVFXObstaclePreset::Obstacles);
		drawObsItem(LOCAL_CSTR("VFXEditor.PresetDummy"), EVFXObstaclePreset::DummyCharacter);
		ImGui::EndPopup();
	}
	// [실시간 환경 외력 시뮬레이터 팝업] (Wind & Gravity)
	const bool hasForce = (m_SimGravityMultiplier != 1.0f || glm::length2(m_SimWind) > 0.0001f);
	if (toolbar.Icon("SimForces", ICON_FA_VOLCANO, "VFXEditor.EnvironmentForces", { .Active = hasForce }))
		ImGui::OpenPopup("EnvironmentForcesPopup");
	if (ImGui::BeginPopup("EnvironmentForcesPopup"))
	{
		ImGui::TextUnformatted(LOCAL_CSTR("VFXEditor.EnvironmentForces"));
		ImGui::Separator();
		ImGui::SetNextItemWidth(140.0f);
		ImGui::SliderFloat(LOCAL_LABEL("VFXEditor.GravityMultiplier").c_str(), &m_SimGravityMultiplier, 0.0f, 3.0f, "%.2fx");
		ImGui::SetNextItemWidth(140.0f);
		ImGui::DragFloat2(LOCAL_LABEL("VFXEditor.WindXZ").c_str(), &m_SimWind.x, 0.1f, -20.0f, 20.0f, "%.1f m/s");
		if (ImGui::Button(LOCAL_LABEL("EditorCommon.Reset").c_str()))
		{
			m_SimGravityMultiplier = 1.0f;
			m_SimWind = Engine::vec3(0.0f);
		}
		ImGui::EndPopup();
	}
	if (!m_StatusMessage.empty()) toolbar.Text(m_StatusMessage.c_str());
}

void VFXEditorPanel::DrawAddTrackMenuItems(Engine::CompositeEffect& effect)
{
	// 트랙 추가를 Undo 가능한 명령으로 실행한다. 실행=목록 끝에 추가, 취소=끝에서 제거 (이력은 선형이라 취소 시점의 마지막 항목이 이 트랙이다)
	const auto addTrack = [this](ESelectedVFXTrackType type, auto track)
		{
			if (m_InspectorEdit) CommitInspectorEdit();
			using Track = decltype(track);
			const auto handle = m_EffectHandle;
			const auto apply = [this, handle, type, track](bool undo)
				{
					auto* target = handle.Get();
					if (!target) return false;
					bool changed = false;
					ForEachVFXTrackList(*target, [&](ESelectedVFXTrackType listType, auto& list)
						{
							if constexpr (std::is_same_v<typename std::remove_reference_t<decltype(list)>::value_type, Track>)
							{
								if (listType != type || (undo && list.empty())) return;
								if (undo) list.pop_back();
								else list.push_back(track);
								m_SelectedTrack = undo ? SelectedVFXTrack{} : SelectedVFXTrack{ type, static_cast<int>(list.size() - 1) };
								changed = true;
							}
						});
					if (changed)
					{
						m_Timeline.Drag.reset();
						InvalidateTimelinePreview();
					}
					return changed;
				};
			m_TimelineHistory.Execute(std::make_unique<CallbackEditorCommand>(LOCAL("VFXEditor.AddTrack"),
				[apply] { return apply(false); }, [apply] { return apply(true); }));
		};

	if (ImGui::MenuItem((std::string(ICON_FA_IMAGES " ") + LOCAL_CSTR("VFXEditor.AddSpriteTrack")).c_str()))
	{
		Engine::VFXSpriteTrack track;
		track.trackName = "Sprite_" + std::to_string(effect.GetSpriteTracks().size() + 1);
		addTrack(ESelectedVFXTrackType::Sprite, std::move(track));
	}
	if (ImGui::MenuItem((std::string(ICON_FA_WAND_MAGIC_SPARKLES " ") + LOCAL_CSTR("VFXEditor.AddParticleTrack")).c_str()))
	{
		Engine::VFXParticleTrack track;
		track.trackName = "Particle_" + std::to_string(effect.GetParticleTracks().size() + 1);
		addTrack(ESelectedVFXTrackType::Particle, std::move(track));
	}
	if (ImGui::MenuItem((std::string(ICON_FA_LIGHTBULB " ") + LOCAL_CSTR("VFXEditor.AddLightTrack")).c_str()))
	{
		Engine::VFXLightTrack track;
		track.trackName = "Light_" + std::to_string(effect.GetLightTracks().size() + 1);
		addTrack(ESelectedVFXTrackType::Light, std::move(track));
	}
	if (ImGui::MenuItem((std::string(ICON_FA_CUBE " ") + LOCAL_CSTR("VFXEditor.AddMeshTrack")).c_str()))
	{
		Engine::VFXMeshTrack track;
		track.trackName = "Mesh_" + std::to_string(effect.GetMeshTracks().size() + 1);
		auto additiveMat = Engine::ResourceManager::Get().GetResourceHandle<Engine::MaterialInterface>(L"Engine://Material/ParticleMaterial_Additive.bammat.json");
		if (additiveMat.IsValid()) track.materialHandle = additiveMat;
		addTrack(ESelectedVFXTrackType::Mesh, std::move(track));
	}
	if (ImGui::MenuItem((std::string(ICON_FA_MUSIC " ") + LOCAL_CSTR("VFXEditor.AddSoundTrack")).c_str()))
	{
		Engine::VFXSoundTrack track;
		track.trackName = "Sound_" + std::to_string(effect.GetSoundTracks().size() + 1);
		addTrack(ESelectedVFXTrackType::Sound, std::move(track));
	}
	if (ImGui::MenuItem((std::string(ICON_FA_LINES_LEANING " ") + LOCAL_CSTR("VFXEditor.AddTrailTrack")).c_str()))
	{
		Engine::VFXTrailTrack track;
		track.trackName = "Trail_" + std::to_string(effect.GetTrailTracks().size() + 1);
		addTrack(ESelectedVFXTrackType::Trail, std::move(track));
	}
	if (ImGui::MenuItem((std::string(ICON_FA_STAMP " ") + LOCAL_CSTR("VFXEditor.AddDecalTrack")).c_str()))
	{
		Engine::VFXDecalTrack track;
		track.trackName = "Decal_" + std::to_string(effect.GetDecalTracks().size() + 1);
		addTrack(ESelectedVFXTrackType::Decal, std::move(track));
	}
	if (ImGui::MenuItem((std::string(ICON_FA_EARTH_AMERICAS " ") + LOCAL_CSTR("VFXEditor.AddCameraShakeTrack")).c_str()))
	{
		Engine::VFXCameraShakeTrack track;
		track.trackName = "Shake_" + std::to_string(effect.GetCameraShakeTracks().size() + 1);
		addTrack(ESelectedVFXTrackType::CameraShake, std::move(track));
	}
}

void VFXEditorPanel::DrawTrackList()
{
	EditorUI::ToolbarLayout toolbar;
	auto* effect = m_EffectHandle.Get();
	if (!effect)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", LOCAL_CSTR("VFXEditor.NoEffectLoaded"));
		ImGui::Spacing();
		if (toolbar.Button("VFXEditor.CreateNewEffect", ICON_FA_FILE, "VFXEditor.CreateNewEffect"))
		{
			NewEffect();
		}
		if (toolbar.Button("ParticleEditor.MenuOpen", ICON_FA_FOLDER_OPEN, "ParticleEditor.MenuOpen"))
		{
			OpenEffect();
		}
		return;
	}

	ImGuiTextBuffer trackLabel;
	trackLabel.appendf("%s %s (%u)", ICON_FA_LAYER_GROUP, LOCAL_CSTR("VFXEditor.Tracks"), effect->GetTotalTrackCount());
	toolbar.Text(trackLabel.c_str(), false);
	if (toolbar.Button("AddTrack", ICON_FA_PLUS, "VFXEditor.AddTrack"))
	{
		ImGui::OpenPopup("AddVFXTrackPopup");
	}

	if (ImGui::BeginPopup("AddVFXTrackPopup"))
	{
		DrawAddTrackMenuItems(*effect);
		ImGui::EndPopup();
	}

	// 트랙 창 빈 공간 또는 항목 위에서 우클릭 시 컨텍스트 메뉴 오픈
	if (ImGui::BeginPopupContextWindow("VFXTrackListContextMenu", ImGuiPopupFlags_MouseButtonRight))
	{
		DrawAddTrackMenuItems(*effect);
		if (m_SelectedTrack.IsValid())
		{
			ImGui::Separator();
			if (ImGui::MenuItem((std::string(ICON_FA_TRASH " ") + LOCAL_CSTR("VFXEditor.DeleteTrack")).c_str()))
				DeleteSelectedTrack();
		}
		ImGui::EndPopup();
	}

	ImGui::Separator();

	// 표준 검색 바 (EditorUI::DrawSearchBar) 도입
	EditorUI::DrawSearchBar("VFXTrackSearch", m_TrackSearch, sizeof(m_TrackSearch), "VFXEditor.SearchTracks");
	ImGui::Spacing();

	if (effect->GetTotalTrackCount() == 0)
	{
		// (기존 NoTracks 안내 유지)
		return;
	}

	const std::string filterLower = Engine::ToLowerStr(m_TrackSearch);

	ForEachVFXTrackList(*effect, [&](ESelectedVFXTrackType type, auto& tracks)
		{
			const auto& style = g_VFXTrackPresentation[static_cast<int>(type)];
			ImGui::PushID(static_cast<int>(type));
			for (std::size_t i = 0; i < tracks.size(); ++i)
			{
				auto& track = tracks[i];

				// 트랙 이름 검색 필터링
				if (!filterLower.empty() && Engine::ToLowerStr(track.trackName).find(filterLower) == std::string::npos)
					continue;

				ImGui::PushID(static_cast<int>(i));
				const bool previous = track.enabled;
				bool enabled = previous;
				if (ImGui::Checkbox("##Enabled", &enabled))
				{
					if (m_InspectorEdit) CommitInspectorEdit();
					const SelectedVFXTrack selection{ type, static_cast<int>(i) };
					const auto apply = [this, handle = m_EffectHandle, selection](bool value) {
						auto* target = handle.Get();
						if (!target || !VisitVFXTrack(*target, selection,
							[value](auto& list, std::size_t index) { list[index].enabled = value; }))
							return false;
						InvalidateTimelinePreview();
						return true;
					};
					m_TimelineHistory.Execute(std::make_unique<CallbackEditorCommand>(LOCAL("VFXEditor.PropEnabled"),
						[apply, enabled] { return apply(enabled); }, [apply, previous] { return apply(previous); }));
				}
				ImGui::SameLine();
				ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(style.Color));
				const auto label = std::string(style.Icon) + " " + track.trackName + "###Track";
				const bool selected = m_SelectedTrack.type == type && m_SelectedTrack.index == static_cast<int>(i);
				if (ImGui::Selectable(label.c_str(), selected)) SelectTrack({ type, static_cast<int>(i) });
				if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) SelectTrack({ type, static_cast<int>(i) });
				ImGui::PopStyleColor();
				ImGui::PopID();
			}
			ImGui::PopID();
		});
}

void VFXEditorPanel::DrawViewport()
{
	m_PreviewWidget.Draw();

	const ImVec2 viewportPos = m_PreviewWidget.GetViewportScreenPos();
	const ImVec2 viewportSize = m_PreviewWidget.GetViewportSize();

	// 1. 3D 기즈모 렌더링 및 인터랙션 평가
	DrawGizmo();

	// 2. 블렌더 스타일 세로 도구 툴바 플로팅 오버레이 렌더링
	GizmoHelper::DrawGizmoToolbar(this, viewportPos, viewportSize, true);

	// 3. 뷰포트 우상단 정보 & 통계 HUD 오버레이
	DrawViewportHUD(viewportPos, viewportSize);
}

void VFXEditorPanel::DrawViewportHUD(const ImVec2& viewportPos, const ImVec2& viewportSize)
{
	if (viewportSize.x < 180.0f || viewportSize.y < 100.0f) return;

	auto* effect = m_EffectHandle.Get();
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	if (!drawList) return;

	constexpr float margin = 10.0f;
	constexpr float padX = 10.0f;
	constexpr float padY = 7.0f;
	constexpr float hudWidth = 165.0f;
	const bool hasForces = effect && (glm::length(m_SimWind) > 0.01f || std::abs(m_SimGravityMultiplier - 1.0f) > 0.01f);
	const float lineHeight = ImGui::GetFontSize() + 3.0f;
	const float hudHeight = padY * 2.0f + lineHeight * (hasForces ? 4.0f : 3.0f);

	// 뷰포트 우측 하단에 배치 및 클램핑
	const float minX = viewportPos.x + margin;
	const float maxX = std::max(minX, viewportPos.x + viewportSize.x - hudWidth - margin);
	const float minY = viewportPos.y + margin;
	const float maxY = std::max(minY, viewportPos.y + viewportSize.y - hudHeight - margin);

	const ImVec2 hudMin(maxX, maxY);
	const ImVec2 hudMax(maxX + hudWidth, maxY + hudHeight);

	// 1. 반투명 다크 배경 및 테두리 (ImDrawList 직접 렌더링으로 스크롤바 유발 방지)
	drawList->AddRectFilled(hudMin, hudMax, IM_COL32(18, 18, 22, 220), 6.0f);
	drawList->AddRect(hudMin, hudMax, IM_COL32(70, 75, 88, 180), 6.0f, 0, 1.0f);

	float textY = hudMin.y + padY;
	const float textX = hudMin.x + padX;

	const float curTime = m_TimelineSeek.value_or(m_CurrentTime);
	const float maxTime = effect ? GetTimelineDuration() : 0.0f;
	const int curFrame = static_cast<int>(std::round(curTime * EditorUI::EffectTimelineFPS));
	const int totalFrames = static_cast<int>(std::round(maxTime * EditorUI::EffectTimelineFPS));

	// 1행: 시간 (청록색)
	char timeBuf[64];
	std::snprintf(timeBuf, sizeof(timeBuf), ICON_FA_CLOCK "  %.2fs / %.2fs", curTime, maxTime);
	drawList->AddText(ImVec2(textX, textY), IM_COL32(80, 230, 210, 255), timeBuf);
	textY += lineHeight;

	// 2행: (프레임) (연회색)
	char frameBuf[64];
	std::snprintf(frameBuf, sizeof(frameBuf), "  (%s %d / %d)", LOCAL_CSTR("VFXEditor.HUDFrame"), curFrame, totalFrames);
	drawList->AddText(ImVec2(textX, textY), IM_COL32(170, 175, 185, 220), frameBuf);
	textY += lineHeight;

	// 3행: 재생 상태 (연두색 / 황색)
	if (!effect)
	{
		std::string s = std::string(ICON_FA_CIRCLE_INFO " ") + LOCAL_CSTR("VFXEditor.NoEffectLoaded");
		drawList->AddText(ImVec2(textX, textY), IM_COL32(170, 170, 170, 200), s.c_str());
	}
	else if (m_IsPlaying)
	{
		char playBuf[64];
		std::snprintf(playBuf, sizeof(playBuf), ICON_FA_PLAY " %s (%.1fx)", LOCAL_CSTR("VFXEditor.HUDPlaying"), m_PlayRate);
		drawList->AddText(ImVec2(textX, textY), IM_COL32(100, 230, 100, 255), playBuf);
	}
	else
	{
		std::string pauseStr = std::string(ICON_FA_PAUSE " ") + LOCAL_CSTR("VFXEditor.HUDPaused");
		drawList->AddText(ImVec2(textX, textY), IM_COL32(240, 190, 50, 255), pauseStr.c_str());
	}
	textY += lineHeight;

	// 4행: 환경 외력 (바람 또는 중력이 설정되어 있을 때만 컴팩트 노출)
	if (hasForces)
	{
		char forceBuf[80];
		std::snprintf(forceBuf, sizeof(forceBuf), "%s:(%.1f,%.1f) %s:%.1fx",
			LOCAL_CSTR("VFXEditor.HUDWind"), m_SimWind.x, m_SimWind.z,
			LOCAL_CSTR("VFXEditor.HUDGravity"), m_SimGravityMultiplier);
		drawList->AddText(ImVec2(textX, textY), IM_COL32(150, 160, 175, 220), forceBuf);
	}
}

void VFXEditorPanel::DrawGizmo()
{
	auto* effect = m_EffectHandle.Get();
	if (!effect || !m_SelectedTrack.IsValid()) return;

	auto* camCtrl = m_PreviewWidget.GetCameraController();
	if (!camCtrl) return;
	const auto cameraBuffer = camCtrl->GetCameraBuffer();
	if (!cameraBuffer) return;

	// 뷰포트 정확한 화면 좌표와 크기 사용
	const ImVec2 viewportPos = m_PreviewWidget.GetViewportScreenPos();
	const ImVec2 viewportSize = m_PreviewWidget.GetViewportSize();
	if (viewportSize.x <= 0.0f || viewportSize.y <= 0.0f) return;

	// 공통 기즈모 헬퍼 뷰포트 바인딩 및 와이어프레임 헬퍼 렌더링
	GizmoHelper::BeginViewport(*cameraBuffer, viewportPos, viewportSize, camCtrl->IsOrthographic());
	DrawVisualHelpers(cameraBuffer->viewProjMatrix, viewportPos, viewportSize);

	Engine::mat4 entityWorld(1.0f);
	if (m_PreviewEntity)
	{
		if (auto* scene = m_PreviewWidget.GetScene())
		{
			if (auto* trans = scene->GetRegistry().try_get<Engine::TransformComponent>(m_PreviewEntity->GetEntityHandle()))
				entityWorld = trans->GetLocalMatrix();
		}
	}

	VisitVFXTrack(*effect, m_SelectedTrack, [&](auto& tracks, std::size_t index)
		{
			auto& track = tracks[index];
			using TrackType = std::remove_reference_t<decltype(track)>;

			Engine::vec3 currentPos(0.0f);
			Engine::vec3 currentRot(0.0f);
			Engine::vec3 currentScale(1.0f);
			Engine::vec2 currentDecalSize(1.0f);
			Engine::vec3 currentOffsetA(0.0f);
			Engine::vec3 currentOffsetB(0.0f);
			bool canRotate = false;
			bool canScale = false;

			if constexpr (std::is_same_v<TrackType, Engine::VFXSpriteTrack> || std::is_same_v<TrackType, Engine::VFXParticleTrack>)
			{
				currentPos = track.localPosition;
				currentRot = track.localRotation;
				currentScale = track.localScale;
				canRotate = true;
				canScale = true;
			}
			else if constexpr (std::is_same_v<TrackType, Engine::VFXLightTrack>) currentPos = track.localPosition;
			else if constexpr (std::is_same_v<TrackType, Engine::VFXMeshTrack>)
			{
				currentPos = track.localPosition;
				currentRot = track.initialRotation;
				currentScale = track.scaleStart;
				canRotate = true;
				canScale = true;
			}
			else if constexpr (std::is_same_v<TrackType, Engine::VFXDecalTrack>)
			{
				currentPos = track.localPosition;
				currentRot = track.localRotation;
				currentScale = Engine::vec3(track.size.x, 1.0f, track.size.y);
				currentDecalSize = track.size;
				canRotate = true;
				canScale = true;
			}
			else if constexpr (std::is_same_v<TrackType, Engine::VFXTrailTrack>)
			{
				currentPos = (track.localOffsetA + track.localOffsetB) * 0.5f;
				currentOffsetA = track.localOffsetA;
				currentOffsetB = track.localOffsetB;
			}
			else return;

			ImGuizmo::OPERATION op = m_GizmoOperation;
			if (op == ImGuizmo::ROTATE && !canRotate) op = ImGuizmo::TRANSLATE;
			if (op == ImGuizmo::SCALE && !canScale) op = ImGuizmo::TRANSLATE;

			Engine::mat4 localMat = glm::translate(Engine::mat4(1.0f), currentPos);
			if (canRotate && (currentRot.x != 0.0f || currentRot.y != 0.0f || currentRot.z != 0.0f))
				localMat = localMat * glm::mat4_cast(glm::quat(glm::radians(currentRot)));
			if (canScale) localMat = glm::scale(localMat, currentScale);

			Engine::mat4 gizmoWorld = entityWorld * localMat;

			GizmoParams params;
			params.Operation = op;
			params.Mode = m_GizmoMode;
			params.UseSnap = m_GizmoUseSnap;
			params.ParentWorldMatrix = entityWorld;

			const GizmoResult res = GizmoHelper::Manipulate("VFXTrackGizmo", gizmoWorld, params, m_IsGizmoUsing);

			if (res.DragStarted)
			{
				m_GizmoSnapshot.track = m_SelectedTrack;
				m_GizmoSnapshot.position = currentPos;
				m_GizmoSnapshot.rotation = currentRot;
				m_GizmoSnapshot.scale = currentScale;
				m_GizmoSnapshot.decalSize = currentDecalSize;
				m_GizmoSnapshot.trailOffsetA = currentOffsetA;
				m_GizmoSnapshot.trailOffsetB = currentOffsetB;
				m_GizmoSnapshot.valid = true;
			}

			if (res.Changed)
			{
				if constexpr (std::is_same_v<TrackType, Engine::VFXSpriteTrack> || std::is_same_v<TrackType, Engine::VFXParticleTrack>)
				{
					track.localPosition = res.Position;
					track.localRotation = res.Rotation;
					track.localScale = res.Scale;
				}
				else if constexpr (std::is_same_v<TrackType, Engine::VFXLightTrack>) track.localPosition = res.Position;
				else if constexpr (std::is_same_v<TrackType, Engine::VFXMeshTrack>)
				{
					track.localPosition = res.Position;
					track.initialRotation = res.Rotation;
					if (currentScale.x > 0.0001f && currentScale.y > 0.0001f && currentScale.z > 0.0001f)
						track.scaleEnd *= (res.Scale / currentScale);
					track.scaleStart = res.Scale;
				}
				else if constexpr (std::is_same_v<TrackType, Engine::VFXDecalTrack>)
				{
					track.localPosition = res.Position;
					track.localRotation = res.Rotation;
					track.size = Engine::vec2(std::max(res.Scale.x, 0.05f), std::max(res.Scale.z, 0.05f));
				}
				else if constexpr (std::is_same_v<TrackType, Engine::VFXTrailTrack>)
				{
					const Engine::vec3 delta = res.Position - currentPos;
					track.localOffsetA += delta;
					track.localOffsetB += delta;
				}
				InvalidateTimelinePreview();
			}

			if (res.DragEnded && m_GizmoSnapshot.valid &&
				m_GizmoSnapshot.track.type == m_SelectedTrack.type &&
				m_GizmoSnapshot.track.index == m_SelectedTrack.index)
			{
				const auto snapshot = m_GizmoSnapshot;
				VFXGizmoSnapshot finalState;
				finalState.track = m_SelectedTrack;
				finalState.position = currentPos;
				finalState.rotation = currentRot;
				finalState.scale = currentScale;
				finalState.decalSize = currentDecalSize;
				finalState.trailOffsetA = currentOffsetA;
				finalState.trailOffsetB = currentOffsetB;

				const bool wasModified = (snapshot.position != finalState.position) ||
					(snapshot.rotation != finalState.rotation) ||
					(snapshot.scale != finalState.scale) ||
					(snapshot.decalSize != finalState.decalSize) ||
					(snapshot.trailOffsetA != finalState.trailOffsetA) ||
					(snapshot.trailOffsetB != finalState.trailOffsetB);

				if (wasModified)
				{
					const auto handle = m_EffectHandle;
					const auto selected = m_SelectedTrack;
					auto applyState = [this, handle, selected](const VFXGizmoSnapshot& state) -> bool
						{
							auto* target = handle.Get();
							if (!target) return false;
							return VisitVFXTrack(*target, selected, [&](auto& list, std::size_t i)
								{
									auto& item = list[i];
									using T = std::remove_reference_t<decltype(item)>;
									if constexpr (std::is_same_v<T, Engine::VFXSpriteTrack> || std::is_same_v<T, Engine::VFXParticleTrack>)
									{
										item.localPosition = state.position;
										item.localRotation = state.rotation;
										item.localScale = state.scale;
									}
									else if constexpr (std::is_same_v<T, Engine::VFXLightTrack>) item.localPosition = state.position;
									else if constexpr (std::is_same_v<T, Engine::VFXMeshTrack>)
									{
										item.localPosition = state.position;
										item.initialRotation = state.rotation;
										item.scaleStart = state.scale;
									}
									else if constexpr (std::is_same_v<T, Engine::VFXDecalTrack>)
									{
										item.localPosition = state.position;
										item.localRotation = state.rotation;
										item.size = state.decalSize;
									}
									else if constexpr (std::is_same_v<T, Engine::VFXTrailTrack>)
									{
										item.localOffsetA = state.trailOffsetA;
										item.localOffsetB = state.trailOffsetB;
									}
									InvalidateTimelinePreview();
								});
						};

					m_TimelineHistory.Record(std::make_unique<CallbackEditorCommand>(
						LOCAL("Action.Viewport.GizmoTranslate"),
						[applyState, finalState] { return applyState(finalState); },
						[applyState, snapshot] { return applyState(snapshot); }));
				}
				m_GizmoSnapshot.valid = false;
			}
		});
}

void VFXEditorPanel::DrawVisualHelpers(const Engine::mat4& viewProj, const ImVec2& imageScreenPos, const ImVec2& imageSize)
{
	auto* effect = m_EffectHandle.Get();
	if (!effect || !m_SelectedTrack.IsValid()) return;

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	if (!drawList) return;

	drawList->PushClipRect(imageScreenPos, ImVec2(imageScreenPos.x + imageSize.x, imageScreenPos.y + imageSize.y), true);

	Engine::mat4 entityWorld(1.0f);
	if (m_PreviewEntity)
	{
		if (auto* scene = m_PreviewWidget.GetScene())
		{
			if (auto* trans = scene->GetRegistry().try_get<Engine::TransformComponent>(m_PreviewEntity->GetEntityHandle()))
				entityWorld = trans->GetLocalMatrix();
		}
	}

	VisitVFXTrack(*effect, m_SelectedTrack, [&](const auto& tracks, std::size_t index)
		{
			const auto& track = tracks[index];
			using TrackType = std::decay_t<decltype(track)>;

			if constexpr (std::is_same_v<TrackType, Engine::VFXLightTrack>)
			{
				const Engine::vec3 worldPos = Engine::vec3(entityWorld * Engine::vec4(track.localPosition, 1.0f));
				const ImU32 color = IM_COL32(
					static_cast<int>(std::clamp(track.lightColor.r, 0.0f, 1.0f) * 255.0f),
					static_cast<int>(std::clamp(track.lightColor.g, 0.0f, 1.0f) * 255.0f),
					static_cast<int>(std::clamp(track.lightColor.b, 0.0f, 1.0f) * 255.0f), 160);

				GizmoHelper::DrawWireSphere(drawList, worldPos, track.radius, viewProj, imageScreenPos, imageSize, color);
			}
			else if constexpr (std::is_same_v<TrackType, Engine::VFXTrailTrack>)
			{
				const Engine::vec3 worldA = Engine::vec3(entityWorld * Engine::vec4(track.localOffsetA, 1.0f));
				const Engine::vec3 worldB = Engine::vec3(entityWorld * Engine::vec4(track.localOffsetB, 1.0f));
				ImVec2 screenA{}, screenB{};
				if (GizmoHelper::WorldToScreen(worldA, viewProj, imageScreenPos, imageSize, screenA) &&
					GizmoHelper::WorldToScreen(worldB, viewProj, imageScreenPos, imageSize, screenB))
				{
					drawList->AddLine(screenA, screenB, IM_COL32(50, 220, 190, 255), 2.5f);
					drawList->AddCircleFilled(screenA, 4.0f, IM_COL32(255, 100, 100, 255));
					drawList->AddCircleFilled(screenB, 4.0f, IM_COL32(100, 100, 255, 255));
				}
			}
			else if constexpr (std::is_same_v<TrackType, Engine::VFXDecalTrack>)
			{
				// CompositeVFXSystem::CollectDecals와 같은 투영 상자
				const Engine::mat4 local = glm::translate(Engine::mat4(1.0f), track.localPosition) * glm::mat4_cast(glm::quat(glm::radians(track.localRotation)));
				const Engine::mat4 box = Engine::MakeDecalBoxMatrix(entityWorld * local, Engine::vec3(track.size.x, track.size.y, track.projectionDepth));
				GizmoHelper::DrawWireBox(drawList, Engine::vec3(box[3]), Engine::vec3(0.5f), box, viewProj, imageScreenPos, imageSize, IM_COL32(170, 90, 190, 200));
			}
			else if constexpr (std::is_same_v<TrackType, Engine::VFXParticleTrack>)
			{
				const Engine::vec3 worldPos = Engine::vec3(entityWorld * Engine::vec4(track.localPosition, 1.0f));
				Engine::vec3 dir(0.0f, 1.0f, 0.0f);
				float spreadAngle = 30.0f;
				float speed = 2.0f;

				if (auto effectHandle = track.effectHandle; effectHandle.IsValid() && effectHandle.Get())
				{
					const auto& emitters = effectHandle.Get()->GetEmitters();
					if (!emitters.empty())
					{
						dir = emitters[0].direction;
						spreadAngle = emitters[0].spreadAngleDegrees;
						speed = emitters[0].speedMax;
					}
				}

				const Engine::mat4 rotMat = glm::mat4_cast(glm::quat(glm::radians(track.localRotation)));
				const Engine::vec3 worldDir = Engine::vec3(entityWorld * rotMat * Engine::vec4(dir, 0.0f));
				const float length = std::max(0.6f, speed * 0.35f);

				GizmoHelper::DrawWireCone(drawList, worldPos, worldDir, length, spreadAngle,
					viewProj, imageScreenPos, imageSize, IM_COL32(255, 175, 40, 200));
			}
		});

	drawList->PopClipRect();
}

void VFXEditorPanel::StepFrame(int step)
{
	m_IsPlaying = false;
	const float dt = 1.0f / EditorUI::EffectTimelineFPS;
	const float time = m_TimelineSeek.value_or(m_CurrentTime);
	ScrubTime(std::clamp(time + static_cast<float>(step) * dt, 0.0f, GetTimelineDuration()));
}

void VFXEditorPanel::SelectTrack(SelectedVFXTrack selection)
{
	auto* effect = m_EffectHandle.Get();
	if (!effect) return;
	VisitVFXTrack(*effect, selection, [&](auto& tracks, std::size_t index)
	{
		m_SelectedTrack = selection;
		const auto& track = tracks[index];
		if constexpr (requires { track.localPosition; })
			m_PreviewWidget.FocusTarget(track.localPosition);
		else if constexpr (requires { track.localOffsetA; track.localOffsetB; })
			m_PreviewWidget.FocusTarget((track.localOffsetA + track.localOffsetB) * 0.5f);
		else
			m_PreviewWidget.FocusTarget(Engine::vec3(0.0f, 0.5f, 0.0f));

		if constexpr (requires { track.duration; })
		{
			const float time = m_TimelineSeek.value_or(m_CurrentTime);
			const float end = VFXTrackEnd(track);
			if (time < track.startTime || time >= end)
				ScrubTime(std::min(end, track.startTime + 1.0f / EditorUI::EffectTimelineFPS));
		}
	});
}

std::function<void()> VFXEditorPanel::CaptureCurveHistory()
{
	CommitInspectorEdit();
	auto* effect = m_EffectHandle.Get();
	if (!effect) return {};
	const auto selection = m_SelectedTrack;
	std::any before;
	VisitVFXTrack(*effect, selection, [&](auto& tracks, std::size_t index) { before = tracks[index]; });
	if (!before.has_value()) return {};

	return [this, effect, selection, before = std::move(before)]() mutable {
		if (m_EffectHandle.Get() != effect) return;
		m_InspectorEdit = VFXInspectorEdit{ effect, selection, std::move(before) };
		CommitInspectorEdit();
	};
}

void VFXEditorPanel::CommitInspectorEdit()
{
	if (!m_InspectorEdit) return;
	VFXInspectorEdit edit = std::move(*m_InspectorEdit);
	m_InspectorEdit.reset();

	// 편집 중 다른 이펙트를 열었으면 기록하지 않는다.
	auto* effect = m_EffectHandle.Get();
	if (!effect || effect != edit.effect) return;

	std::any after;
	VisitVFXTrack(*effect, edit.track, [&](auto& tracks, std::size_t index)
		{
			using T = std::remove_cvref_t<decltype(tracks[index])>;
			if (std::any_cast<T>(&edit.before)) after = tracks[index];
		});
	if (!after.has_value()) return;

	const auto handle = m_EffectHandle;
	const auto selection = edit.track;
	auto applyState = [this, handle, selection](const std::any& state) -> bool
		{
			auto* target = handle.Get();
			if (!target) return false;
			bool applied = false;
			VisitVFXTrack(*target, selection, [&](auto& tracks, std::size_t index)
				{
					using T = std::remove_cvref_t<decltype(tracks[index])>;
					if (const T* value = std::any_cast<T>(&state)) { tracks[index] = *value; applied = true; }
				});
			if (applied) InvalidateTimelinePreview(); // 프리뷰 재구성 (미저장 여부는 이력 상태 ID로 판단)
			return applied;
		};

	m_TimelineHistory.Record(std::make_unique<CallbackEditorCommand>(LOCAL("VFXEditor.EditTrackProperties"),
		[applyState, after] { return applyState(after); },
		[applyState, before = std::move(edit.before)] { return applyState(before); }));
}

void VFXEditorPanel::DeleteSelectedTrack()
{
	if (m_InspectorEdit) CommitInspectorEdit();
	auto* effect = m_EffectHandle.Get();
	if (!effect) return;
	const auto selected = m_SelectedTrack;
	VisitVFXTrack(*effect, selected, [&](auto& tracks, std::size_t index)
	{
		using Track = typename std::remove_reference_t<decltype(tracks)>::value_type;
		const auto removed = tracks[index];
		const auto handle = m_EffectHandle;
		const auto apply = [this, handle, selected, removed](bool undo)
		{
			auto* target = handle.Get();
			if (!target) return false;
			bool changed = false;
			ForEachVFXTrackList(*target, [&](ESelectedVFXTrackType type, auto& list)
			{
				if constexpr (std::is_same_v<typename std::remove_reference_t<decltype(list)>::value_type, Track>)
				{
					const auto i = static_cast<std::size_t>(selected.index);
					if (type != selected.type || i > list.size() || (!undo && i == list.size())) return;
					if (undo) list.insert(list.begin() + i, removed);
					else list.erase(list.begin() + i);
					changed = true;
				}
			});
			if (changed)
			{
				m_SelectedTrack = undo ? selected : SelectedVFXTrack{};
				m_Timeline.Drag.reset();
				InvalidateTimelinePreview();
			}
			return changed;
		};
		m_TimelineHistory.Execute(std::make_unique<CallbackEditorCommand>(LOCAL("Action.Timeline.DeleteSelected"),
			[apply] { return apply(false); }, [apply] { return apply(true); }));
	});
}

void VFXEditorPanel::ApplyTimelineEdit(const EditorUI::TimelineEdit& edit)
{
	auto* effect = m_EffectHandle.Get();
	if (!effect) return;
	const auto handle = m_EffectHandle;
	const float oldDuration = GetTimelineDuration();
	if (edit.Mode == EditorUI::TimelineDragMode::Duration)
	{
		const auto apply = [this, handle](float duration)
		{
			auto* target = handle.Get();
			if (!target) return false;
			target->SetTotalDuration(duration);
			InvalidateTimelinePreview();
			return true;
		};
		m_TimelineHistory.Execute(std::make_unique<CallbackEditorCommand>(LOCAL("Timeline.Duration"),
			[apply, edit] { return apply(edit.End); }, [apply, oldDuration] { return apply(oldDuration); }));
		return;
	}

	const SelectedVFXTrack selection{ static_cast<ESelectedVFXTrackType>(edit.Row >> 32), static_cast<int>(edit.Row & 0xffffffffu) };
	VisitVFXTrack(*effect, selection, [&](auto& tracks, std::size_t index)
	{
		const auto& track = tracks[index];
		const float oldStart = track.startTime;
		float oldLength = 0.0f;
		if constexpr (requires { track.duration; }) oldLength = track.duration;
		const float length = edit.Mode == EditorUI::TimelineDragMode::Move ? oldLength : edit.End - edit.Start;
		const float total = std::max(oldDuration, edit.End + (length == 0.0f ? EditorUI::MinimumTimelineDuration : 0.0f));
		const auto apply = [this, handle, selection](float start, float duration, float totalDuration)
		{
			auto* target = handle.Get();
			if (!target) return false;
			if (!VisitVFXTrack(*target, selection, [&](auto& list, std::size_t i)
			{
				list[i].startTime = start;
				if constexpr (requires { list[i].duration; }) list[i].duration = duration;
			})) return false;
			target->SetTotalDuration(totalDuration);
			InvalidateTimelinePreview();
			return true;
		};
		m_TimelineHistory.Execute(std::make_unique<CallbackEditorCommand>(LOCAL("EffectTimeline.EditRange"),
			[apply, edit, length, total] { return apply(edit.Start, length, total); },
			[apply, oldStart, oldLength, oldDuration] { return apply(oldStart, oldLength, oldDuration); }));
	});
}

void VFXEditorPanel::DrawTransportBar()
{
	const bool hasResource = m_EffectHandle.IsValid() && (m_EffectHandle.Get() != nullptr);
	auto* effect = m_EffectHandle.Get();

	EditorUI::ToolbarLayout toolbar;

	// 1. 재생 컨트롤 버튼들 (|<<, <, Play/Pause, >, >>|, Loop)
	EditorUI::DrawTimelineAction(toolbar, "CompositeTimeline", "FirstFrame", ICON_FA_BACKWARD_FAST, "Action.Timeline.FirstFrame");
	EditorUI::DrawTimelineAction(toolbar, "CompositeTimeline", "PrevFrame", ICON_FA_BACKWARD_STEP, "Action.Timeline.PrevFrame");
	EditorUI::DrawTimelineAction(toolbar, "CompositeTimeline", "PlayPause", m_IsPlaying ? ICON_FA_PAUSE : ICON_FA_PLAY, "Action.Timeline.PlayPause");
	EditorUI::DrawTimelineAction(toolbar, "CompositeTimeline", "NextFrame", ICON_FA_FORWARD_STEP, "Action.Timeline.NextFrame");
	EditorUI::DrawTimelineAction(toolbar, "CompositeTimeline", "LastFrame", ICON_FA_FORWARD_FAST, "Action.Timeline.LastFrame");
	EditorUI::DrawTimelineAction(toolbar, "CompositeTimeline", "ToggleLoop", ICON_FA_REPEAT, "Action.Timeline.ToggleLoop", m_IsLooping);

	// 2. 현재 시간 및 프레임 카운터 (초록색 시간 + 프레임)
	if (effect)
	{
		constexpr float fps = 30.0f;
		const float curTime = m_TimelineSeek.value_or(m_CurrentTime);
		const float maxTime = GetTimelineDuration();
		const int curFrame = static_cast<int>(curTime * fps);
		const int totalFrames = static_cast<int>(maxTime * fps);

		const std::string timeLabel = LOCAL_FMT("Timeline.TimeCounter", curTime, maxTime);
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.9f, 0.5f, 1.0f));
		toolbar.Text(timeLabel.c_str(), false);
		ImGui::PopStyleColor();

		const std::string frameLabel = LOCAL_FMT("Timeline.FrameCounter", curFrame, totalFrames);
		toolbar.Text(frameLabel.c_str(), false);
	}

	// 3. 재생 속도 슬라이더
	ImGui::BeginDisabled(!hasResource);
	toolbar.SetNextItemWidth(ImGui::GetFontSize() * 4.5f);
	if (ImGui::DragFloat("##TransportPlayRate", &m_PlayRate, 0.05f, 0.1f, 5.0f, "%.1fx"))
		m_PlayRate = std::clamp(m_PlayRate, 0.1f, 5.0f);
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		ImGui::SetTooltip("%s", LOCAL_CSTR("VFXEditor.PropPlaySpeed"));
	ImGui::EndDisabled();

	// 4. 줌 배율 표시 및 줌 리셋
	const std::string zoomLabel = LOCAL_FMT("Timeline.ZoomFactor", m_Timeline.Zoom);
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.4f, 1.0f));
	toolbar.Text(zoomLabel.c_str(), false);
	ImGui::PopStyleColor();

	EditorUI::DrawTimelineAction(toolbar, "CompositeTimeline", "ZoomReset", ICON_FA_MAXIMIZE, "Action.Timeline.ZoomReset");

	// 5. 트랙 기반 길이 자동 계산 버튼 (우측 정렬 배치)
	if (effect)
	{
		if (toolbar.Button("AutoDuration", ICON_FA_ARROWS_ROTATE, "VFXEditor.AutoDuration"))
		{
			const float previous = effect->GetTotalDuration();
			effect->RecalculateDurationFromTracks();
			const float next = effect->GetTotalDuration();
			effect->SetTotalDuration(previous);
			if (next != previous)
				ApplyTimelineEdit({ EditorUI::TimelineDragMode::Duration, 0, 0.0f, next });
		}
	}
}

void VFXEditorPanel::DrawTimeline()
{
	auto* effect = m_EffectHandle.Get();
	if (!effect)
	{
		EditorUI::EmptyState("VFXEditor.TimelineUnavailable");
		return;
	}
	std::vector<EditorUI::TimelineRow> rows;
	rows.reserve(effect->GetTotalTrackCount());
	float minimumDuration = EditorUI::MinimumTimelineDuration;
	ForEachVFXTrackList(*effect, [&](ESelectedVFXTrackType type, const auto& tracks)
	{
		const auto& style = g_VFXTrackPresentation[static_cast<int>(type)];
		for (std::size_t i = 0; i < tracks.size(); ++i)
		{
			const auto& track = tracks[i];
			const float end = VFXTrackEnd(track);
			if (!std::isfinite(track.startTime) || !std::isfinite(end)) continue;
			const bool marker = type == ESelectedVFXTrackType::Sound;
			minimumDuration = std::max(minimumDuration, end + (marker ? EditorUI::MinimumTimelineDuration : 0.0f));

			const Engine::FloatCurve* activeCurve = nullptr;
			const char* curveLabel = nullptr;
			if constexpr (std::is_same_v<std::decay_t<decltype(track)>, Engine::VFXMeshTrack>)
			{
				if (track.useScaleCurve && !track.scaleCurve.IsEmpty())
				{
					activeCurve = &track.scaleCurve;
					curveLabel = "Scale";
				}
				else if (track.useAlphaCurve && !track.alphaCurve.IsEmpty())
				{
					activeCurve = &track.alphaCurve;
					curveLabel = "Alpha";
				}
			}
			else if constexpr (std::is_same_v<std::decay_t<decltype(track)>, Engine::VFXLightTrack>)
			{
				if (track.useIntensityCurve && !track.intensityCurve.IsEmpty())
				{
					activeCurve = &track.intensityCurve;
					curveLabel = "Light";
				}
			}

			rows.push_back({
				.Id = VFXRowId(type, i),
				.Label = track.trackName,
				.Icon = style.Icon,
				.Color = style.Color,
				.Start = track.startTime,
				.End = end,
				.Selected = m_SelectedTrack.type == type && m_SelectedTrack.index == static_cast<int>(i),
				.Kind = marker ? EditorUI::TimelineRowKind::Marker : EditorUI::TimelineRowKind::Interval,
				.Curve = activeCurve,
				.CurveLabel = curveLabel,
				.Disabled = !track.enabled || track.isMuted
				});
		}
	});
	const auto result = EditorUI::DrawEffectTimeline(m_Timeline, rows, GetTimelineDuration(),
		m_TimelineSeek.value_or(m_CurrentTime), minimumDuration, "VFXEditor.AddTrack");
	if (result.Selection)
		SelectTrack({ static_cast<ESelectedVFXTrackType>(*result.Selection >> 32), static_cast<int>(*result.Selection & 0xffffffffu) });
	if (result.Seek) ScrubTime(*result.Seek);
	if (result.Commit) ApplyTimelineEdit(*result.Commit);

	// 팝업은 연 창과 같은 창에서 그려야 한다. 타임라인 창 안에서 열고 그린다.
	if (result.AddTrack) ImGui::OpenPopup("##TimelineAddTrackPopup");
	if (ImGui::BeginPopup("##TimelineAddTrackPopup"))
	{
		DrawAddTrackMenuItems(*effect);
		ImGui::EndPopup();
	}
}

bool VFXEditorPanel::CanExecuteTimelineAction(EditorUI::TimelineAction action) const
{
	using enum EditorUI::TimelineAction;
	auto* effect = m_EffectHandle.Get();
	if (!m_Open || !effect || IsAsyncLoading() || m_Timeline.Drag || m_CurveModal.IsOpen()) return false;
	if (action == Undo) return m_InspectorEdit.has_value() || m_TimelineHistory.CanUndo();
	if (action == Redo) return !m_InspectorEdit && m_TimelineHistory.CanRedo();
	if (action == Delete) return VisitVFXTrack(*effect, m_SelectedTrack, [](auto&, std::size_t) {});
	return true;
}

void VFXEditorPanel::ExecuteTimelineAction(EditorUI::TimelineAction action)
{
	using enum EditorUI::TimelineAction;
	if (!CanExecuteTimelineAction(action)) return;
	if ((action == Undo || action == Loop) && m_InspectorEdit) CommitInspectorEdit();
	const float time = m_TimelineSeek.value_or(m_CurrentTime);
	switch (action)
	{
	case PlayPause:
		if (!m_IsPlaying && time >= GetTimelineDuration()) RestartSimulation();
		m_IsPlaying = !m_IsPlaying;
		if (!m_IsPlaying) StopPreviewSounds();
		break;
	case Previous: ScrubTime(time - 1.0f / EditorUI::EffectTimelineFPS); break;
	case Next: ScrubTime(time + 1.0f / EditorUI::EffectTimelineFPS); break;
	case First: ScrubTime(0.0f); break;
	case Last: ScrubTime(GetTimelineDuration()); break;
	case Loop:
	{
		auto* effect = m_EffectHandle.Get();
		const bool previous = effect->IsLooping();
		const auto apply = [this, effect](bool value) {
			effect->SetLooping(value);
			m_IsLooping = value;
			InvalidateTimelinePreview();
			return true;
		};
		m_TimelineHistory.Execute(std::make_unique<CallbackEditorCommand>(LOCAL("Action.Timeline.ToggleLoop"),
			[apply, previous] { return apply(!previous); }, [apply, previous] { return apply(previous); }));
		break;
	}
	case Fit: m_Timeline.Fit(); break;
	case Delete: DeleteSelectedTrack(); break;
	case Undo: m_TimelineHistory.Undo(); break;
	case Redo: m_TimelineHistory.Redo(); break;
	case Save: SaveEffect(); break;
	}
}

void VFXEditorPanel::DrawInspector()
{
	// 앞 프레임에서 시작한 속성 편집이 끝났으면(활성 위젯 없음) Undo 한 단위로 확정한다.
	if (m_InspectorEdit && !ImGui::IsAnyItemActive()) CommitInspectorEdit();

	auto* effect = m_EffectHandle.Get();
	if (!effect)
	{
		ImGui::TextDisabled("%s", LOCAL_CSTR("VFXEditor.NoEffectLoaded"));
		return;
	}

	if (!m_SelectedTrack.IsValid())
	{
		ImGui::TextDisabled("%s", LOCAL_CSTR("VFXEditor.SelectTrackPrompt"));
		return;
	}

	ImGui::Text(ICON_FA_SLIDERS " %s", LOCAL_CSTR("VFXEditor.TrackInspector"));
	ImGui::Separator();

	// 그리기 전 값을 복사해 둔다. 이번 프레임에 처음 바뀌면 이 값이 편집 전 상태가 된다.
	const SelectedVFXTrack selection = m_SelectedTrack;
	std::any before;
	VisitVFXTrack(*effect, selection, [&](auto& tracks, std::size_t index) { before = tracks[index]; });

	if (!DrawSelectedTrackInspector(effect)) return;

	// 삭제 버튼 등으로 선택이 바뀐 프레임에는 세션을 열지 않는다.
	const bool sameTrack = m_SelectedTrack.type == selection.type && m_SelectedTrack.index == selection.index;
	if (sameTrack && !m_InspectorEdit && before.has_value())
		m_InspectorEdit = VFXInspectorEdit{ effect, selection, std::move(before) };
}

bool VFXEditorPanel::DrawSelectedTrackInspector(Engine::CompositeEffect* effect)
{
	bool changed = false;

	// 리플렉션에 등록된 트랙 구조체를 공용 속성 표로 그린다. 하나라도 바뀌면 true.
	const auto drawReflectedTrack = [](auto& track)
		{
			const auto object = reflection::ObjectView::From(track);
			const auto& registry = Engine::ReflectionRegistry::Get().GetRegistry();
			const auto* type = registry.FindType(object);
			return type != nullptr && ReflectionPropertyDrawer::DrawPropertyTable(*type, object, &registry);
		};

	// 1. Sprite Track Inspector
	if (m_SelectedTrack.type == ESelectedVFXTrackType::Sprite)
	{
		auto& tracks = effect->GetSpriteTracks();
		if (m_SelectedTrack.index < 0 || m_SelectedTrack.index >= static_cast<int>(tracks.size())) return false;
		auto& track = tracks[m_SelectedTrack.index];

		if (drawReflectedTrack(track)) changed = true;

		ImGui::Spacing();
		if (ImGui::Button((std::string(ICON_FA_TRASH " ") + LOCAL_CSTR("EditorCommon.Delete")).c_str()))
		{
			DeleteSelectedTrack();
		}
	}
	// 2. Particle Track Inspector
	else if (m_SelectedTrack.type == ESelectedVFXTrackType::Particle)
	{
		auto& tracks = effect->GetParticleTracks();
		if (m_SelectedTrack.index < 0 || m_SelectedTrack.index >= static_cast<int>(tracks.size())) return false;
		auto& track = tracks[m_SelectedTrack.index];

		if (drawReflectedTrack(track)) changed = true;

		ImGui::Spacing();
		if (ImGui::Button((std::string(ICON_FA_TRASH " ") + LOCAL_CSTR("EditorCommon.Delete")).c_str()))
		{
			DeleteSelectedTrack();
		}
	}
	// 3. Light Flash Track Inspector
	else if (m_SelectedTrack.type == ESelectedVFXTrackType::Light)
	{
		auto& tracks = effect->GetLightTracks();
		if (m_SelectedTrack.index < 0 || m_SelectedTrack.index >= static_cast<int>(tracks.size())) return false;
		auto& track = tracks[m_SelectedTrack.index];

		if (drawReflectedTrack(track)) changed = true;
		if (track.useIntensityCurve && EditorUI::BeginPropertyTable("LightCurvePresetTable", EditorUI::InspectorPropertyTableStyle()))
		{
			if (DrawCurvePresetRow("VFXEditor.IntensityCurvePreset", track.intensityCurve, IntensityCurvePresets, m_CurveModal, [this] { return CaptureCurveHistory(); })) changed = true;
			EditorUI::EndPropertyTable();
		}

		ImGui::Spacing();
		if (ImGui::Button((std::string(ICON_FA_TRASH " ") + LOCAL_CSTR("EditorCommon.Delete")).c_str()))
		{
			DeleteSelectedTrack();
		}
	}
	// 4. Mesh Track Inspector
	else if (m_SelectedTrack.type == ESelectedVFXTrackType::Mesh)
	{
		auto& tracks = effect->GetMeshTracks();
		if (m_SelectedTrack.index < 0 || m_SelectedTrack.index >= static_cast<int>(tracks.size())) return false;
		auto& track = tracks[m_SelectedTrack.index];

		// 메시/텍스처 미지정 기본값 힌트는 리플렉션 상단에 배치하여 커브 영역과 분리한다
		const auto* meshMaterial = track.materialHandle.Get();
		const bool usesRingTexture = !track.textureHandle.IsValid() && (!meshMaterial || meshMaterial->GetTextureBindings().empty() || !meshMaterial->GetTextureBindings()[0].texture.Get());
		if (!track.meshHandle.IsValid() || usesRingTexture)
		{
			if (!track.meshHandle.IsValid()) ImGui::TextDisabled(ICON_FA_CIRCLE_INFO " %s", LOCAL_CSTR("VFXEditor.QuadDefault"));
			if (usesRingTexture) ImGui::TextDisabled(ICON_FA_CIRCLE_INFO " %s", LOCAL_CSTR("VFXEditor.RingTextureDefault"));
			ImGui::Spacing();
		}

		if (drawReflectedTrack(track)) changed = true;

		// 커브 프리셋 및 정밀 편집 모달 행
		if ((track.useScaleCurve || track.useAlphaCurve) && EditorUI::BeginPropertyTable("MeshCurvePresetTable", EditorUI::InspectorPropertyTableStyle()))
		{
			if (track.useScaleCurve && DrawCurvePresetRow("VFXEditor.ScaleCurvePreset", track.scaleCurve, ScaleCurvePresets, m_CurveModal, [this] { return CaptureCurveHistory(); })) changed = true;
			if (track.useAlphaCurve && DrawCurvePresetRow("VFXEditor.AlphaCurvePreset", track.alphaCurve, AlphaCurvePresets, m_CurveModal, [this] { return CaptureCurveHistory(); })) changed = true;
			EditorUI::EndPropertyTable();
		}

		ImGui::Spacing();
		if (ImGui::Button((std::string(ICON_FA_TRASH " ") + LOCAL_CSTR("EditorCommon.Delete")).c_str()))
		{
			DeleteSelectedTrack();
		}
	}
	// 5. Sound Track Inspector
	else if (m_SelectedTrack.type == ESelectedVFXTrackType::Sound)
	{
		auto& tracks = effect->GetSoundTracks();
		if (m_SelectedTrack.index < 0 || m_SelectedTrack.index >= static_cast<int>(tracks.size())) return false;
		auto& track = tracks[m_SelectedTrack.index];

		const auto object = reflection::ObjectView::From(track);
		const auto& registry = Engine::ReflectionRegistry::Get().GetRegistry();
		const auto* type = registry.FindType(object);
		const auto drawRegistered = [&](const char* key, std::string_view name, auto&& fallback)
		{
			EditorUI::PropertyRow(key);
			if (type)
			{
				const auto property = std::find_if(type->Properties.begin(), type->Properties.end(),
					[&](const auto& p) { return p.Name == name; });
				if (property != type->Properties.end())
					return ReflectionPropertyDrawer::DrawValue(*property, object, &registry);
			}
			ImGui::SetNextItemWidth(-FLT_MIN);
			return fallback();
		};
		const auto style = EditorUI::InspectorPropertyTableStyle();
		if (EditorUI::BeginPropertyTable("SoundTrackInspectorTable", style))
		{
			if (drawRegistered("VFXEditor.PropTrackName", "trackName", [&] {
				char nameBuf[128];
				strncpy_s(nameBuf, track.trackName.c_str(), sizeof(nameBuf) - 1);
				if (!ImGui::InputText("##Name", nameBuf, sizeof(nameBuf))) return false;
				track.trackName = nameBuf;
				return true;
			})) changed = true;

			if (drawRegistered("VFXEditor.PropEnabled", "enabled",
				[&] { return ImGui::Checkbox("##Enabled", &track.enabled); }))
			{
				changed = true; InvalidateTimelinePreview();
			}

			EditorUI::PropertyRow("VFXEditor.PropStartTime");
			ImGui::SetNextItemWidth(-FLT_MIN);

			const auto row = VFXRowId(ESelectedVFXTrackType::Sound, static_cast<std::size_t>(m_SelectedTrack.index));
			float startTime = m_SoundStartEdit && m_SoundStartEdit->Row == row
				? m_SoundStartEdit->Start
				: track.startTime;

			ImGui::PushID(m_SelectedTrack.index);
			ImGui::DragFloat("##StartTime", &startTime, 0.02f, 0.0f, 30.0f, "%.2fs", ImGuiSliderFlags_AlwaysClamp);
			const bool active = ImGui::IsItemActive();
			const bool commit = ImGui::IsItemDeactivatedAfterEdit();
			ImGui::PopID();

			const EditorUI::TimelineEdit edit{ EditorUI::TimelineDragMode::Move, row, startTime, startTime };
			if (active)
				m_SoundStartEdit = edit;
			else
				m_SoundStartEdit.reset();

			if (commit && std::isfinite(startTime) && startTime != track.startTime)
				ApplyTimelineEdit(edit);

			const float maxSoundTime = std::max(30.0f, GetTimelineDuration());

			EditorUI::PropertyRow("VFXEditor.PropSoundDuration");
			ImGui::SetNextItemWidth(-FLT_MIN);
			if (ImGui::DragFloat("##SoundDuration", &track.playDuration, 0.02f, 0.0f, maxSoundTime, "%.2fs",
				ImGuiSliderFlags_AlwaysClamp))
			{
				changed = true; InvalidateTimelinePreview();
			}

			EditorUI::PropertyRow("VFXEditor.PropFadeOutDuration");
			ImGui::SetNextItemWidth(-FLT_MIN);
			if (ImGui::DragFloat("##FadeOutDuration", &track.fadeOutDuration, 0.01f, 0.0f, maxSoundTime, "%.2fs",
				ImGuiSliderFlags_AlwaysClamp))
			{
				changed = true; InvalidateTimelinePreview();
			}

			if (drawRegistered("VFXEditor.PropAudioClip", "audioClip",
				[&] { return Editor::DrawInspectorResourcePicker(track.audioClip, true, true); }))
			{
				changed = true; InvalidateTimelinePreview();
			}

			EditorUI::PropertyRow("VFXEditor.PropVolume");
			ImGui::SetNextItemWidth(-FLT_MIN);
			if (ImGui::DragFloat("##Volume", &track.volume, 0.02f, 0.0f, 2.0f, "%.2f")) changed = true;

			EditorUI::PropertyRow("VFXEditor.PropPitch");
			ImGui::SetNextItemWidth(-FLT_MIN);
			if (ImGui::DragFloat("##Pitch", &track.pitch, 0.02f, 0.1f, 3.0f, "%.2f")) changed = true;

			EditorUI::EndPropertyTable();
		}

		ImGui::Spacing();
		if (ImGui::Button((std::string(ICON_FA_TRASH " ") + LOCAL_CSTR("EditorCommon.Delete")).c_str()))
		{
			DeleteSelectedTrack();
		}
	}
	// 6. Trail Track Inspector
	else if (m_SelectedTrack.type == ESelectedVFXTrackType::Trail)
	{
		auto& tracks = effect->GetTrailTracks();
		if (m_SelectedTrack.index < 0 || m_SelectedTrack.index >= static_cast<int>(tracks.size())) return false;
		auto& track = tracks[m_SelectedTrack.index];

		if (drawReflectedTrack(track)) changed = true;

		ImGui::Spacing();
		if (ImGui::Button((std::string(ICON_FA_TRASH " ") + LOCAL_CSTR("EditorCommon.Delete")).c_str()))
		{
			DeleteSelectedTrack();
		}
	}
	// 7. Decal Track Inspector
	else if (m_SelectedTrack.type == ESelectedVFXTrackType::Decal)
	{
		auto& tracks = effect->GetDecalTracks();
		if (m_SelectedTrack.index < 0 || m_SelectedTrack.index >= static_cast<int>(tracks.size())) return false;
		auto& track = tracks[m_SelectedTrack.index];

		if (drawReflectedTrack(track)) changed = true;

		ImGui::Spacing();
		if (ImGui::Button((std::string(ICON_FA_TRASH " ") + LOCAL_CSTR("EditorCommon.Delete")).c_str()))
		{
			DeleteSelectedTrack();
		}
	}
	// 8. Camera Shake Track Inspector
	else if (m_SelectedTrack.type == ESelectedVFXTrackType::CameraShake)
	{
		auto& tracks = effect->GetCameraShakeTracks();
		if (m_SelectedTrack.index < 0 || m_SelectedTrack.index >= static_cast<int>(tracks.size())) return false;
		auto& track = tracks[m_SelectedTrack.index];

		if (drawReflectedTrack(track)) changed = true;

		ImGui::Spacing();
		if (ImGui::Button((std::string(ICON_FA_TRASH " ") + LOCAL_CSTR("EditorCommon.Delete")).c_str()))
		{
			DeleteSelectedTrack();
		}
	}
	return changed;
}

void VFXEditorPanel::StopPreviewSounds()
{
	if (auto* scene = m_PreviewWidget.GetScene(); scene && m_PreviewEntity)
		scene->GetComponent<Engine::CompositeVFXComponent>(*m_PreviewEntity).StopSoundTracks();
}
