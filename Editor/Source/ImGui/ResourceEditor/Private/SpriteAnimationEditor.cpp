#include "SpriteAnimationEditor.h"
#include "ImGuiManager.h"
#include "ResourceManager.h"
#include "NotificationManager.h"
#include <IconsFontAwesome7.h>
#include "ShortcutManager.h"

SpriteAnimationEditor::SpriteAnimationEditor()
{
	m_Name = L"Sprite Animation Editor";
	m_DisplayNameKey = "ResourceEditor.SpriteAnimationTitle";
}

bool SpriteAnimationEditor::IsSupported(const std::filesystem::path& assetPath) const
{
	const auto ext = assetPath.extension().wstring();
	return ext == L".spriteanim" || ext == L".bamspriteanim";
}

void SpriteAnimationEditor::SetTargetResource(const std::filesystem::path& assetPath)
{
	ResourceEditorInterface::SetTargetResource(assetPath);
	LoadAnimation(assetPath);
}

void SpriteAnimationEditor::LoadAnimation(const std::filesystem::path& path)
{
	auto& rm = Engine::ResourceManager::Get();
	const std::wstring normPath = Engine::NormalizePath(path.wstring());
	m_AnimationHandle = rm.GetResourceHandle<Engine::SpriteAnimation>(normPath);

	if (!m_AnimationHandle.IsValid() && std::filesystem::exists(path))
	{
		auto* newAnim = Engine::SpriteAnimation::Create();
		if (newAnim)
		{
			newAnim->LoadFromFile(normPath);
			m_AnimationHandle = rm.AddResource<Engine::SpriteAnimation>(normPath, newAnim);
		}
	}

	m_CurrentTime = 0.0f;
	m_CurrentFrameIndex = 0;
	m_IsPlaying = true;
	m_IsDirty = false;
}

bool SpriteAnimationEditor::CanSaveAnimation() const
{
	return m_Open && GetAnimation() != nullptr && !m_TargetResourcePath.empty();
}

void SpriteAnimationEditor::SaveAnimation()
{
	if (!CanSaveAnimation()) return;

	auto* anim = GetAnimation();

	if (!anim || m_TargetResourcePath.empty()) return;

	if (anim->SaveToFile(m_TargetResourcePath.wstring()))
	{
		m_IsDirty = false;
		NotificationManager::Get().AddNotification(LOCAL("ResourceEditor.SpriteAnimationTitle"),
			LOCAL_FMT("Notification.Saved", m_TargetResourcePath.filename().string()), ELogLevel::Info);
	}
	else
	{
		NotificationManager::Get().AddNotification(LOCAL("ResourceEditor.SpriteAnimationTitle"),
			LOCAL_FMT("Notification.SaveFailed", m_TargetResourcePath.filename().string()), ELogLevel::Error);
	}
}

void SpriteAnimationEditor::Update(f32 dt)
{
	if (!m_Open) return;
	auto* anim = GetAnimation();
	if (!anim) return;

	if (m_IsPlaying && anim->GetFrameCount() > 0)
	{
		m_CurrentTime += dt;
		const bool loop = anim->GetPlayMode() == Engine::ESpriteAnimationPlayMode::Loop;
		m_CurrentFrameIndex = anim->GetFrameIndexAtTime(m_CurrentTime, loop);
	}
}

