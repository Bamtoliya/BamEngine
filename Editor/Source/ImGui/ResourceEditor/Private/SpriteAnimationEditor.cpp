#include "SpriteAnimationEditor.h"
#include "ImGuiManager.h"
#include "ResourceManager.h"
#include "NotificationManager.h"
#include <IconsFontAwesome7.h>
#include <algorithm>

SpriteAnimationEditor::SpriteAnimationEditor()
{
	m_Name = L"Sprite Animation Editor";
	m_DisplayNameKey = "ResourceEditor.SpriteAnimationTitle";
}

SpriteAnimationEditor::~SpriteAnimationEditor()
{
	if (m_Animation)
	{
		delete m_Animation;
		m_Animation = nullptr;
	}
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
	if (m_Animation)
	{
		delete m_Animation;
		m_Animation = nullptr;
	}

	m_Animation = Engine::SpriteAnimation::Create();
	if (m_Animation && std::filesystem::exists(path))
	{
		m_Animation->LoadFromFile(path.wstring());
	}
	m_CurrentTime = 0.0f;
	m_CurrentFrameIndex = 0;
	m_IsPlaying = true;
	m_IsDirty = false;
}

void SpriteAnimationEditor::SaveAnimation()
{
	if (!m_Animation || m_TargetResourcePath.empty()) return;

	if (m_Animation->SaveToFile(m_TargetResourcePath.wstring()))
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
	if (!m_Open || !m_Animation) return;

	if (m_IsPlaying && m_Animation->GetFrameCount() > 0)
	{
		m_CurrentTime += dt;
		const bool loop = m_Animation->GetPlayMode() == Engine::ESpriteAnimationPlayMode::Loop;
		m_CurrentFrameIndex = m_Animation->GetFrameIndexAtTime(m_CurrentTime, loop);
	}
}

void SpriteAnimationEditor::Draw()
{
	if (!m_Open) return;

	std::string title = LOCAL("ResourceEditor.SpriteAnimationTitle");
	if (!m_TargetResourcePath.empty())
		title += " - " + m_TargetResourcePath.filename().string();
	if (m_IsDirty) title += " *";
	title += "###" + WStrToStr(m_Name);

	ImGui::SetNextWindowSize(ImVec2(800.0f, 600.0f), ImGuiCond_FirstUseEver);
	if (ImGui::Begin(title.c_str(), &m_Open, ImGuiWindowFlags_MenuBar))
	{
		DrawToolbar();

		const ImVec2 contentAvail = ImGui::GetContentRegionAvail();
		const f32 filmstripHeight = 90.0f;
		const f32 mainHeight = std::max(contentAvail.y - filmstripHeight - 8.0f, 200.0f);
		const f32 canvasWidth = contentAvail.x * 0.65f;
		const f32 inspectorWidth = contentAvail.x - canvasWidth - ImGui::GetStyle().ItemSpacing.x;

		DrawPreviewCanvas(canvasWidth, mainHeight);
		ImGui::SameLine();
		DrawFramesList(inspectorWidth);

		ImGui::Separator();
		DrawFilmstrip(filmstripHeight);
	}
	ImGui::End();
}

