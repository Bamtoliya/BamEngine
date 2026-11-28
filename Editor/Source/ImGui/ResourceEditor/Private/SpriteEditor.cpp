#pragma once
#include "SpriteEditor.h"
#include "AsyncResourceOpenService.h"
#include "TextureEditor.h"
#include "ResourceEditorPath.h"
#include "EditorUI.h"
#include "Functions.h"
#include "ResourceManager.h"
#include "NotificationManager.h"
#include "PlatformPath.h"
#include "ImGuiManager.h"
#include "AssetCache.h"
#include "AssetPathRelocation.h"
#include "EditorSettings.h"
#include "InputManager.h"
#include "ImGuiSplitter.h"
#include "FileDialogs.h"
#include <IconsFontAwesome7.h>
#include <sstream>
#include <algorithm>
#include <string_view>
#include <unordered_set>

namespace Editor
{
namespace
{
	bool IsValidSpriteOutputName(std::string_view name)
	{
		if (name.empty() || name.ends_with(' ') || name.ends_with('.') || name.find_first_of("<>:\"/\\|?*") != std::string_view::npos)
			return false;

		return std::none_of(name.begin(), name.end(), [](unsigned char ch) { return ch < 32; });
	}

	void NotifySpriteSaveFailure(std::string_view name)
	{
		NotificationManager::Get().AddNotification(LOCAL("ResourceEditor.SpriteTitle"),
			LOCAL_FMT("Notification.SaveFailed", name), ELogLevel::Error);
	}

	bool IsSpriteAsset(const std::filesystem::path& path)
	{
		const auto name = Engine::ToLowerStr(Engine::Unicode::WideToUtf8(path.filename().wstring()));
		return name.ends_with(".bamsprite") || name.ends_with(".bamsprite.json");
	}

	void LoadSpriteOrTextureResource(const std::filesystem::path& path,
		ResourceHandle<Engine::Texture>& outTexture, std::vector<SpriteSliceInfo>& outSlices)
	{
		auto& rm = Engine::ResourceManager::Get();
		const std::wstring normPath = Engine::NormalizePath(path.wstring());

		if (IsSpriteAsset(path))
		{
			auto spriteHandle = rm.GetResourceHandle<Engine::Sprite>(normPath);
			if (!spriteHandle.IsValid())
			{
				auto raw = rm.LoadFile(normPath);
				spriteHandle = static_cast<ResourceHandle<Engine::Sprite>>(raw);
			}

			if (spriteHandle.IsValid() && spriteHandle.Get())
			{
				auto* sprite = spriteHandle.Get();
				outTexture = sprite->GetTextureHandle();
				SpriteSliceInfo info;
				info.name = Engine::Unicode::WideToUtf8(path.stem().wstring());
				if (Engine::ToLowerStr(info.name).ends_with(".bamsprite"))
					info.name = info.name.substr(0, info.name.size() - 10);
				info.region = sprite->GetRegion();
				info.pivot = sprite->GetPivot();
				info.border = sprite->GetBorder();
				info.pixelPerUnit = sprite->GetPixelPerUnit();
				outSlices.push_back(info);
			}
		}
		else
		{
			auto raw = rm.LoadFile(normPath);
			outTexture = static_cast<ResourceHandle<Engine::Texture>>(raw);
			if (!outTexture.IsValid())
			{
				outTexture = rm.GetResourceHandle<Engine::Texture>(normPath);
			}
		}
	}