void SpriteAnimationEditor::Draw()
{
	m_Focused = false;
	m_Hovered = false;
	if (!m_Open) return;

	std::string title = LOCAL("ResourceEditor.SpriteAnimationTitle");
	if (!m_TargetResourcePath.empty())
		title += " - " + m_TargetResourcePath.filename().string();
	if (m_IsDirty) title += " *";
	title += "###" + WStrToStr(m_Name);

	ImGui::SetNextWindowSize(ImVec2(800.0f, 600.0f), ImGuiCond_FirstUseEver);
	if (ImGui::Begin(title.c_str(), &m_Open, ImGuiWindowFlags_MenuBar))
	{
		m_Focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
		m_Hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
		DrawToolbar();
		const ImVec2 contentAvail = ImGui::GetContentRegionAvail();
		const float splitterThickness = 4.0f;
		const float spacingY = ImGui::GetStyle().ItemSpacing.y;
		const float spacingX = ImGui::GetStyle().ItemSpacing.x;
		// 1. 상하 높이 계산 (상단 메인 최소 180px, 하단 필름스트립 최소 60px)
		const float minMainH = 180.0f;
		const float minFilmH = 60.0f;
		float mainHeight = (std::max)(minMainH, contentAvail.y - m_FilmstripHeight - splitterThickness - spacingY);
		// 2. 좌우 너비 계산 (좌측 캔버스 최소 200px, 우측 인스펙터 최소 180px)
		const float minCanvasW = 200.0f;
		const float minInspectorW = 180.0f;
		float inspectorWidth = (std::max)(minInspectorW, contentAvail.x - m_CanvasWidth - splitterThickness - spacingX);
		// ── [상단 좌측] 2D 스프라이트 프리뷰 캔버스 ──
		DrawPreviewCanvas(m_CanvasWidth, mainHeight);
		ImGui::SameLine();
		// ── [좌우 스플리터] 캔버스 ↔ 프레임 목록 ──
		Editor::SplitterV("##AnimCanvasSplitter", &m_CanvasWidth, &inspectorWidth, minCanvasW, minInspectorW, mainHeight, splitterThickness);
		ImGui::SameLine();
		// ── [상단 우측] 키프레임 인스펙터 목록 ──
		DrawFramesList(inspectorWidth, mainHeight);
		// ── [상하 스플리터] 상단 메인 ↔ 하단 필름스트립 ──
		Editor::SplitterH("##AnimFilmstripSplitter", &mainHeight, &m_FilmstripHeight, minMainH, minFilmH, contentAvail.x, splitterThickness);
		// ── [하단] 애니메이션 타임라인 필름스트립 ──
		DrawFilmstrip(m_FilmstripHeight);
	}
	ImGui::End();
}

void SpriteAnimationEditor::DrawToolbar()
{
	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu(LOCAL_CSTR("EditorMenu.File")))
		{
			ACTION_MENU_ITEM(LOCAL_LABEL("EditorCommon.Save").c_str(), "SpriteAnimation.Save");
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

	auto* anim = GetAnimation();
	if (!anim) return;

	ImGui::BeginDisabled(!CanSaveAnimation());
	if (ImGui::Button(ICON_FA_FLOPPY_DISK "##SaveBtn"))
		SaveAnimation();
	ImGui::EndDisabled();

	const char* playIcon = m_IsPlaying ? ICON_FA_PAUSE "##Pause" : ICON_FA_PLAY "##Play";
	if (ImGui::Button(playIcon))
		m_IsPlaying = !m_IsPlaying;
	ImGui::SameLine();

	if (ImGui::Button(ICON_FA_STOP "##Stop"))
	{
		m_IsPlaying = false;
		m_CurrentTime = 0.0f;
		m_CurrentFrameIndex = 0;
	}
	ImGui::SameLine();

	if (ImGui::Button(ICON_FA_BACKWARD_STEP "##Prev"))
	{
		m_IsPlaying = false;
		const int32 count = anim->GetFrameCount();
		if (count > 0)
			m_CurrentFrameIndex = (m_CurrentFrameIndex - 1 + count) % count;
	}
	ImGui::SameLine();

	if (ImGui::Button(ICON_FA_FORWARD_STEP "##Next"))
	{
		m_IsPlaying = false;
		const int32 count = anim->GetFrameCount();
		if (count > 0)
			m_CurrentFrameIndex = (m_CurrentFrameIndex + 1) % count;
	}
	ImGui::SameLine();

	ImGui::Text("Frame: %d / %d", m_CurrentFrameIndex + 1, std::max<int32>(anim->GetFrameCount(), 1));
	ImGui::SameLine();

	ImGui::SetNextItemWidth(90.0f);
	f32 fps = anim->GetFPS();
	if (ImGui::DragFloat("FPS", &fps, 0.5f, 1.0f, 60.0f, "%.1f"))
	{
		anim->SetFPS(fps);
		m_IsDirty = true;
	}
	ImGui::SameLine();

	ImGui::SetNextItemWidth(110.0f);
	int32 playMode = static_cast<int32>(anim->GetPlayMode());
	const char* modeNames[] = { "Loop", "Once", "PingPong" };
	if (ImGui::Combo("Mode", &playMode, modeNames, IM_ARRAYSIZE(modeNames)))
	{
		anim->SetPlayMode(static_cast<Engine::ESpriteAnimationPlayMode>(playMode));
		m_IsDirty = true;
	}
	ImGui::SameLine();

	if (ImGui::SmallButton("Reset View"))
	{
		m_Zoom = 2.0f;
		m_PanOffset = ImVec2(0.0f, 0.0f);
	}

	ImGui::Separator();
}