void SpriteAnimationEditor::DrawToolbar()
{
	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu(LOCAL_CSTR("EditorMenu.File")))
		{
			if (ImGui::MenuItem(LOCAL_CSTR("EditorCommon.Save"), "Ctrl+S"))
				SaveAnimation();
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

	if (!m_Animation) return;

	// ── 컨트롤 툴바 ──
	if (ImGui::Button(ICON_FA_FLOPPY_DISK "##SaveBtn"))
		SaveAnimation();
	ImGui::SameLine();

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
		const int32 count = m_Animation->GetFrameCount();
		if (count > 0)
			m_CurrentFrameIndex = (m_CurrentFrameIndex - 1 + count) % count;
	}
	ImGui::SameLine();

	if (ImGui::Button(ICON_FA_FORWARD_STEP "##Next"))
	{
		m_IsPlaying = false;
		const int32 count = m_Animation->GetFrameCount();
		if (count > 0)
			m_CurrentFrameIndex = (m_CurrentFrameIndex + 1) % count;
	}
	ImGui::SameLine();

	ImGui::Text("Frame: %d / %d", m_CurrentFrameIndex + 1, std::max(m_Animation->GetFrameCount(), 1));
	ImGui::SameLine();

	ImGui::SetNextItemWidth(90.0f);
	f32 fps = m_Animation->GetFPS();
	if (ImGui::DragFloat("FPS", &fps, 0.5f, 1.0f, 60.0f, "%.1f"))
	{
		m_Animation->SetFPS(fps);
		m_IsDirty = true;
	}
	ImGui::SameLine();

	ImGui::SetNextItemWidth(110.0f);
	int32 playMode = static_cast<int32>(m_Animation->GetPlayMode());
	const char* modeNames[] = { "Loop", "Once", "PingPong" };
	if (ImGui::Combo("Mode", &playMode, modeNames, IM_ARRAYSIZE(modeNames)))
	{
		m_Animation->SetPlayMode(static_cast<Engine::ESpriteAnimationPlayMode>(playMode));
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

	// 1. 체크무늬 배경 렌더링
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

	// 2. 마우스 인터랙션 (패닝 및 줌)
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
	if (m_Animation && m_Animation->GetFrameCount() > 0)
	{
		const auto* keyFrame = m_Animation->GetFrame(m_CurrentFrameIndex);
		if (keyFrame && keyFrame->sprite.IsValid() && keyFrame->sprite.Get())
		{
			auto* sprite = keyFrame->sprite.Get();
			auto* texture = sprite->GetTexture();
			if (texture && texture->GetRHITexture())
			{
				ImTextureID texID = ImGuiManager::Get().GetImGuiTextureID(texture->GetRHITexture());
				const Engine::Rect region = sprite->GetRegion();
				const float texW = static_cast<float>(std::max(texture->GetWidth(), 1u));
				const float texH = static_cast<float>(std::max(texture->GetHeight(), 1u));

				float u0 = 0.0f, v0 = 0.0f, u1 = 1.0f, v1 = 1.0f;
				float spriteW = texW, spriteH = texH;

				if (region.right > region.left && region.bottom > region.top)
				{
					u0 = region.left / texW;
					v0 = region.top / texH;
					u1 = region.right / texW;
					v1 = region.bottom / texH;
					spriteW = region.right - region.left;
					spriteH = region.bottom - region.top;
				}

				const ImVec2 center(canvasPos.x + canvasSize.x * 0.5f + m_PanOffset.x, canvasPos.y + canvasSize.y * 0.5f + m_PanOffset.y);
				const vec2 pivot = sprite->GetPivot();
				const ImVec2 pMin(center.x - spriteW * m_Zoom * pivot.x, center.y - spriteH * m_Zoom * (1.0f - pivot.y));
				const ImVec2 pMax(pMin.x + spriteW * m_Zoom, pMin.y + spriteH * m_Zoom);

				drawList->AddImage(texID, pMin, pMax, ImVec2(u0, v0), ImVec2(u1, v1));

				// 피벗 크로스헤어
				drawList->AddLine(ImVec2(center.x - 8.0f, center.y), ImVec2(center.x + 8.0f, center.y), IM_COL32(255, 60, 60, 220), 1.5f);
				drawList->AddLine(ImVec2(center.x, center.y - 8.0f), ImVec2(center.x, center.y + 8.0f), IM_COL32(60, 255, 60, 220), 1.5f);
			}
		}
	}

	// 좌상단 줌/해상도 오버레이
	ImGui::SetCursorPos(ImVec2(8.0f, 8.0f));
	ImGui::TextColored(ImVec4(0.85f, 0.85f, 0.85f, 1.0f), "Zoom: %.0f%%", m_Zoom * 100.0f);

	ImGui::EndChild();
}

void SpriteAnimationEditor::DrawFramesList(f32 availWidth)
{
	ImGui::BeginChild("FramesInspector", ImVec2(availWidth, 0.0f), true);
	ImGui::TextUnformatted(LOCAL_CSTR("ResourceEditor.KeyFrames"));
	ImGui::Separator();

	if (!m_Animation)
	{
		ImGui::EndChild();
		return;
	}

	if (ImGui::Button(ICON_FA_PLUS " Add Frame", ImVec2(-1.0f, 26.0f)))
	{
		m_Animation->AddFrame(Engine::ResourceHandle<Engine::Sprite>(), 1.0f);
		m_IsDirty = true;
	}

	auto& frames = m_Animation->GetFrames();
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
			m_Animation->SwapFrames(i, i - 1);
			m_IsDirty = true;
		}
		ImGui::SameLine();
		if (ImGui::Button(ICON_FA_ARROW_DOWN) && i + 1 < static_cast<int32>(frames.size()))
		{
			m_Animation->SwapFrames(i, i + 1);
			m_IsDirty = true;
		}
		ImGui::SameLine();
		if (ImGui::Button(ICON_FA_XMARK))
		{
			m_Animation->RemoveFrame(i);
			m_IsDirty = true;
			ImGui::PopID();
			break;
		}

		// 각 프레임 세부 속성 (지속 시간 가중치)
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
	if (!m_Animation || m_Animation->GetFrameCount() == 0)
	{
		ImGui::TextDisabled("No frames in animation. Click 'Add Frame' above.");
		ImGui::EndChild();
		return;
	}

	const auto& frames = m_Animation->GetFrames();
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
			if (texture && texture->GetRHITexture())
			{
				texID = ImGuiManager::Get().GetImGuiTextureID(texture->GetRHITexture());
				const Engine::Rect region = sprite->GetRegion();
				const float tw = static_cast<float>(std::max(texture->GetWidth(), 1u));
				const float th = static_cast<float>(std::max(texture->GetHeight(), 1u));
				if (region.right > region.left && region.bottom > region.top)
				{
					u0 = region.left / tw;
					v0 = region.top / th;
					u1 = region.right / tw;
					v1 = region.bottom / th;
				}
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