	void ScanOrCreateDefaultSlices(const std::filesystem::path& path,
		const ResourceHandle<Engine::Texture>& textureHandle, std::vector<SpriteSliceInfo>& slices)
	{
		if (!textureHandle.IsValid() || !textureHandle.Get() || !slices.empty())
			return;

		auto& rm = Engine::ResourceManager::Get();
		const std::string filename = Engine::ToLowerStr(Engine::Unicode::WideToUtf8(path.filename().wstring()));
		auto* texture = textureHandle.Get();
		auto* rhiTexture = texture->GetRHITexture();
		const float texW = rhiTexture ? static_cast<float>(rhiTexture->GetWidth()) : 0.0f;
		const float texH = rhiTexture ? static_cast<float>(rhiTexture->GetHeight()) : 0.0f;

		std::filesystem::path parentDir = path.parent_path();
		const auto stem = filename.ends_with(".bamtex.json") ? path.stem().stem() : path.stem();
		const std::string stemName = Engine::Unicode::WideToUtf8(stem.wstring());

		std::error_code ec;
		if (std::filesystem::exists(parentDir, ec))
		{
			for (const auto& entry : std::filesystem::directory_iterator(parentDir, ec))
			{
				if (!entry.is_regular_file()) continue;
				const auto entryName = Engine::Unicode::WideToUtf8(entry.path().filename().wstring());
				if (entryName.starts_with(stemName) &&
					(entryName.ends_with(".bamsprite.json") || entry.path().extension() == ".bamsprite"))
				{
					const std::wstring spriteNorm = Engine::NormalizePath(entry.path().wstring());
					auto sHandle = rm.GetResourceHandle<Engine::Sprite>(spriteNorm);
					if (!sHandle.IsValid())
					{
						sHandle = static_cast<ResourceHandle<Engine::Sprite>>(rm.LoadFile(spriteNorm));
					}
					if (sHandle.IsValid() && sHandle.Get())
					{
						auto* sp = sHandle.Get();
						const auto& referencedTexture = sp->GetTextureHandle();
						const auto* referenced = referencedTexture.Get();
						std::error_code compareError;
						if (!referenced || referenced->GetPath().empty() ||
							!std::filesystem::equivalent(Engine::PlatformPath::ResolveUri(referenced->GetPath()),
								Engine::PlatformPath::ResolveUri(path), compareError))
							continue;
						SpriteSliceInfo slice;
						slice.name = Engine::Unicode::WideToUtf8(entry.path().stem().wstring());
						if (Engine::ToLowerStr(slice.name).ends_with(".bamsprite"))
							slice.name = slice.name.substr(0, slice.name.size() - 10);
						slice.region = sp->GetRegion();
						slice.pivot = sp->GetPivot();
						slice.border = sp->GetBorder();
						slice.pixelPerUnit = sp->GetPixelPerUnit();
						slices.push_back(slice);
					}
				}
			}
		}

		if (slices.empty() && texW > 0.0f && texH > 0.0f)
		{
			SpriteSliceInfo defaultSlice;
			defaultSlice.name = Engine::Unicode::WideToUtf8(path.stem().wstring()) + "_0";
			defaultSlice.region = { 0.0f, 0.0f, texW, texH };
			defaultSlice.pivot = { 0.5f, 0.5f };
			defaultSlice.border = { 0.0f, 0.0f, 0.0f, 0.0f };
			slices.push_back(defaultSlice);
		}
	}

}

SpriteEditor::SpriteEditor()
{
	m_Name = L"Sprite Editor";
	m_DisplayNameKey = "ResourceEditor.SpriteTitle";
}

SpriteEditor::~SpriteEditor()
{
	CancelAsyncOpen();
}

bool SpriteEditor::IsSupported(const std::filesystem::path& assetPath) const
{
	const auto ext = Engine::ToLowerStr(Engine::Unicode::WideToUtf8(assetPath.extension().wstring()));
	const std::string lower = Engine::ToLowerStr(Engine::Unicode::WideToUtf8(assetPath.filename().wstring()));
	return ext == ".bamsprite" || lower.ends_with(".bamsprite.json") ||
		ext == ".bamtex" || lower.ends_with(".bamtex.json") ||
		ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" ||
		ext == ".bmp" || ext == ".dds" || ext == ".hdr";
}

bool SpriteEditor::CanSaveDocument() const
{
	return m_Open && !IsAsyncLoading() && m_TextureHandle.IsValid() && m_TextureHandle.Get() && !m_Slices.empty() &&
		!Engine::PlatformPath::IsReadOnlyTarget(m_TargetResourcePath);
}

std::vector<std::filesystem::path> SpriteEditor::GetRetainedResourcePaths() const
{
	auto paths = ResourceEditorInterface::GetRetainedResourcePaths();
	if (const auto* texture = m_TextureHandle.Get(); texture && !texture->GetPath().empty())
		paths.emplace_back(texture->GetPath());
	return paths;
}

bool SpriteEditor::PrepareAssetPathRelocation(AssetPathRelocation& relocation)
{
	if (const auto* texture = m_TextureHandle.Get(); texture && !texture->GetPath().empty() &&
		!SameResourceEditorPath(texture->GetPath(), m_TargetResourcePath) && relocation.Affects(texture->GetPath()))
		return false;
	return PrepareTargetPathRelocation(relocation);
}

void SpriteEditor::OpenInSpriteMode(const std::filesystem::path& path)
{
	Focus();
	if (SameResourceEditorPath(m_TargetResourcePath, path) && !IsAsyncLoading())
		return;
	RunWithDiscardConfirmation([this, path] {
		if (!AsyncResourceOpenService::Get().RequestOpen(this, path))
			SetTargetResource(path);
	});
}

void SpriteEditor::OpenTextureDialog()
{
	std::wstring selectedPath;
	const std::vector<FileDialogFilter> filters{ { Engine::Unicode::Utf8ToWide(LOCAL("FileDialog.ImageFiles")), L"*.png;*.jpg;*.jpeg;*.tga;*.bmp;*.dds;*.bamsprite;*.bamtex" } };
	if (FileDialogs::OpenFileDialog(selectedPath, filters, L"", EFileDialogContext::SpriteOpen))
	{
		const std::filesystem::path targetPath(selectedPath);
		if (!AsyncResourceOpenService::Get().RequestOpen(this, targetPath))
			SetTargetResource(targetPath);
	}
}

void SpriteEditor::ReleaseResourceDocument()
{
	CancelAsyncOpen();
	m_TextureHandle.Reset();
	m_Slices.clear();
	m_SelectedSliceIndex = -1;
	m_History.Clear();
	m_ShowSliceModal = false;
	m_ShowCreateAnimModal = false;
	m_Zoom = 1.0f;
	m_PanOffset = {};
}

void SpriteEditor::SetTargetResource(const std::filesystem::path& path)
{
	CancelAsyncOpen();
	const auto* textureEditor = dynamic_cast<TextureEditor*>(ImGuiManager::Get().GetImGuiPanel(L"Texture Editor"));
	if (textureEditor && textureEditor->IsConversionOutput(path))
	{
		NotificationManager::Get().AddNotification(GetDisplayName(),
			LOCAL("TextureEditor.ConversionTargetBusy"), ELogLevel::Warn);
		return;
	}
	ResourceHandle<Engine::Texture> textureHandle;
	std::vector<SpriteSliceInfo> slices;

	LoadSpriteOrTextureResource(path, textureHandle, slices);
	ScanOrCreateDefaultSlices(path, textureHandle, slices);

	if (textureEditor && textureHandle.Get() && textureEditor->IsConversionOutput(textureHandle.Get()->GetPath()))
	{
		NotificationManager::Get().AddNotification(GetDisplayName(),
			LOCAL("TextureEditor.ConversionTargetBusy"), ELogLevel::Warn);
		return;
	}
	if (!textureHandle.IsValid() || !textureHandle.Get() || !textureHandle.Get()->GetRHITexture())
	{
		NotificationManager::Get().AddNotification(GetDisplayName(),
			LOCAL_FMT("Notification.ResourceLoadFailed", Engine::Unicode::WideToUtf8(path.filename().wstring())), ELogLevel::Error);
		return;
	}

	ReleaseResourceDocument();
	m_TargetResourcePath = path;
	m_TextureHandle = std::move(textureHandle);
	m_Slices = std::move(slices);
	m_History.Reset(m_Slices);
	m_SelectedSliceIndex = m_Slices.empty() ? -1 : 0;
}

bool SpriteEditor::BeginAsyncOpen(const std::filesystem::path& assetPath)
{
	if (!IsSupported(assetPath))
		return false;

	const auto* textureEditor = dynamic_cast<TextureEditor*>(ImGuiManager::Get().GetImGuiPanel(L"Texture Editor"));
	if (textureEditor && textureEditor->IsConversionOutput(assetPath))
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

bool SpriteEditor::StepAsyncOpen(float maxTimeSeconds)
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
		const auto* textureEditor = dynamic_cast<TextureEditor*>(ImGuiManager::Get().GetImGuiPanel(L"Texture Editor"));
		if (textureEditor && textureEditor->IsConversionOutput(m_AsyncTargetPath))
		{
			NotificationManager::Get().AddNotification(GetDisplayName(),
				LOCAL("TextureEditor.ConversionTargetBusy"), ELogLevel::Warn);
			m_AsyncPendingTexture.Reset();
			m_AsyncPendingSlices.clear();
			m_AsyncTargetPath.clear();
			m_AsyncProgress = 0.0f;
			m_AsyncStage = EAsyncStage::None;
			return true;
		}

		ResourceHandle<Engine::Texture> textureHandle;
		std::vector<SpriteSliceInfo> slices;
		LoadSpriteOrTextureResource(m_AsyncTargetPath, textureHandle, slices);

		if (textureEditor && textureHandle.Get() && textureEditor->IsConversionOutput(textureHandle.Get()->GetPath()))
		{
			NotificationManager::Get().AddNotification(GetDisplayName(),
				LOCAL("TextureEditor.ConversionTargetBusy"), ELogLevel::Warn);
			m_AsyncPendingTexture.Reset();
			m_AsyncPendingSlices.clear();
			m_AsyncTargetPath.clear();
			m_AsyncProgress = 0.0f;
			m_AsyncStage = EAsyncStage::None;
			return true;
		}
		if (!textureHandle.IsValid() || !textureHandle.Get() || !textureHandle.Get()->GetRHITexture())
		{
			NotificationManager::Get().AddNotification(GetDisplayName(),
				LOCAL_FMT("Notification.ResourceLoadFailed", Engine::Unicode::WideToUtf8(m_AsyncTargetPath.filename().wstring())), ELogLevel::Error);
			m_AsyncPendingTexture.Reset();
			m_AsyncPendingSlices.clear();
			m_AsyncTargetPath.clear();
			m_AsyncProgress = 0.0f;
			m_AsyncStage = m_TextureHandle.Get() ? EAsyncStage::None : EAsyncStage::Failed;
			return true;
		}

		m_AsyncPendingTexture = std::move(textureHandle);
		m_AsyncPendingSlices = std::move(slices);
		m_AsyncLoadDurationMs = Milliseconds(Clock::now() - m_AsyncLoadStart).count();
		m_AsyncStage = EAsyncStage::ScanningSlices;
		m_AsyncProgress = 0.70f;

		if (Clock::now() >= stepDeadline)
			return false;
	}

	if (m_AsyncStage == EAsyncStage::ScanningSlices)
	{
		if (!RestoreSavedChanges())
		{
			m_AsyncPendingTexture.Reset();
			m_AsyncPendingSlices.clear();
			m_AsyncTargetPath.clear();
			m_AsyncProgress = 0.0f;
			m_AsyncStage = EAsyncStage::None;
			return true;
		}

		const auto scanStart = Clock::now();
		auto targetPath = std::move(m_AsyncTargetPath);
		auto pendingTexture = std::move(m_AsyncPendingTexture);
		auto pendingSlices = std::move(m_AsyncPendingSlices);
		ScanOrCreateDefaultSlices(targetPath, pendingTexture, pendingSlices);

		ReleaseResourceDocument();
		m_TargetResourcePath = targetPath;
		m_TextureHandle = std::move(pendingTexture);
		m_Slices = std::move(pendingSlices);
		m_History.Reset(m_Slices);
		m_SelectedSliceIndex = m_Slices.empty() ? -1 : 0;

		const auto scanEnd = Clock::now();
		const double scanMs = Milliseconds(scanEnd - scanStart).count();
		const double totalMs = Milliseconds(scanEnd - m_AsyncStartTime).count();

		EDITOR_LOG_INFO("[ResourceOpenTiming] Sprite {}: resource={:.2f}ms, slices={:.2f}ms, total={:.2f}ms (async)",
			Engine::Unicode::WideToUtf8(targetPath.wstring()), m_AsyncLoadDurationMs, scanMs, totalMs);

		m_AsyncProgress = 1.0f;
		m_AsyncStage = EAsyncStage::None;
		return true;
	}

	return true;
}