void SpriteAnimationEditor::DrawPreviewCanvas(f32 availWidth, f32 availHeight)
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
	if (ImGui::IsWindowHovered())
	{
		if (ImGui::IsMouseDragging(ImGuiMouseButton_Right) || ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
		{
			const ImVec2 delta = ImGui::GetIO().MouseDelta;
			m_PanOffset.x += delta.x;
			m_PanOffset.y += delta.y;
		}
		const float wheel = ImGui::GetIO().MouseWheel;
		if (wheel != 0.0f)
			m_Zoom = std::clamp(m_Zoom + wheel * 0.25f, 0.25f, 16.0f);
	}

	// 3. 현재 프레임 스프라이트 렌더링
	auto* anim = GetAnimation();
	if (anim && anim->GetFrameCount() > 0)
	{
		const auto* keyFrame = anim->GetFrame(m_CurrentFrameIndex);
		if (keyFrame && keyFrame->sprite.IsValid() && keyFrame->sprite.Get())
		{
			auto* sprite = keyFrame->sprite.Get();
			auto* texture = sprite->GetTexture();
			auto* rhiTexture = texture ? texture->GetRHITexture() : nullptr;
			if (rhiTexture)
			{
				ImTextureID texID = ImGuiManager::Get().GetImGuiTextureID(rhiTexture);
				const vec4 uv = sprite->GetUVTransform();
				const float u0 = uv.x;
				const float v0 = uv.y;
				const float u1 = uv.x + uv.z;
				const float v1 = uv.y + uv.w;

				const Engine::Rect region = sprite->GetRegion();
				const float spriteW = (region.width > 0.0f) ? region.width : static_cast<float>(rhiTexture->GetWidth());
				const float spriteH = (region.height > 0.0f) ? region.height : static_cast<float>(rhiTexture->GetHeight());

				const ImVec2 center(canvasPos.x + canvasSize.x * 0.5f + m_PanOffset.x, canvasPos.y + canvasSize.y * 0.5f + m_PanOffset.y);
				const vec2 pivot = sprite->GetPivot();
				const ImVec2 pMin(center.x - spriteW * m_Zoom * pivot.x, center.y - spriteH * m_Zoom * (1.0f - pivot.y));
				const ImVec2 pMax(pMin.x + spriteW * m_Zoom, pMin.y + spriteH * m_Zoom);

				drawList->AddImage(texID, pMin, pMax, ImVec2(u0, v0), ImVec2(u1, v1));

				// 피벗 십자선
				drawList->AddLine(ImVec2(center.x - 8.0f, center.y), ImVec2(center.x + 8.0f, center.y), IM_COL32(255, 60, 60, 220), 1.5f);
				drawList->AddLine(ImVec2(center.x, center.y - 8.0f), ImVec2(center.x, center.y + 8.0f), IM_COL32(60, 255, 60, 220), 1.5f);
			}
		}
	}

	ImGui::SetCursorPos(ImVec2(8.0f, 8.0f));
	ImGui::TextColored(ImVec4(0.85f, 0.85f, 0.85f, 1.0f), "Zoom: %.0f%%", m_Zoom * 100.0f);

	ImGui::EndChild();
}

