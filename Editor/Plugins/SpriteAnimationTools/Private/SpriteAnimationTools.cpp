#include "ResourcePanelPluginApi.h"
#include "Editor_Includes.h"
#include "EditorUI.h"
#include "ImGuiSplitter.h"
#include "InputManager.h"
#include "SpriteAnimation.h"
#include "Texture.h"
#include "RHITexture.h"
#include "ResourceManager.h"
#include <cstring>
#include <memory>
#include <type_traits>

namespace
{
	using Editor::ResourcePanelPluginHost;

	constexpr char SpriteAnimationSaveAction[] = "SpriteAnimation.Save";

	struct SpriteAnimationToolsState
	{
		bool IsPlaying = true;
		float CurrentTime = 0.0f;
		std::int32_t CurrentFrameIndex = 0;
		float Zoom = 2.0f;
		ImVec2 PanOffset{ 0.0f, 0.0f };
		bool IsDirty = false;
		float CanvasWidth = 520.0f;
		float FilmstripHeight = 110.0f;
	};

	struct SpriteAnimationSession
	{
		SpriteAnimationToolsState State;
		Engine::ResourceHandle<Engine::SpriteAnimation> Animation;
	};

	struct SpriteAnimationSnapshot
	{
		SpriteAnimationToolsState State;
		Engine::Handle Animation;
	};

	static_assert(std::is_trivially_copyable_v<SpriteAnimationSnapshot>);

	bool SupportsSpriteAnimation(const wchar_t* path)
	{
		const auto name = Engine::ToLowerStr(Engine::WStrToStr(std::filesystem::path(path).filename().wstring()));
		return name.ends_with(".spriteanim") || name.ends_with(".bamspriteanim") ||
			name.ends_with(".spriteanim.json") || name.ends_with(".bamspriteanim.json");
	}

	void* CreateSpriteAnimationSession(const void* snapshot)
	{
		auto session = std::make_unique<SpriteAnimationSession>();
		if (snapshot)
		{
			SpriteAnimationSnapshot state{};
			std::memcpy(&state, snapshot, sizeof(state));
			session->State = state.State;
			if (state.Animation.IsValid())
			{
				session->Animation = Engine::ResourceHandle<Engine::SpriteAnimation>(state.Animation);
				if (!session->Animation.IsValid())
					return nullptr;
			}
		}
		return session.release();
	}

	void OpenSpriteAnimation(void* instance, const wchar_t* path)
	{
		auto& session = *static_cast<SpriteAnimationSession*>(instance);
		auto& manager = Engine::ResourceManager::Get();
		const auto normalized = Engine::NormalizePath(path);
		session.Animation = manager.GetResourceHandle<Engine::SpriteAnimation>(normalized);
		if (!session.Animation.IsValid() && std::filesystem::exists(path))
		{
			if (auto* animation = Engine::SpriteAnimation::Create())
			{
				animation->LoadFromFile(normalized);
				session.Animation = manager.AddResource<Engine::SpriteAnimation>(normalized, animation);
			}
		}

		session.State.CurrentTime = 0.0f;
		session.State.CurrentFrameIndex = 0;
		session.State.IsPlaying = true;
		session.State.IsDirty = false;
	}

	class SpriteAnimationPanelUi
	{
	public:
		SpriteAnimationPanelUi(SpriteAnimationSession& session, const ResourcePanelPluginHost& host)
			: m_State(session.State), m_Animation(session.Animation.Get()), m_Host(host) {}

		void DrawCustomToolbar();
		void DrawCustomQuickAccess();
		void DrawCustomContent();

	private:
		Engine::SpriteAnimation* GetAnimation() const { return m_Animation; }
		void DrawPreviewCanvas(f32 availWidth, f32 availHeight);
		void DrawFramesList(f32 availWidth, f32 availHeight);
		void DrawFilmstrip(f32 height);

		SpriteAnimationToolsState& m_State;
		Engine::SpriteAnimation* m_Animation;
		const ResourcePanelPluginHost& m_Host;
	};