void SpriteEditor::CancelAsyncOpen()
{
	AsyncResourceOpenService::Get().CancelRequestsForEditor(this);
	m_AsyncStage = EAsyncStage::None;
	m_AsyncProgress = 0.0f;
	m_AsyncTargetPath.clear();
	m_AsyncPendingTexture.Reset();
	m_AsyncPendingSlices.clear();
}

void SpriteEditor::DrawAsyncLoadingOverlay()
{
	DrawStandardAsyncLoadingOverlay(m_AsyncTargetPath, m_AsyncProgress,
		m_AsyncStage == EAsyncStage::WaitingBackground, m_AsyncStage == EAsyncStage::Failed);
}

void SpriteEditor::FitView(float canvasW, float canvasH, float texW, float texH)
{
	if (texW <= 0.0f || texH <= 0.0f || canvasW <= 0.0f || canvasH <= 0.0f) return;
	const float margin = 40.0f;
	const float scaleX = (canvasW - margin) / texW;
	const float scaleY = (canvasH - margin) / texH;
	m_Zoom = std::clamp((std::min)(scaleX, scaleY), 0.1f, 16.0f);
	m_PanOffset = { 0.0f, 0.0f };
}

void SpriteEditor::DrawCustomContent()
{
	const ImVec2 contentAvail = ImGui::GetContentRegionAvail();
	const float splitterThickness = Editor::SplitterThickness;
	const float spacing = ImGui::GetStyle().ItemSpacing.x;
	const float overhead = splitterThickness + spacing * 2.0f;

	if (contentAvail.x > overhead + 2.0f && contentAvail.y > 0.0f)
	{
		const float paneWidth = contentAvail.x - overhead;
		const float minSidebarW = (std::min)(220.0f, paneWidth * 0.5f);
		const float minCanvasW = (std::min)(200.0f, paneWidth - minSidebarW);
		float sidebarWidth = std::clamp(m_SidebarWidth, minSidebarW, paneWidth - minCanvasW);
		float canvasWidth = paneWidth - sidebarWidth;
		const float displayedSidebarWidth = sidebarWidth;

		DrawCanvas(canvasWidth, contentAvail.y);
		ImGui::SameLine();

		if (SplitterV("##SpriteEditorSplitter", &canvasWidth, &sidebarWidth, minCanvasW, minSidebarW,
			contentAvail.y, splitterThickness) && sidebarWidth != displayedSidebarWidth)
		{
			m_SidebarWidth = sidebarWidth;
		}
		ImGui::SameLine();

		ImGui::BeginDisabled(Engine::PlatformPath::IsReadOnlyTarget(m_TargetResourcePath));
		DrawSidebar(displayedSidebarWidth, contentAvail.y);
		ImGui::EndDisabled();
	}

	// 팝업 모달
	DrawSliceModal();
	DrawCreateAnimationModal();
	if (!IsHistoryGestureActive()) CommitDocumentEdit();
}

bool SpriteEditor::CommitDocumentEdit()
{
	return m_History.Record(m_Slices, [this](const auto& value) { return ApplyDocumentSnapshot(value); },
		LOCAL("History.Command.EditProperty"));
}

bool SpriteEditor::ApplyDocumentSnapshot(const std::vector<SpriteSliceInfo>& snapshot)
{
	m_Slices = snapshot;
	m_SelectedSliceIndex = m_Slices.empty() ? -1 : std::clamp(m_SelectedSliceIndex, 0, static_cast<int>(m_Slices.size()) - 1);
	return true;
}

bool SpriteEditor::CanUndoDocument() const
{
	return !IsAsyncLoading() && m_TextureHandle.Get() && m_History.CanUndo() && (!m_History.HasPendingEdit() || !IsHistoryGestureActive());
}

bool SpriteEditor::CanRedoDocument() const
{
	return !IsAsyncLoading() && m_TextureHandle.Get() && m_History.CanRedo();
}

void SpriteEditor::UndoDocument()
{
	if (CanUndoDocument() && CommitDocumentEdit()) m_History.Undo();
}

void SpriteEditor::RedoDocument()
{
	if (CanRedoDocument()) m_History.Redo();
}

void SpriteEditor::DrawCustomToolbar()
{
	const bool loading = IsAsyncLoading();
	if (ImGui::BeginMenu(LOCAL_CSTR("EditorMenu.File")))
	{
		if (ImGui::MenuItem((std::string(ICON_FA_FOLDER_OPEN) + " " + LOCAL("SpriteEditor.OpenTexture")).c_str(), nullptr, false, !loading))
			RunWithDiscardConfirmation([this] { OpenTextureDialog(); });
		ImGui::Separator();
		DrawSaveMenuItem();
		ImGui::Separator();
		DrawCloseMenuItem();
		ImGui::EndMenu();
	}

	ImGui::BeginDisabled(loading || !m_TextureHandle.Get() ||
		Engine::PlatformPath::IsReadOnlyTarget(m_TargetResourcePath));
	if (ImGui::BeginMenu(LOCAL_CSTR("SpriteEditor.SliceMethod")))
	{
		if (ImGui::MenuItem(LOCAL_CSTR("SpriteEditor.SliceModalTitle")))
		{
			m_ShowSliceModal = true;
		}
		if (ImGui::MenuItem(LOCAL_CSTR("SpriteEditor.ClearSlices"), nullptr, false, !m_Slices.empty()))
		{
			m_Slices.clear();
			m_SelectedSliceIndex = -1;
			m_History.MarkEdited();
		}
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu(LOCAL_CSTR("ResourceEditor.SpriteAnimationTitle")))
	{
		if (ImGui::MenuItem(LOCAL_CSTR("SpriteEditor.CreateAnimModalTitle"), nullptr, false, !m_Slices.empty()))
		{
			m_ShowCreateAnimModal = true;
		}
		ImGui::EndMenu();
	}
	ImGui::EndDisabled();

	if (ImGui::BeginMenu(LOCAL_LABEL("EditorMenu.Edit").c_str()))
	{
		DrawHistoryMenuItems("SpriteEditor");
		ImGui::EndMenu();
	}
}

void SpriteEditor::DrawCustomQuickAccess()
{
	if (IsAsyncLoading()) return;
	EditorUI::ToolbarLayout toolbar;
	if (toolbar.Button("SpriteEditor.OpenTexture", ICON_FA_FOLDER_OPEN, "SpriteEditor.OpenTexture"))
		RunWithDiscardConfirmation([this] { OpenTextureDialog(); });

	EditorUI::IconButtonOptions copyOptions;
	copyOptions.Enabled = !m_TargetResourcePath.empty();
	if (toolbar.Icon("CopyResourcePath", ICON_FA_COPY, "EditorCommon.CopyPath", copyOptions))
		ImGui::SetClipboardText(Engine::Unicode::WideToUtf8(m_TargetResourcePath.wstring()).c_str());
	DrawSaveButton(toolbar);
	DrawHistoryButtons(toolbar, "SpriteEditor");

	ImGui::BeginDisabled(!m_TextureHandle.Get() ||
		Engine::PlatformPath::IsReadOnlyTarget(m_TargetResourcePath));
	if (toolbar.Button("SpriteEditor.SliceModalTitle", ICON_FA_SCISSORS, "SpriteEditor.SliceModalTitle"))
	{
		m_ShowSliceModal = true;
	}

	ImGui::BeginDisabled(m_Slices.empty());
	if (toolbar.Button("SpriteEditor.CreateAnimModalTitle", ICON_FA_FILM, "SpriteEditor.CreateAnimModalTitle"))
	{
		m_ShowCreateAnimModal = true;
	}
	ImGui::EndDisabled();

	ImGui::EndDisabled();

	auto* texture = m_TextureHandle.Get();
	auto* rhiTexture = texture ? texture->GetRHITexture() : nullptr;
	const float texW = rhiTexture ? static_cast<float>(rhiTexture->GetWidth()) : 0.0f;
	const float texH = rhiTexture ? static_cast<float>(rhiTexture->GetHeight()) : 0.0f;

	ImGui::BeginDisabled(!m_TextureHandle.Get());
	if (toolbar.Button("SpriteEditor.FitView", ICON_FA_EXPAND, "SpriteEditor.FitView"))
	{
		const ImVec2 contentAvail = ImGui::GetContentRegionAvail();
		FitView(contentAvail.x - m_SidebarWidth - 10.0f, contentAvail.y, texW, texH);
	}

	if (toolbar.Button("SpriteEditor.OneToOne", ICON_FA_MAGNIFYING_GLASS, "SpriteEditor.OneToOne"))
	{
		m_Zoom = 1.0f;
		m_PanOffset = { 0.0f, 0.0f };
	}

	toolbar.SetNextItemWidth(ImGui::GetFontSize() * 6.0f);
	ImGui::SliderFloat("##Zoom", &m_Zoom, 0.1f, 8.0f, "%.1fx");
	ImGui::EndDisabled();


	// 상태 배지
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 1.0f, 1.0f));
	toolbar.Text(LOCAL_FMT("SpriteEditor.SliceCountFormat", m_Slices.size()).c_str(), false);
	ImGui::PopStyleColor();

	if (rhiTexture)
	{
		ImGuiTextBuffer resolution;
		resolution.appendf(LOCAL_CSTR("SpriteEditor.ResolutionFormat"),
			rhiTexture->GetWidth(), rhiTexture->GetHeight());
		toolbar.Text(resolution.c_str());
	}
	if (!m_TargetResourcePath.empty() && Engine::PlatformPath::IsReadOnlyTarget(m_TargetResourcePath))
		toolbar.Text((std::string(ICON_FA_LOCK) + " " + LOCAL("ContentBrowser.EngineReadOnlyBadge")).c_str());
}