void SpriteAnimationEditor::DrawFramesList(f32 availWidth, f32 availHeight)
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

	if (ImGui::Button(ICON_FA_PLUS " Add Frame", ImVec2(-1.0f, 26.0f)))
	{
		anim->AddFrame(Engine::ResourceHandle<Engine::Sprite>(), 1.0f);
		m_IsDirty = true;
	}

	auto& frames = anim->GetFrames();
	for (int32 i = 0; i < static_cast<int32>(frames.size()); ++i)
	{
		ImGui::PushID(i);
		const bool isSelected = (i == m_CurrentFrameIndex);
		if (isSelected) ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.45f, 0.7f, 0.6f));

		const std::string headerLabel = "Frame #" + std::to_string(i);
		if (ImGui::Selectable(headerLabel.c_str(), isSelected, 0, ImVec2(ImGui::GetContentRegionAvail().x - 70.0f, 0.0f)))
		{
			m_CurrentFrameIndex = i;
			m_IsPlaying = false;
		}
		if (isSelected) ImGui::PopStyleColor();

		ImGui::SameLine();
		if (ImGui::Button(ICON_FA_ARROW_UP) && i > 0)
		{
			anim->SwapFrames(i, i - 1);
			m_IsDirty = true;
		}
		ImGui::SameLine();
		if (ImGui::Button(ICON_FA_ARROW_DOWN) && i + 1 < static_cast<int32>(frames.size()))
		{
			anim->SwapFrames(i, i + 1);
			m_IsDirty = true;
		}
		ImGui::SameLine();
		if (ImGui::Button(ICON_FA_XMARK))
		{
			anim->RemoveFrame(i);
			m_IsDirty = true;
			ImGui::PopID();
			break;
		}

		ImGui::SetNextItemWidth(90.0f);
		if (ImGui::DragFloat("Duration", &frames[i].frameDuration, 0.1f, 0.1f, 10.0f, "%.1fx"))
			m_IsDirty = true;

		ImGui::Separator();
		ImGui::PopID();
	}

	ImGui::EndChild();
}

void SpriteAnimationEditor::DrawFilmstrip(f32 height)
{
	ImGui::BeginChild("Filmstrip", ImVec2(0.0f, height), true, ImGuiWindowFlags_HorizontalScrollbar);
	auto* anim = GetAnimation();
	if (!anim || anim->GetFrameCount() == 0)
	{
		ImGui::TextDisabled("No frames in animation. Click 'Add Frame' above.");
		ImGui::EndChild();
		return;
	}

	const auto& frames = anim->GetFrames();
	const float itemSize = 64.0f;

	for (int32 i = 0; i < static_cast<int32>(frames.size()); ++i)
	{
		ImGui::PushID(i);
		if (i > 0) ImGui::SameLine();

		const bool isCurrent = (i == m_CurrentFrameIndex);
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
				texID = ImGuiManager::Get().GetImGuiTextureID(rhiTexture);
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
				m_CurrentFrameIndex = i;
				m_IsPlaying = false;
			}
		}
		else
		{
			const std::string label = "#" + std::to_string(i);
			if (ImGui::Button(label.c_str(), ImVec2(itemSize + 8.0f, itemSize + 8.0f)))
			{
				m_CurrentFrameIndex = i;
				m_IsPlaying = false;
			}
		}

		if (isCurrent)
			ImGui::PopStyleColor();

		ImGui::PopID();
	}

	ImGui::EndChild();
}

void SpriteAnimationEditor::CaptureState(EditorWorkspace& workspace) const
{
	IImGui::CaptureState(workspace);
	workspace.SetSplitter("SpriteAnim.CanvasWidth", m_CanvasWidth);
	workspace.SetSplitter("SpriteAnim.FilmstripHeight", m_FilmstripHeight);
}

void SpriteAnimationEditor::RestoreState(const EditorWorkspace& workspace)
{
	IImGui::RestoreState(workspace);
	m_CanvasWidth = workspace.GetSplitter("SpriteAnim.CanvasWidth", 520.0f);
	m_FilmstripHeight = workspace.GetSplitter("SpriteAnim.FilmstripHeight", 110.0f);
}