	void BindSpriteAnimationImGui(const ResourcePanelPluginHost& host)
	{
		ImGui::SetAllocatorFunctions(host.Allocate, host.Free, host.AllocatorData);
		ImGui::SetCurrentContext(host.ImGui);
	}

	void UpdateSpriteAnimation(void* instance, float dt)
	{
		auto& session = *static_cast<SpriteAnimationSession*>(instance);
		auto* anim = session.Animation.Get();
		if (!anim) return;

		if (session.State.IsPlaying && anim->GetFrameCount() > 0)
		{
			session.State.CurrentTime += dt;
			const bool loop = anim->GetPlayMode() == Engine::ESpriteAnimationPlayMode::Loop;
			session.State.CurrentFrameIndex = anim->GetFrameIndexAtTime(session.State.CurrentTime, loop);
		}
	}

	void SpriteAnimationPanelUi::DrawCustomContent()
	{
		const ImVec2 contentAvail = ImGui::GetContentRegionAvail();
		const float splitterThickness = 4.0f;
		const float spacingY = ImGui::GetStyle().ItemSpacing.y;
		const float spacingX = ImGui::GetStyle().ItemSpacing.x;
		// 1. 상하 높이 계산 (상단 메인 최소 180px, 하단 필름스트립 최소 60px)
		const float minMainH = 180.0f;
		const float minFilmH = 60.0f;
		float mainHeight = (std::max)(minMainH, contentAvail.y - m_State.FilmstripHeight - splitterThickness - spacingY);
		// 2. 좌우 너비 계산 (좌측 캔버스 최소 200px, 우측 인스펙터 최소 180px)
		const float minCanvasW = 200.0f;
		const float minInspectorW = 180.0f;
		float inspectorWidth = (std::max)(minInspectorW, contentAvail.x - m_State.CanvasWidth - splitterThickness - spacingX);
		// ── [상단 좌측] 2D 스프라이트 프리뷰 캔버스 ──
		DrawPreviewCanvas(m_State.CanvasWidth, mainHeight);
		ImGui::SameLine();
		// ── [좌우 스플리터] 캔버스 ↔ 프레임 목록 ──
		Editor::SplitterV("##AnimCanvasSplitter", &m_State.CanvasWidth, &inspectorWidth, minCanvasW, minInspectorW, mainHeight, splitterThickness);
		ImGui::SameLine();
		// ── [상단 우측] 키프레임 인스펙터 목록 ──
		DrawFramesList(inspectorWidth, mainHeight);
		// ── [상하 스플리터] 상단 메인 ↔ 하단 필름스트립 ──
		Editor::SplitterH("##AnimFilmstripSplitter", &mainHeight, &m_State.FilmstripHeight, minMainH, minFilmH, contentAvail.x, splitterThickness);
		// ── [하단] 애니메이션 타임라인 필름스트립 ──
		DrawFilmstrip(m_State.FilmstripHeight);
	}

	void SpriteAnimationPanelUi::DrawCustomToolbar()
	{
		if (ImGui::BeginMenu(LOCAL_CSTR("EditorMenu.File")))
		{
			m_Host.DrawActionMenuItem(LOCAL_LABEL("EditorCommon.Save").c_str(), SpriteAnimationSaveAction);
			ImGui::EndMenu();
		}
	}