void SpriteEditor::DrawCanvas(float availWidth, float availHeight)
{
	ImGui::BeginChild("##SpriteCanvasChild", ImVec2(availWidth, availHeight), true,
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

	const ImVec2 canvasPos = ImGui::GetCursorScreenPos();
	const ImVec2 canvasSize = ImGui::GetContentRegionAvail();
	ImDrawList* drawList = ImGui::GetWindowDrawList();

	// 1. 체커보드 배경 렌더링
	const ImU32 colBg1 = IM_COL32(32, 32, 32, 255);
	const ImU32 colBg2 = IM_COL32(44, 44, 44, 255);
	const float checkSize = 16.0f;
	drawList->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), colBg1);
	for (float y = 0.0f; y < canvasSize.y; y += checkSize)
	{
		for (float x = 0.0f; x < canvasSize.x; x += checkSize)
		{
			if (static_cast<int>((x / checkSize) + (y / checkSize)) % 2 == 1)
			{
				drawList->AddRectFilled(
					ImVec2(canvasPos.x + x, canvasPos.y + y),
					ImVec2((std::min)(canvasPos.x + x + checkSize, canvasPos.x + canvasSize.x),
						(std::min)(canvasPos.y + y + checkSize, canvasPos.y + canvasSize.y)),
					colBg2);
			}
		}
	}

	// 2. 마우스 줌 & 팬 (InputManager 일원화)
	auto& input = Engine::InputManager::Get();
	if (ImGui::IsWindowHovered())
	{
		if (input.IsMouseButtonDragging(Engine::EMouseButton::Right) || input.IsMouseButtonDragging(Engine::EMouseButton::Middle))
		{
			const auto delta = input.GetMouseDelta();
			m_PanOffset.x += delta.x;
			m_PanOffset.y += delta.y;
		}
		const float wheel = input.GetMouseScrollDeltaY();
		if (wheel != 0.0f)
		{
			m_Zoom = std::clamp(m_Zoom + wheel * 0.2f, 0.1f, 16.0f);
		}
	}

	auto* texture = m_TextureHandle.Get();
	auto* rhiTexture = texture ? texture->GetRHITexture() : nullptr;

	if (rhiTexture)
	{
		ImTextureID texID = ImGuiManager::Get().GetImGuiTextureID(rhiTexture);
		const float texW = static_cast<float>(rhiTexture->GetWidth());
		const float texH = static_cast<float>(rhiTexture->GetHeight());

		const ImVec2 center(canvasPos.x + canvasSize.x * 0.5f + m_PanOffset.x, canvasPos.y + canvasSize.y * 0.5f + m_PanOffset.y);
		const ImVec2 texMin(center.x - texW * 0.5f * m_Zoom, center.y - texH * 0.5f * m_Zoom);
		const ImVec2 texMax(texMin.x + texW * m_Zoom, texMin.y + texH * m_Zoom);

		// 텍스처 본체 드로우
		drawList->AddImage(texID, texMin, texMax, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f));
		drawList->AddRect(texMin, texMax, IM_COL32(100, 100, 100, 180), 0.0f, 0, 1.0f);

		// 마우스 클릭으로 슬라이스 선택 판정 (InputManager 기반)
		const vec2 mousePos = input.GetMousePosition();
		const bool isMouseClicked = input.IsMouseButtonDown(Engine::EMouseButton::Left) && ImGui::IsWindowHovered();

		// 3. 슬라이스 박스 렌더링
		for (int i = 0; i < static_cast<int>(m_Slices.size()); ++i)
		{
			const auto& slice = m_Slices[i];
			const bool isSelected = (i == m_SelectedSliceIndex);

			const ImVec2 sMin(texMin.x + slice.region.left * m_Zoom, texMin.y + slice.region.top * m_Zoom);
			const ImVec2 sMax(sMin.x + slice.region.width * m_Zoom, sMin.y + slice.region.height * m_Zoom);

			// 클릭 히트 테스트
			if (isMouseClicked && mousePos.x >= sMin.x && mousePos.x <= sMax.x &&
				mousePos.y >= sMin.y && mousePos.y <= sMax.y)
			{
				m_SelectedSliceIndex = i;
			}

			if (isSelected)
			{
				// 선택된 슬라이스: 노란색 테두리 + 반투명 채우기
				drawList->AddRectFilled(sMin, sMax, IM_COL32(255, 230, 40, 40));
				drawList->AddRect(sMin, sMax, IM_COL32(255, 230, 40, 255), 0.0f, 0, 2.0f);

				// 피벗 십자선
				const ImVec2 pivotPos(sMin.x + slice.region.width * m_Zoom * slice.pivot.x,
					sMax.y - slice.region.height * m_Zoom * slice.pivot.y);
				drawList->AddLine(ImVec2(pivotPos.x - 8.0f, pivotPos.y), ImVec2(pivotPos.x + 8.0f, pivotPos.y), IM_COL32(255, 60, 60, 255), 2.0f);
				drawList->AddLine(ImVec2(pivotPos.x, pivotPos.y - 8.0f), ImVec2(pivotPos.x, pivotPos.y + 8.0f), IM_COL32(60, 255, 60, 255), 2.0f);

				// 9-Slice 보더 가이드라인 (Border L, R, T, B)
				if (slice.border.x > 0.0f || slice.border.y > 0.0f || slice.border.z > 0.0f || slice.border.w > 0.0f)
				{
					const float bL = sMin.x + slice.border.x * m_Zoom;
					const float bR = sMax.x - slice.border.y * m_Zoom;
					const float bT = sMin.y + slice.border.z * m_Zoom;
					const float bB = sMax.y - slice.border.w * m_Zoom;

					drawList->AddLine(ImVec2(bL, sMin.y), ImVec2(bL, sMax.y), IM_COL32(80, 200, 255, 200), 1.0f);
					drawList->AddLine(ImVec2(bR, sMin.y), ImVec2(bR, sMax.y), IM_COL32(80, 200, 255, 200), 1.0f);
					drawList->AddLine(ImVec2(sMin.x, bT), ImVec2(sMax.x, bT), IM_COL32(80, 200, 255, 200), 1.0f);
					drawList->AddLine(ImVec2(sMin.x, bB), ImVec2(sMax.x, bB), IM_COL32(80, 200, 255, 200), 1.0f);
				}
			}
			else
			{
				// 비선택 슬라이스: 얇은 녹색 테두리
				drawList->AddRect(sMin, sMax, IM_COL32(60, 220, 60, 180), 0.0f, 0, 1.0f);
			}
		}
	}
	else
	{
		ImGui::SetCursorPos(ImVec2(20.0f, 20.0f));
		ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", LOCAL_CSTR("SpriteEditor.NoTextureLoaded"));

		// 화면 중앙에 점선 드롭 가이드 및 텍스처 열기 버튼 제공
		const ImVec2 centerBoxMin(canvasPos.x + 40.0f, canvasPos.y + 40.0f);
		const ImVec2 centerBoxMax(canvasPos.x + canvasSize.x - 40.0f, canvasPos.y + canvasSize.y - 40.0f);
		drawList->AddRect(centerBoxMin, centerBoxMax, IM_COL32(120, 120, 120, 150), 8.0f, ImDrawFlags_None, 1.5f);

		EditorUI::TextButtonOptions openBtnOpt;
		openBtnOpt.Icon = ICON_FA_FOLDER_OPEN;
		openBtnOpt.MinWidth = ImGui::GetFontSize() * 10.0f;
		openBtnOpt.Height = ImGui::GetFrameHeight() * 1.35f;
		const std::string openText = LOCAL("SpriteEditor.OpenTexture");
		const ImVec2 btnSize = EditorUI::GetTextButtonSize(openText.c_str(), openBtnOpt);
		ImGui::SetCursorScreenPos(ImVec2(canvasPos.x + (canvasSize.x - btnSize.x) * 0.5f, canvasPos.y + canvasSize.y * 0.5f - btnSize.y));
		if (EditorUI::TextButtonRaw("OpenTextureEmpty", openText.c_str(), openBtnOpt))
			RunWithDiscardConfirmation([this] { OpenTextureDialog(); });

		ImGui::SetCursorScreenPos(ImVec2(canvasPos.x + (canvasSize.x - 320.0f) * 0.5f, canvasPos.y + canvasSize.y * 0.5f + 12.0f));
		ImGui::TextDisabled("%s %s", ICON_FA_ARROW_DOWN, LOCAL_CSTR("TilePalette.NoSprites"));
	}

	// 캔버스 영역에서 에셋 드래그 앤 드롭 수신
	if (ImGui::BeginDragDropTarget())
	{
		const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM");
		if (!payload) payload = ImGui::AcceptDragDropPayload("ASSET_PATH");
		if (payload)
		{
			const std::filesystem::path droppedPath(static_cast<const wchar_t*>(payload->Data));
			if (IsSupported(droppedPath))
				RunWithDiscardConfirmation([this, droppedPath] {
					if (!AsyncResourceOpenService::Get().RequestOpen(this, droppedPath))
						SetTargetResource(droppedPath);
				});
		}
		ImGui::EndDragDropTarget();
	}

	ImGui::EndChild();
}