	void SpriteAnimationPanelUi::DrawCustomQuickAccess()
	{
		auto* anim = GetAnimation();
		if (!anim) return;

		EditorUI::ToolbarLayout toolbar;
		EditorUI::IconButtonOptions saveOptions;
		saveOptions.Enabled = m_Host.CanExecuteAction(SpriteAnimationSaveAction);
		saveOptions.ShortcutHint = m_Host.GetShortcutHint(SpriteAnimationSaveAction);
		if (toolbar.Icon("SaveBtn", ICON_FA_FLOPPY_DISK, "EditorCommon.Save", saveOptions))
			m_Host.ExecuteAction(SpriteAnimationSaveAction);

		const char* playIcon = m_State.IsPlaying ? ICON_FA_PAUSE : ICON_FA_PLAY;
		if (toolbar.Icon("PlayPause", playIcon, m_State.IsPlaying ? "EditorCommon.Pause" : "EditorCommon.Play"))
			m_State.IsPlaying = !m_State.IsPlaying;

		if (toolbar.Icon("Stop", ICON_FA_STOP, "EditorCommon.Stop"))
		{
			m_State.IsPlaying = false;
			m_State.CurrentTime = 0.0f;
			m_State.CurrentFrameIndex = 0;
		}

		if (toolbar.Icon("Prev", ICON_FA_BACKWARD_STEP, "Action.Timeline.PrevFrame"))
		{
			m_State.IsPlaying = false;
			const int32 count = anim->GetFrameCount();
			if (count > 0)
				m_State.CurrentFrameIndex = (m_State.CurrentFrameIndex - 1 + count) % count;
		}

		if (toolbar.Icon("Next", ICON_FA_FORWARD_STEP, "Action.Timeline.NextFrame"))
		{
			m_State.IsPlaying = false;
			const int32 count = anim->GetFrameCount();
			if (count > 0)
				m_State.CurrentFrameIndex = (m_State.CurrentFrameIndex + 1) % count;
		}

		ImGuiTextBuffer frameText;
		frameText.appendf(LOCAL_CSTR("SpriteAnimation.Frame"),
			m_State.CurrentFrameIndex + 1, std::max<int32>(anim->GetFrameCount(), 1));
		toolbar.Text(frameText.c_str(), false);

		toolbar.Text(LOCAL_CSTR("SpriteAnimation.Fps"));
		toolbar.SetNextItemWidth(ImGui::GetFontSize() * 5.0f);
		f32 fps = anim->GetFPS();
		if (ImGui::DragFloat("##Fps", &fps, 0.5f, 1.0f, 60.0f, "%.1f"))
		{
			anim->SetFPS(fps);
			m_State.IsDirty = true;
		}

		toolbar.Text(LOCAL_CSTR("SpriteAnimation.PlayMode"));
		toolbar.SetNextItemWidth(ImGui::GetFontSize() * 7.0f);
		int32 playMode = static_cast<int32>(anim->GetPlayMode());
		const std::string modeLabels[] = { LOCAL("SpriteAnimation.Loop"), LOCAL("SpriteAnimation.Once"), LOCAL("SpriteAnimation.PingPong") };
		const char* modeNames[] = { modeLabels[0].c_str(), modeLabels[1].c_str(), modeLabels[2].c_str() };
		if (ImGui::Combo("##PlayMode", &playMode, modeNames, IM_ARRAYSIZE(modeNames)))
		{
			anim->SetPlayMode(static_cast<Engine::ESpriteAnimationPlayMode>(playMode));
			m_State.IsDirty = true;
		}

		if (toolbar.Button("ResetView", ICON_FA_MAXIMIZE, "SpriteAnimation.ResetView"))
		{
			m_State.Zoom = 2.0f;
			m_State.PanOffset = ImVec2(0.0f, 0.0f);
		}
	}