void SpriteEditor::DrawSidebar(float availWidth, float availHeight)
{
	ImGui::BeginChild("##SpriteSidebarChild", ImVec2(availWidth, availHeight), true);

	// 1. 선택된 슬라이스 속성 인스펙터
	if (ImGui::CollapsingHeader(LOCAL_LABEL("SpriteEditor.SelectedSlice").c_str(), ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (m_SelectedSliceIndex >= 0 && m_SelectedSliceIndex < static_cast<int>(m_Slices.size()))
		{
			auto& slice = m_Slices[m_SelectedSliceIndex];

			// 잘린 슬라이스 영역 실시간 썸네일 프리뷰 (원본 종횡비 유지)
			if (auto* tex = m_TextureHandle.Get())
			{
				if (auto* rhiTex = tex->GetRHITexture())
				{
					const float texW = static_cast<float>(rhiTex->GetWidth());
					const float texH = static_cast<float>(rhiTex->GetHeight());
					if (texW > 0.0f && texH > 0.0f)
					{
						const float cropW = (slice.region.width > 0.0f) ? slice.region.width : texW;
						const float cropH = (slice.region.height > 0.0f) ? slice.region.height : texH;
						const ImVec2 uv0(slice.region.left / texW, slice.region.top / texH);
						const ImVec2 uv1((slice.region.left + cropW) / texW, (slice.region.top + cropH) / texH);

						const float boxSize = 96.0f;
						const float scale = boxSize / (std::max)(cropW, cropH);
						const ImVec2 fitSize(cropW * scale, cropH * scale);

						const float availX = ImGui::GetContentRegionAvail().x;
						const ImVec2 boxMin(ImGui::GetCursorScreenPos().x + (availX - boxSize) * 0.5f, ImGui::GetCursorScreenPos().y);
						const ImVec2 boxMax(boxMin.x + boxSize, boxMin.y + boxSize);
						const ImVec2 imgMin(boxMin.x + (boxSize - fitSize.x) * 0.5f, boxMin.y + (boxSize - fitSize.y) * 0.5f);
						const ImVec2 imgMax(imgMin.x + fitSize.x, imgMin.y + fitSize.y);

						ImDrawList* dl = ImGui::GetWindowDrawList();
						dl->AddRectFilled(boxMin, boxMax, IM_COL32(30, 30, 30, 255), 4.0f);
						ImTextureID texID = ImGuiManager::Get().GetImGuiTextureID(rhiTex);
						dl->AddImage(texID, imgMin, imgMax, uv0, uv1);
						dl->AddRect(boxMin, boxMax, IM_COL32(100, 100, 100, 180), 4.0f);
						ImGui::Dummy(ImVec2(availX, boxSize + 6.0f));
					}
				}
			}

			if (EditorUI::BeginPropertyTable("##SliceProperties", EditorUI::InspectorPropertyTableStyle()))
			{
				EditorUI::PropertyRow("SpriteEditor.ClipName");
				char nameBuf[128];
				strncpy_s(nameBuf, slice.name.c_str(), sizeof(nameBuf));
				ImGui::SetNextItemWidth(-FLT_MIN);
				if (ImGui::InputText("##SliceName", nameBuf, sizeof(nameBuf)))
				{
					slice.name = nameBuf;
					m_History.MarkEdited();
				}

				float rectVal[4] = { slice.region.left, slice.region.top, slice.region.width, slice.region.height };
				EditorUI::PropertyRow("SpriteEditor.XYLabel");
				ImGui::SetNextItemWidth(-FLT_MIN);
				if (ImGui::DragFloat2("##SlicePosition", rectVal, 1.0f, 0.0f, 8192.0f, "%.0f px"))
				{
					slice.region.left = rectVal[0];
					slice.region.top = rectVal[1];
					m_History.MarkEdited();
				}
				EditorUI::PropertyRow("SpriteEditor.WHLabel");
				ImGui::SetNextItemWidth(-FLT_MIN);
				if (ImGui::DragFloat2("##SliceSize", &rectVal[2], 1.0f, 1.0f, 8192.0f, "%.0f px"))
				{
					slice.region.width = rectVal[2];
					slice.region.height = rectVal[3];
					m_History.MarkEdited();
				}

				EditorUI::PropertyRow("SpriteEditor.PivotLabel");
				ImGui::SetNextItemWidth(-FLT_MIN);
				if (ImGui::SliderFloat2("##SlicePivot", &slice.pivot.x, 0.0f, 1.0f, "%.2f"))
					m_History.MarkEdited();

				EditorUI::PropertyRow("SpriteEditor.PivotPresets");
				EditorUI::ToolbarLayout presets;
				if (presets.Icon("PivotCenter", ICON_FA_CROSSHAIRS, "SpriteEditor.PivotCenter")) { slice.pivot = { 0.5f, 0.5f }; m_History.MarkEdited(); }
				if (presets.Icon("PivotBottom", ICON_FA_ARROW_DOWN, "SpriteEditor.PivotBottom")) { slice.pivot = { 0.5f, 0.0f }; m_History.MarkEdited(); }
				if (presets.Icon("PivotTop", ICON_FA_ARROW_UP, "SpriteEditor.PivotTop")) { slice.pivot = { 0.5f, 1.0f }; m_History.MarkEdited(); }
				if (presets.Icon("PivotLeft", ICON_FA_ARROW_LEFT, "SpriteEditor.PivotLeft")) { slice.pivot = { 0.0f, 0.5f }; m_History.MarkEdited(); }
				if (presets.Icon("PivotRight", ICON_FA_ARROW_RIGHT, "SpriteEditor.PivotRight")) { slice.pivot = { 1.0f, 0.5f }; m_History.MarkEdited(); }

				float borderVal[4] = { slice.border.x, slice.border.y, slice.border.z, slice.border.w };
				EditorUI::PropertyRow("SpriteEditor.BorderHorizontal");
				ImGui::SetNextItemWidth(-FLT_MIN);
				if (ImGui::DragFloat2("##SliceBorderHorizontal", borderVal, 1.0f, 0.0f, 512.0f, "%.0f px"))
				{
					slice.border.x = borderVal[0];
					slice.border.y = borderVal[1];
					m_History.MarkEdited();
				}
				EditorUI::PropertyRow("SpriteEditor.BorderVertical");
				ImGui::SetNextItemWidth(-FLT_MIN);
				if (ImGui::DragFloat2("##SliceBorderVertical", &borderVal[2], 1.0f, 0.0f, 512.0f, "%.0f px"))
				{
					slice.border.z = borderVal[2];
					slice.border.w = borderVal[3];
					m_History.MarkEdited();
				}
				EditorUI::EndPropertyTable();
			}

			ImGui::Spacing();
			EditorUI::ToolbarLayout actions;
			if (actions.Button("DeleteSlice", ICON_FA_TRASH, "SpriteEditor.DeleteSlice"))
			{
				m_Slices.erase(m_Slices.begin() + m_SelectedSliceIndex);
				if (m_SelectedSliceIndex >= static_cast<int>(m_Slices.size()))
					m_SelectedSliceIndex = static_cast<int>(m_Slices.size()) - 1;
				m_History.MarkEdited();
			}
		}
		else
		{
			ImGui::TextDisabled("%s", LOCAL_CSTR("SpriteEditor.NoSliceSelected"));
		}
	}

	ImGui::Spacing();

	// 2. 슬라이스 전체 목록 테이블
	if (ImGui::CollapsingHeader(LOCAL_LABEL("SpriteEditor.SliceList").c_str(), ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (m_Slices.empty())
		{
			ImGui::TextWrapped("%s", LOCAL_CSTR("SpriteEditor.NoSlices"));
		}
		else
		{
			// 표준 검색 바 (EditorUI::DrawSearchBar) 도입
			EditorUI::DrawSearchBar("SpriteSliceSearch", m_SliceSearch, sizeof(m_SliceSearch), "SpriteEditor.SearchSlices");
			ImGui::Spacing();

			const std::string filterLower = Engine::ToLowerStr(m_SliceSearch);

			ImGui::BeginChild("##SliceListScroll", ImVec2(0.0f, 260.0f), false);
			bool hasMatches = false;
			if (EditorUI::BeginSettingsTable("##SlicesTable", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter))
			{
				ImGui::TableSetupColumn(LOCAL_LABEL("SpriteEditor.IndexHeader").c_str(), ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 2.0f);
				ImGui::TableSetupColumn(LOCAL_LABEL("SpriteEditor.NameHeader").c_str(), ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn(LOCAL_LABEL("SpriteEditor.SizeHeader").c_str(), ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 5.5f);
				ImGui::TableHeadersRow();

				for (int i = 0; i < static_cast<int>(m_Slices.size()); ++i)
				{
					const auto& slice = m_Slices[i];

					// 슬라이스 이름 및 인덱스 검색 필터링
					if (!filterLower.empty())
					{
						const bool matchName = Engine::ToLowerStr(slice.name).find(filterLower) != std::string::npos;
						const bool matchIdx = std::to_string(i).find(filterLower) != std::string::npos;
						if (!matchName && !matchIdx) continue;
					}

					hasMatches = true;
					ImGui::TableNextRow();

					ImGui::TableSetColumnIndex(0);
					ImGui::Text("%d", i);

					ImGui::TableSetColumnIndex(1);
					const bool isSelected = (i == m_SelectedSliceIndex);
					const bool clipped = ImGui::CalcTextSize(slice.name.c_str()).x > ImGui::GetContentRegionAvail().x;
					ImGui::PushID(i);
					if (ImGui::Selectable(slice.name.c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns))
					{
						m_SelectedSliceIndex = i;
					}
					if (clipped && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", slice.name.c_str());
					ImGui::PopID();

					ImGui::TableSetColumnIndex(2);
					ImGui::Text(LOCAL_CSTR("SpriteEditor.SizeFormat"), slice.region.width, slice.region.height);
				}
				EditorUI::EndSettingsTable();
				if (!hasMatches) EditorUI::EmptyState("SpriteEditor.NoSearchResults");
			}
			ImGui::EndChild();
		}
	}

	ImGui::EndChild();
}

void SpriteEditor::DrawSliceModal()
{
	if (!m_ShowSliceModal) return;

	ImGui::OpenPopup(LOCAL_CSTR("SpriteEditor.SliceModalTitle"));
	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(380.0f, 320.0f), ImGuiCond_Appearing);

	if (ImGui::BeginPopupModal(LOCAL_CSTR("SpriteEditor.SliceModalTitle"), &m_ShowSliceModal, ImGuiWindowFlags_AlwaysAutoResize))
	{
		const char* methods[] = {
			LOCAL_CSTR("SpriteEditor.GridByCellSize"),
			LOCAL_CSTR("SpriteEditor.GridByCellCount"),
			LOCAL_CSTR("SpriteEditor.Automatic")
		};
		int currentMethod = static_cast<int>(m_SliceParams.method);
		if (ImGui::Combo(LOCAL_CSTR("SpriteEditor.SliceMethod"), &currentMethod, methods, IM_ARRAYSIZE(methods)))
		{
			m_SliceParams.method = static_cast<ESpriteSliceMethod>(currentMethod);
		}

		ImGui::Separator();

		if (m_SliceParams.method == ESpriteSliceMethod::GridByCellSize)
		{
			ImGui::DragInt2(LOCAL_CSTR("SpriteEditor.CellSize"), &m_SliceParams.cellSizeX, 1, 4, 2048);
		}
		else if (m_SliceParams.method == ESpriteSliceMethod::GridByCellCount)
		{
			ImGui::DragInt2(LOCAL_CSTR("SpriteEditor.CellCount"), &m_SliceParams.cellCountCols, 1, 1, 128);
		}
		else
		{
			ImGui::TextDisabled("%s", LOCAL_CSTR("SpriteEditor.AutoDesc"));
			int alphaVal = static_cast<int>(m_SliceParams.alphaThreshold);
			if (ImGui::SliderInt(LOCAL_CSTR("SpriteEditor.AlphaThreshold"), &alphaVal, 0, 255))
			{
				m_SliceParams.alphaThreshold = static_cast<uint8>(alphaVal);
			}
			ImGui::DragInt(LOCAL_CSTR("SpriteEditor.MinPixelArea"), &m_SliceParams.minPixelArea, 1, 1, 1000);
		}

		ImGui::DragInt2(LOCAL_CSTR("SpriteEditor.Offset"), &m_SliceParams.offsetX, 1, 0, 1024);
		ImGui::DragInt2(LOCAL_CSTR("SpriteEditor.Padding"), &m_SliceParams.paddingX, 1, 0, 256);

		ImGui::SliderFloat2(LOCAL_CSTR("SpriteEditor.DefaultPivot"), &m_SliceParams.defaultPivot.x, 0.0f, 1.0f, "%.2f");
		ImGui::DragFloat(LOCAL_LABEL("SpriteEditor.PixelsPerUnit").c_str(), &m_SliceParams.pixelPerUnit, 1.0f, 1.0f, 1000.0f, "%.0f");

		ImGui::Separator();
		ImGui::Spacing();

		const EditorUI::DialogAction actions[] = {
			{ "EditorCommon.Cancel" }, { "SpriteEditor.ExecuteSlice" }
		};
		const int action = EditorUI::DialogButtonBar("SliceActions", actions, IM_ARRAYSIZE(actions));
		if (action == 1)
		{
			ExecuteSlice(m_SliceParams);
			m_ShowSliceModal = false;
			ImGui::CloseCurrentPopup();
		}

		if (action == 0)
		{
			m_ShowSliceModal = false;
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}

void SpriteEditor::DrawCreateAnimationModal()
{
	if (!m_ShowCreateAnimModal) return;

	ImGui::OpenPopup(LOCAL_CSTR("SpriteEditor.CreateAnimModalTitle"));
	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(420.0f, 320.0f), ImGuiCond_Appearing);

	if (ImGui::BeginPopupModal(LOCAL_CSTR("SpriteEditor.CreateAnimModalTitle"), &m_ShowCreateAnimModal, ImGuiWindowFlags_AlwaysAutoResize))
	{
		int mode = static_cast<int>(m_AnimParams.mode);
		ImGui::RadioButton(LOCAL_CSTR("SpriteEditor.AnimModeSingle"), &mode, 0);
		ImGui::SameLine();
		ImGui::RadioButton(LOCAL_CSTR("SpriteEditor.AnimModeRow"), &mode, 1);
		m_AnimParams.mode = static_cast<EAutoAnimGenerationMode>(mode);

		ImGui::Separator();

		if (m_AnimParams.mode == EAutoAnimGenerationMode::SingleClip)
		{
			char nameBuf[128];
			strncpy_s(nameBuf, m_AnimParams.singleClipName.c_str(), sizeof(nameBuf));
			if (ImGui::InputText(LOCAL_CSTR("SpriteEditor.ClipName"), nameBuf, sizeof(nameBuf)))
			{
				m_AnimParams.singleClipName = nameBuf;
			}
		}
		else
		{
			char rowBuf[256];
			strncpy_s(rowBuf, m_AnimParams.rowNames.c_str(), sizeof(rowBuf));
			if (ImGui::InputText(LOCAL_CSTR("SpriteEditor.RowNames"), rowBuf, sizeof(rowBuf)))
			{
				m_AnimParams.rowNames = rowBuf;
			}
			ImGui::TextDisabled("%s", LOCAL_CSTR("SpriteEditor.RowHelp"));
		}

		ImGui::SliderFloat(LOCAL_CSTR("SpriteEditor.FpsLabel"), &m_AnimParams.fps, 1.0f, 60.0f, "%.0f fps");

		const char* playModes[] = { "Loop", "Once", "PingPong" };
		int currentPlayMode = static_cast<int>(m_AnimParams.playMode);
		if (ImGui::Combo(LOCAL_CSTR("SpriteEditor.PlayModeLabel"), &currentPlayMode, playModes, IM_ARRAYSIZE(playModes)))
		{
			m_AnimParams.playMode = static_cast<Engine::ESpriteAnimationPlayMode>(currentPlayMode);
		}

		ImGui::Separator();
		ImGui::Spacing();

		const EditorUI::DialogAction actions[] = {
			{ "EditorCommon.Cancel" }, { "SpriteEditor.GenerateAnim" }
		};
		const int action = EditorUI::DialogButtonBar("GenerateAnimationActions", actions, IM_ARRAYSIZE(actions));
		if (action == 1 && GenerateAnimation(m_AnimParams))
		{
			m_ShowCreateAnimModal = false;
			ImGui::CloseCurrentPopup();
		}

		if (action == 0)
		{
			m_ShowCreateAnimModal = false;
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}

void SpriteEditor::ExecuteSlice(const SpriteSliceParameters& params)
{
	auto* texture = m_TextureHandle.Get();
	auto* rhiTexture = texture ? texture->GetRHITexture() : nullptr;
	if (!rhiTexture) return;

	const int texW = static_cast<int>(rhiTexture->GetWidth());
	const int texH = static_cast<int>(rhiTexture->GetHeight());
	if (texW <= 0 || texH <= 0) return;

	std::string baseName = Engine::Unicode::WideToUtf8(m_TargetResourcePath.stem().wstring());
	if (Engine::ToLowerStr(baseName).ends_with(".bamsprite"))
		baseName = baseName.substr(0, baseName.size() - 10);

	if (params.method == ESpriteSliceMethod::Automatic)
	{
		const auto resolvedPath = Engine::PlatformPath::ResolveUri(texture->GetPath());
		auto slices = SpriteSlicingService::SliceFromFile(resolvedPath, params, baseName);
		if (slices.empty())
		{
			NotificationManager::Get().AddNotification(GetDisplayName(),
				LOCAL("TextureEditor.AutomaticSlicingFailed"), ELogLevel::Warn);
			return;
		}
		m_Slices = std::move(slices);
	}
	else
	{
		auto slices = SpriteSlicingService::SliceGrid(texW, texH, params, baseName);
		if (slices.empty())
		{
			NotificationManager::Get().AddNotification(GetDisplayName(),
				LOCAL("TextureEditor.SlicingNoRegions"), ELogLevel::Warn);
			return;
		}
		m_Slices = std::move(slices);
	}

	for (auto& slice : m_Slices)
		slice.pixelPerUnit = params.pixelPerUnit > 0.0f ? params.pixelPerUnit : 100.0f;
	m_SelectedSliceIndex = m_Slices.empty() ? -1 : 0;
	m_History.MarkEdited();
}

void SpriteEditor::ApplySlices()
{
	if (SaveSlices())
	{
		NotificationManager::Get().AddNotification(LOCAL("ResourceEditor.SpriteTitle"),
			LOCAL_FMT("Notification.Saved", Engine::Unicode::WideToUtf8(m_TargetResourcePath.filename().wstring())), ELogLevel::Info);
	}
}

std::optional<std::vector<ResourceHandle<Engine::Sprite>>> SpriteEditor::SaveSlices()
{
	if (!CommitDocumentEdit()) return std::nullopt;
	if (m_Slices.empty() || !m_TextureHandle.IsValid())
	{
		NotifySpriteSaveFailure(Engine::Unicode::WideToUtf8(m_TargetResourcePath.filename().wstring()));
		return std::nullopt;
	}
	if (Engine::PlatformPath::IsReadOnlyTarget(m_TargetResourcePath))
	{
		NotifySpriteSaveFailure(Engine::Unicode::WideToUtf8(m_TargetResourcePath.filename().wstring()));
		return std::nullopt;
	}

	auto& rm = Engine::ResourceManager::Get();
	const std::filesystem::path destDir = m_TargetResourcePath.parent_path();
	const bool preferJson = !Engine::ToLowerWStr(m_TargetResourcePath.filename().wstring()).ends_with(L".bamsprite");
	std::vector<std::filesystem::path> outputPaths;
	outputPaths.reserve(m_Slices.size());
	std::unordered_set<std::wstring> uniquePaths;

	for (const auto& slice : m_Slices)
	{
		std::string cleanName = slice.name;
		const auto lowerName = Engine::ToLowerStr(cleanName);
		if (lowerName.ends_with(".bamsprite.json"))
			cleanName.resize(cleanName.size() - std::string_view(".bamsprite.json").size());
		else if (lowerName.ends_with(".bamsprite"))
			cleanName.resize(cleanName.size() - std::string_view(".bamsprite").size());

		if (!IsValidSpriteOutputName(cleanName))
		{
			NotifySpriteSaveFailure(slice.name);
			return std::nullopt;
		}

		const auto jsonPath = destDir / Engine::Utf8ToPath(cleanName + ".bamsprite.json");
		const auto binPath = destDir / Engine::Utf8ToPath(cleanName + ".bamsprite");
		std::error_code error;
		const bool jsonExists = std::filesystem::exists(jsonPath, error);
		const bool binaryExists = !error && !jsonExists && preferJson && std::filesystem::exists(binPath, error);
		if (error)
		{
			NotifySpriteSaveFailure(slice.name);
			return std::nullopt;
		}

		const auto outPath = IsSpriteAsset(m_TargetResourcePath) &&
			(SameResourceEditorPath(m_TargetResourcePath, jsonPath) || SameResourceEditorPath(m_TargetResourcePath, binPath))
			? m_TargetResourcePath : (jsonExists || (preferJson && !binaryExists)) ? jsonPath : binPath;
		const auto* textureEditor = dynamic_cast<TextureEditor*>(ImGuiManager::Get().GetImGuiPanel(L"Texture Editor"));
		if (textureEditor && textureEditor->IsConversionOutput(outPath))
		{
			NotificationManager::Get().AddNotification(GetDisplayName(), LOCAL("TextureEditor.ConversionTargetBusy"), ELogLevel::Warn);
			return std::nullopt;
		}
		if (Engine::PlatformPath::IsReadOnlyTarget(outPath))
		{
			NotifySpriteSaveFailure(Engine::Unicode::WideToUtf8(outPath.filename().wstring()));
			return std::nullopt;
		}
		if (std::filesystem::exists(outPath, error))
		{
			auto existing = rm.GetResourceHandle<Engine::Sprite>(Engine::NormalizePath(outPath.wstring()));
			if (!existing.IsValid())
				existing = static_cast<ResourceHandle<Engine::Sprite>>(rm.LoadFile(outPath.wstring()));
			const auto* sprite = existing.Get();
			const auto* referenced = sprite ? sprite->GetTextureHandle().Get() : nullptr;
			const auto* target = m_TextureHandle.Get();
			if (!referenced || !target || referenced->GetPath().empty() || target->GetPath().empty() ||
				!std::filesystem::equivalent(Engine::PlatformPath::ResolveUri(referenced->GetPath()),
					Engine::PlatformPath::ResolveUri(target->GetPath()), error) || error)
			{
				NotifySpriteSaveFailure(Engine::Unicode::WideToUtf8(outPath.filename().wstring()));
				return std::nullopt;
			}
		}
		if (error)
		{
			NotifySpriteSaveFailure(Engine::Unicode::WideToUtf8(outPath.filename().wstring()));
			return std::nullopt;
		}
		if (!uniquePaths.insert(Engine::ToLowerWStr(outPath.lexically_normal().wstring())).second)
		{
			NotifySpriteSaveFailure(Engine::Unicode::WideToUtf8(outPath.filename().wstring()));
			return std::nullopt;
		}
		outputPaths.push_back(outPath);
	}

	std::vector<ResourceHandle<Engine::Sprite>> savedSprites;
	savedSprites.reserve(m_Slices.size());
	for (size_t i = 0; i < m_Slices.size(); ++i)
	{
		const auto& slice = m_Slices[i];
		const auto& outPath = outputPaths[i];
		const bool useJson = Engine::ToLowerWStr(outPath.extension().wstring()) == L".json";
		SpriteCreateDesc desc;
		desc.texture = m_TextureHandle;
		desc.region = slice.region;
		desc.pivot = slice.pivot;
		desc.border = slice.border;
		desc.pixelPerUnit = slice.pixelPerUnit > 0.0f ? slice.pixelPerUnit : 100.0f;
		desc.Path = outPath.wstring();
		desc.Key = Engine::PlatformPath::ToProjectRelative(outPath).wstring();

		auto* sprite = Engine::Sprite::Create(&desc);
		if (!sprite)
		{
			NotifySpriteSaveFailure(Engine::Unicode::WideToUtf8(outPath.filename().wstring()));
			return std::nullopt;
		}

		const auto result = useJson ? rm.SaveToJsonFile(sprite, desc.Path) : rm.SaveToBinaryFile(sprite, desc.Path);
		rm.DestroyResource(sprite);
		if (result != Engine::EResult::Success)
		{
			NotifySpriteSaveFailure(Engine::Unicode::WideToUtf8(outPath.filename().wstring()));
			return std::nullopt;
		}
		// 이미 메모리에 로드된 Sprite 인스턴스가 있으면 잘린 영역(Region)/피벗/보더를 즉시 실시간 갱신
		Engine::Handle handle = rm.FindHandleByFilePath(outPath);
		if (!handle.IsValid())
		{
			handle = rm.FindHandleByKey(desc.Key);
			if (!handle.IsValid() && useJson && desc.Key.ends_with(L".json"))
			{
				const auto legacyHandle = rm.FindHandleByKey(desc.Key.substr(0, desc.Key.size() - 5));
				const auto* legacySprite = dynamic_cast<Engine::Sprite*>(rm.GetResource(legacyHandle));
				std::error_code error;
				if (legacySprite && std::filesystem::equivalent(Engine::PlatformPath::ResolveUri(legacySprite->GetPath()),
					Engine::PlatformPath::ResolveUri(outPath), error))
				{
					handle = legacyHandle;
				}
			}
		}

		if (auto* liveSprite = dynamic_cast<Engine::Sprite*>(rm.GetResource(handle)))
		{
			liveSprite->SetTexture(m_TextureHandle);
			liveSprite->SetRegion(slice.region);
			liveSprite->SetPivot(slice.pivot);
			liveSprite->SetBorder(slice.border);
			liveSprite->SetPixelPerUnit(desc.pixelPerUnit);
		}
		else
		{
			handle = rm.LoadFile(desc.Path);
			if (!dynamic_cast<Engine::Sprite*>(rm.GetResource(handle)))
			{
				NotifySpriteSaveFailure(Engine::Unicode::WideToUtf8(outPath.filename().wstring()));
				return std::nullopt;
			}
		}

		savedSprites.emplace_back(handle);
	}

	m_History.MarkSaved();
	return savedSprites;
}

bool SpriteEditor::GenerateAnimation(const SpriteAutoAnimParameters& params)
{
	if (m_Slices.empty() || !m_TextureHandle.IsValid())
	{
		NotifySpriteSaveFailure(Engine::Unicode::WideToUtf8(m_TargetResourcePath.filename().wstring()));
		return false;
	}

	struct AnimationClipOutput
	{
		std::string name;
		std::vector<size_t> sliceIndices;
		std::filesystem::path path;
	};

	std::vector<AnimationClipOutput> clips;
	if (params.mode == EAutoAnimGenerationMode::SingleClip)
	{
		auto& clip = clips.emplace_back();
		clip.name = params.singleClipName;
		clip.sliceIndices.reserve(m_Slices.size());
		for (size_t i = 0; i < m_Slices.size(); ++i)
			clip.sliceIndices.push_back(i);
	}
	else
	{
		std::vector<std::string> actionNames;
		std::stringstream ss(params.rowNames);
		std::string item;
		while (std::getline(ss, item, ','))
		{
			const size_t start = item.find_first_not_of(" \t\r\n");
			const size_t end = item.find_last_not_of(" \t\r\n");
			if (start != std::string::npos)
				actionNames.push_back(item.substr(start, end - start + 1));
		}

		// 슬라이스의 원래 인덱스를 보관해 저장 결과와 연결한다.
		for (size_t i = 0; i < m_Slices.size(); ++i)
		{
			const auto row = std::find_if(clips.begin(), clips.end(), [&](const auto& clip) {
				return std::abs(m_Slices[clip.sliceIndices.front()].region.top - m_Slices[i].region.top) <= 4.0f;
			});
			if (row == clips.end())
				clips.push_back({ {}, { i }, {} });
			else
				row->sliceIndices.push_back(i);
		}

		std::sort(clips.begin(), clips.end(), [&](const auto& a, const auto& b) {
			return m_Slices[a.sliceIndices.front()].region.top < m_Slices[b.sliceIndices.front()].region.top;
		});

		const auto baseName = Engine::Unicode::WideToUtf8(m_TargetResourcePath.stem().wstring());
		for (size_t r = 0; r < clips.size(); ++r)
		{
			auto& clip = clips[r];
			std::sort(clip.sliceIndices.begin(), clip.sliceIndices.end(), [&](size_t a, size_t b) {
				return m_Slices[a].region.left < m_Slices[b].region.left;
			});
			const auto actionName = r < actionNames.size() ? actionNames[r] : "Action_" + std::to_string(r);
			clip.name = baseName + "_" + actionName;
		}
	}

	const auto destDir = m_TargetResourcePath.parent_path();
	std::unordered_set<std::wstring> uniquePaths;
	for (auto& clip : clips)
	{
		if (!IsValidSpriteOutputName(clip.name))
		{
			NotifySpriteSaveFailure(clip.name);
			return false;
		}

		clip.path = destDir / Engine::Utf8ToPath(clip.name + ".spriteanim");
		if (Engine::PlatformPath::IsReadOnlyTarget(clip.path) ||
			!uniquePaths.insert(Engine::ToLowerWStr(clip.path.wstring())).second)
		{
			NotifySpriteSaveFailure(Engine::Unicode::WideToUtf8(clip.path.filename().wstring()));
			return false;
		}
	}

	const auto savedSprites = SaveSlices();
	if (!savedSprites)
		return false;

	auto& rm = Engine::ResourceManager::Get();
	for (const auto& clip : clips)
	{
		auto* anim = Engine::SpriteAnimation::Create();
		if (!anim)
		{
			NotifySpriteSaveFailure(Engine::Unicode::WideToUtf8(clip.path.filename().wstring()));
			return false;
		}

		anim->SetFPS(params.fps);
		anim->SetPlayMode(params.playMode);
		for (const size_t sliceIndex : clip.sliceIndices)
			anim->AddFrame((*savedSprites)[sliceIndex], 1.0f);

		const bool saved = anim->SaveToFile(clip.path.wstring());
		rm.DestroyResource(anim);
		if (!saved)
		{
			NotifySpriteSaveFailure(Engine::Unicode::WideToUtf8(clip.path.filename().wstring()));
			return false;
		}
	}

	NotificationManager::Get().AddNotification(LOCAL("ResourceEditor.SpriteTitle"),
		LOCAL_CSTR("SpriteEditor.GenerateAnimSuccess"), ELogLevel::Info);
	return true;
}

} // namespace Editor