	void SpriteAnimationPanelUi::DrawPreviewCanvas(f32 availWidth, f32 availHeight)
	{
		ImGui::BeginChild("PreviewCanvas", ImVec2(availWidth, availHeight), true,
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

		const ImVec2 canvasPos = ImGui::GetCursorScreenPos();
		const ImVec2 canvasSize = ImGui::GetContentRegionAvail();
		ImDrawList* drawList = ImGui::GetWindowDrawList();

		// 1. 체크무늬 배경
		const float gridStep = 16.0f;
		for (float y = 0.0f; y < canvasSize.y; y += gridStep)
		{
			for (float x = 0.0f; x < canvasSize.x; x += gridStep)
			{
				const bool dark = (static_cast<int>(x / gridStep) + static_cast<int>(y / gridStep)) % 2 == 0;
				const ImU32 color = dark ? IM_COL32(45, 45, 45, 255) : IM_COL32(60, 60, 60, 255);
				drawList->AddRectFilled(
					ImVec2(canvasPos.x + x, canvasPos.y + y),
					ImVec2(canvasPos.x + std::min(x + gridStep, canvasSize.x), canvasPos.y + std::min(y + gridStep, canvasSize.y)),
					color);
			}
		}

		// 2. 마우스 줌 & 패닝
		auto& input = Engine::InputManager::Get();
		if (ImGui::IsWindowHovered())
		{
			if (input.IsMouseButtonDragging(Engine::EMouseButton::Right) || input.IsMouseButtonDragging(Engine::EMouseButton::Middle))
			{
				const auto delta = input.GetMouseDelta();
				m_State.PanOffset.x += delta.x;
				m_State.PanOffset.y += delta.y;
			}
			const float wheel = input.GetMouseScrollDeltaY();
			if (wheel != 0.0f)
				m_State.Zoom = std::clamp(m_State.Zoom + wheel * 0.25f, 0.25f, 16.0f);
		}

		// 3. 현재 프레임 스프라이트 렌더링
		auto* anim = GetAnimation();
		if (anim && anim->GetFrameCount() > 0)
		{
			const auto* keyFrame = anim->GetFrame(m_State.CurrentFrameIndex);
			if (keyFrame && keyFrame->sprite.IsValid() && keyFrame->sprite.Get())
			{
				auto* sprite = keyFrame->sprite.Get();
				auto* texture = sprite->GetTexture();
				auto* rhiTexture = texture ? texture->GetRHITexture() : nullptr;
				if (rhiTexture)
				{
					ImTextureID texID = m_Host.GetTextureId(rhiTexture);
					const vec4 uv = sprite->GetUVTransform();
					const float u0 = uv.x;
					const float v0 = uv.y;
					const float u1 = uv.x + uv.z;
					const float v1 = uv.y + uv.w;

					const Engine::Rect region = sprite->GetRegion();
					const float spriteW = (region.width > 0.0f) ? region.width : static_cast<float>(rhiTexture->GetWidth());
					const float spriteH = (region.height > 0.0f) ? region.height : static_cast<float>(rhiTexture->GetHeight());

					const ImVec2 center(canvasPos.x + canvasSize.x * 0.5f + m_State.PanOffset.x, canvasPos.y + canvasSize.y * 0.5f + m_State.PanOffset.y);
					const vec2 pivot = sprite->GetPivot();
					const ImVec2 pMin(center.x - spriteW * m_State.Zoom * pivot.x, center.y - spriteH * m_State.Zoom * (1.0f - pivot.y));
					const ImVec2 pMax(pMin.x + spriteW * m_State.Zoom, pMin.y + spriteH * m_State.Zoom);

					drawList->AddImage(texID, pMin, pMax, ImVec2(u0, v0), ImVec2(u1, v1));

					// 피벗 십자선
					drawList->AddLine(ImVec2(center.x - 8.0f, center.y), ImVec2(center.x + 8.0f, center.y), IM_COL32(255, 60, 60, 220), 1.5f);
					drawList->AddLine(ImVec2(center.x, center.y - 8.0f), ImVec2(center.x, center.y + 8.0f), IM_COL32(60, 255, 60, 220), 1.5f);
				}
			}
		}

		ImGui::SetCursorPos(ImVec2(8.0f, 8.0f));
		ImGui::TextColored(ImVec4(0.85f, 0.85f, 0.85f, 1.0f), LOCAL_CSTR("SpriteAnimation.Zoom"), m_State.Zoom * 100.0f);

		ImGui::EndChild();
	}

	void SpriteAnimationPanelUi::DrawFramesList(f32 availWidth, f32 availHeight)
	{
		ImGui::BeginChild("FramesInspector", ImVec2(availWidth, availHeight), true);
		ImGui::TextUnformatted(LOCAL_CSTR("ResourceEditor.KeyFrames"));
		ImGui::Separator();

		auto* anim = GetAnimation();
		if (!anim)
		{
			ImGui::EndChild();
			return;
		}

		if (ImGui::Button((std::string(ICON_FA_PLUS " ") + LOCAL_LABEL("SpriteAnimation.AddFrame")).c_str(), ImVec2(-1.0f, 26.0f)))
		{
			anim->AddFrame(Engine::ResourceHandle<Engine::Sprite>(), 1.0f);
			m_State.IsDirty = true;
		}

		auto& frames = anim->GetFrames();
		for (int32 i = 0; i < static_cast<int32>(frames.size()); ++i)
		{
			ImGui::PushID(i);
			const bool isSelected = (i == m_State.CurrentFrameIndex);
			if (isSelected) ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.45f, 0.7f, 0.6f));

			const std::string headerLabel = LOCAL_FMT("SpriteAnimation.FrameLabel", i) + "###Frame";
			if (ImGui::Selectable(headerLabel.c_str(), isSelected, 0, ImVec2(std::max(1.0f, ImGui::GetContentRegionAvail().x -
				EditorUI::GetIconButtonSize(ICON_FA_ARROW_UP, true).x * 3.0f - ImGui::GetStyle().ItemSpacing.x * 3.0f), 0.0f)))
			{
				m_State.CurrentFrameIndex = i;
				m_State.IsPlaying = false;
			}
			if (isSelected) ImGui::PopStyleColor();

			EditorUI::SameLineIfFits(EditorUI::GetIconButtonSize(ICON_FA_ARROW_UP, true).x);
			if (EditorUI::SmallIconButton("MoveUp", ICON_FA_ARROW_UP, "EditorCommon.MoveUp", i > 0))
			{
				anim->SwapFrames(i, i - 1);
				m_State.IsDirty = true;
			}
			EditorUI::SameLineIfFits(EditorUI::GetIconButtonSize(ICON_FA_ARROW_UP, true).x);
			if (EditorUI::SmallIconButton("MoveDown", ICON_FA_ARROW_DOWN, "EditorCommon.MoveDown", i + 1 < static_cast<int32>(frames.size())))
			{
				anim->SwapFrames(i, i + 1);
				m_State.IsDirty = true;
			}
			EditorUI::SameLineIfFits(EditorUI::GetIconButtonSize(ICON_FA_ARROW_UP, true).x);
			if (EditorUI::SmallIconButton("Remove", ICON_FA_XMARK, "EditorCommon.Delete", true))
			{
				anim->RemoveFrame(i);
				m_State.IsDirty = true;
				ImGui::PopID();
				break;
			}

			ImGui::SetNextItemWidth(90.0f);
			if (ImGui::DragFloat(LOCAL_LABEL("SpriteAnimation.FrameDuration").c_str(), &frames[i].frameDuration, 0.1f, 0.1f, 10.0f, "%.1fx"))
				m_State.IsDirty = true;

			ImGui::Separator();
			ImGui::PopID();
		}

		ImGui::EndChild();
	}

	void SpriteAnimationPanelUi::DrawFilmstrip(f32 height)
	{
		ImGui::BeginChild("Filmstrip", ImVec2(0.0f, height), true, ImGuiWindowFlags_HorizontalScrollbar);
		auto* anim = GetAnimation();
		if (!anim || anim->GetFrameCount() == 0)
		{
			EditorUI::EmptyState("SpriteAnimation.EmptyFramesHint");
			ImGui::EndChild();
			return;
		}

		const auto& frames = anim->GetFrames();
		const float itemSize = 64.0f;

		for (int32 i = 0; i < static_cast<int32>(frames.size()); ++i)
		{
			ImGui::PushID(i);
			if (i > 0) ImGui::SameLine();

			const bool isCurrent = (i == m_State.CurrentFrameIndex);
			if (isCurrent)
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.6f, 0.9f, 0.8f));

			ImTextureID texID = 0;
			float u0 = 0.0f, v0 = 0.0f, u1 = 1.0f, v1 = 1.0f;

			if (frames[i].sprite.IsValid() && frames[i].sprite.Get())
			{
				auto* sprite = frames[i].sprite.Get();
				auto* texture = sprite->GetTexture();
				auto* rhiTexture = texture ? texture->GetRHITexture() : nullptr;
				if (rhiTexture)
				{
					texID = m_Host.GetTextureId(rhiTexture);
					const vec4 uv = sprite->GetUVTransform();
					u0 = uv.x;
					v0 = uv.y;
					u1 = uv.x + uv.z;
					v1 = uv.y + uv.w;
				}
			}

			if (texID)
			{
				if (ImGui::ImageButton("##FrameBtn", texID, ImVec2(itemSize, itemSize), ImVec2(u0, v0), ImVec2(u1, v1)))
				{
					m_State.CurrentFrameIndex = i;
					m_State.IsPlaying = false;
				}
			}
			else
			{
				const std::string label = "#" + std::to_string(i);
				if (ImGui::Button(label.c_str(), ImVec2(itemSize + 8.0f, itemSize + 8.0f)))
				{
					m_State.CurrentFrameIndex = i;
					m_State.IsPlaying = false;
				}
			}

			if (isCurrent)
				ImGui::PopStyleColor();

			ImGui::PopID();
		}

		ImGui::EndChild();
	}

	const Editor::ResourcePanelPluginApi SpriteAnimationApi{
		.ApiSize = sizeof(Editor::ResourcePanelPluginApi),
		.HostSize = sizeof(ResourcePanelPluginHost),
		.StateVersion = 1,
		.StateSize = sizeof(SpriteAnimationSnapshot),
		.PanelName = L"Sprite Animation Editor",
		.TitleKey = "ResourceEditor.SpriteAnimationTitle",
		.DescriptionKey = "PluginManager.SpriteAnimationToolsDescription",
		.SaveActionId = SpriteAnimationSaveAction,
		.InitialSize = ImVec2(800.0f, 600.0f),
		.Supports = SupportsSpriteAnimation,
		.Create = CreateSpriteAnimationSession,
		.Destroy = [](void* session) { delete static_cast<SpriteAnimationSession*>(session); },
		.Capture = [](const void* instance, void* output) {
			const auto& session = *static_cast<const SpriteAnimationSession*>(instance);
			const SpriteAnimationSnapshot snapshot{ session.State, session.Animation.GetRawHandle() };
			std::memcpy(output, &snapshot, sizeof(snapshot));
		},
		.Open = OpenSpriteAnimation,
		.IsDirty = [](const void* session) { return static_cast<const SpriteAnimationSession*>(session)->State.IsDirty; },
		.CanSave = [](const void* session) { return static_cast<const SpriteAnimationSession*>(session)->Animation.Get() != nullptr; },
		.Save = [](void* instance, const wchar_t* path) {
			auto& session = *static_cast<SpriteAnimationSession*>(instance);
			auto* animation = session.Animation.Get();
			if (!animation || !animation->SaveToFile(path))
				return false;
			session.State.IsDirty = false;
			return true;
		},
		.Update = UpdateSpriteAnimation,
		.DrawToolbar = [](void* session, const ResourcePanelPluginHost& host) {
			BindSpriteAnimationImGui(host);
			SpriteAnimationPanelUi(*static_cast<SpriteAnimationSession*>(session), host).DrawCustomToolbar();
		},
		.DrawQuickAccess = [](void* session, const ResourcePanelPluginHost& host) {
			BindSpriteAnimationImGui(host);
			SpriteAnimationPanelUi(*static_cast<SpriteAnimationSession*>(session), host).DrawCustomQuickAccess();
		},
		.DrawContent = [](void* session, const ResourcePanelPluginHost& host) {
			BindSpriteAnimationImGui(host);
			SpriteAnimationPanelUi(*static_cast<SpriteAnimationSession*>(session), host).DrawCustomContent();
		},
		.SaveWorkspace = [](const void* session, void* workspace, void (*set)(void*, const char*, float)) {
			const auto& state = static_cast<const SpriteAnimationSession*>(session)->State;
			set(workspace, "SpriteAnim.CanvasWidth", state.CanvasWidth);
			set(workspace, "SpriteAnim.FilmstripHeight", state.FilmstripHeight);
		},
		.RestoreWorkspace = [](void* session, const void* workspace, float (*get)(const void*, const char*, float)) {
			auto& state = static_cast<SpriteAnimationSession*>(session)->State;
			state.CanvasWidth = get(workspace, "SpriteAnim.CanvasWidth", 520.0f);
			state.FilmstripHeight = get(workspace, "SpriteAnim.FilmstripHeight", 110.0f);
		},
	};
}

extern "C" __declspec(dllexport) const Editor::ResourcePanelPluginApi* BamGetResourcePanelPluginV1()
{
	return &SpriteAnimationApi;
}
