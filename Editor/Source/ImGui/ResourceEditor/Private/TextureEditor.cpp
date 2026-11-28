#include "TextureEditor.h"
#include "AsyncResourceOpenService.h"
#include "SpriteEditor.h"
#include "ResourceEditorPath.h"
#include "EditorUI.h"
#include "Functions.h"
#include "ResourceManager.h"
#include "NotificationManager.h"
#include "PlatformPath.h"
#include "ImGuiManager.h"
#include "AssetCache.h"
#include "AssetManager.h"
#include "AssetPathRelocation.h"
#include "AssetWorkerHost.h"
#include <IconsFontAwesome7.h>
#include <algorithm>
#include <utility>
#include <Windows.h>

namespace Editor
{
namespace
{
	bool IsTextureEditorConversionSource(const std::filesystem::path& path)
	{
		const auto ext = Engine::ToLowerStr(Engine::Unicode::WideToUtf8(path.extension().wstring()));
		return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga";
	}

	struct TextureEditorStagedFile
	{
		std::filesystem::path Path;
		~TextureEditorStagedFile()
		{
			if (Path.empty()) return;
			std::error_code error;
			if (!std::filesystem::remove(Path, error) && error)
				BAM_LOG(Error, "Editor", "Asset", "Failed to remove staged sprite: {}", error.message());
		}
	};

	bool CreateTextureEditorSprite(const std::filesystem::path& texturePath, const std::filesystem::path& outputPath)
	{
		TextureEditorStagedFile staged;
		if (!AssetWorkerHost::CreateTextureStagingPath(outputPath, staged.Path)) return false;
		auto& manager = Engine::ResourceManager::Get();
		SpriteCreateDesc desc;
		desc.texture = static_cast<ResourceHandle<Engine::Texture>>(manager.LoadFile(texturePath.wstring()));
		if (!desc.texture.IsValid() || !desc.texture.Get() || !desc.texture.Get()->GetRHITexture()) return false;
		desc.Path = outputPath.wstring();
		desc.Key = Engine::PlatformPath::ToProjectRelative(outputPath).wstring();
		auto* sprite = Engine::Sprite::Create(&desc);
		if (!sprite) return false;
		const bool valid = sprite->GetTextureHandle().IsValid() && sprite->GetTextureHandle().Get();
		const auto saved = valid ? manager.SaveToBinaryFile(sprite, staged.Path.wstring()) : EResult::Fail;
		manager.DestroyResource(sprite);
		if (saved != EResult::Success) return false;
		if (!MoveFileExW(staged.Path.c_str(), outputPath.c_str(), MOVEFILE_WRITE_THROUGH))
		{
			BAM_LOG(Error, "Editor", "Asset", "Failed to create converted sprite: {}, error={}",
				Engine::Unicode::WideToUtf8(outputPath.wstring()), GetLastError());
			return false;
		}
		if (AssetCache::IsCreated()) AssetCache::Get().RequestInvalidate(outputPath);
		return true;
	}
}

bool TextureEditor::IsSupported(const std::filesystem::path& path) const
{
	const auto ext = Engine::ToLowerStr(Engine::Unicode::WideToUtf8(path.extension().wstring()));
	return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga" ||
		ext == ".dds" || ext == ".hdr" || ext == ".bamtex" ||
		Engine::ToLowerStr(Engine::Unicode::WideToUtf8(path.filename().wstring())).ends_with(".bamtex.json");
}

bool TextureEditor::IsConversionOutput(const std::filesystem::path& path) const
{
	return m_Conversion->InProgress &&
		(SameResourceEditorPath(path, m_Conversion->BamtexPath) || SameResourceEditorPath(path, m_Conversion->DdsPath) ||
			SameResourceEditorPath(path, m_Conversion->SpritePath));
}

std::vector<std::filesystem::path> TextureEditor::GetRetainedResourcePaths() const
{
	auto paths = ResourceEditorInterface::GetRetainedResourcePaths();
	if (m_Conversion->InProgress)
	{
		paths.push_back(m_Conversion->SourcePath);
		paths.push_back(m_Conversion->BamtexPath);
		paths.push_back(m_Conversion->DdsPath);
		if (!m_Conversion->SpritePath.empty()) paths.push_back(m_Conversion->SpritePath);
	}
	return paths;
}

bool TextureEditor::PrepareAssetPathRelocation(AssetPathRelocation& relocation)
{
	if (m_Conversion->InProgress &&
		(relocation.Affects(m_Conversion->SourcePath) || relocation.Affects(m_Conversion->BamtexPath) ||
			relocation.Affects(m_Conversion->DdsPath) ||
			(!m_Conversion->SpritePath.empty() && relocation.Affects(m_Conversion->SpritePath))))
		return false;
	return PrepareTargetPathRelocation(relocation);
}

TextureEditor::~TextureEditor()
{
	CancelAsyncOpen();
}

void TextureEditor::ReleaseResourceDocument()
{
	CancelAsyncOpen();
	m_TextureHandle.Reset();
	m_ShowConversionConfirmation = false;
	m_PendingSource.clear();
}

void TextureEditor::ApplyLoadedTexture(const std::filesystem::path& path, ResourceHandle<Engine::Texture> handle)
{
	m_TextureHandle.Reset();
	m_ShowConversionConfirmation = false;
	m_PendingSource.clear();
	m_TargetResourcePath = path;
	m_TextureHandle = std::move(handle);
	if (IsTextureEditorConversionSource(path))
	{
		m_ImportDesc = TextureImporter::GetDefaultImportDesc(path);
		m_ImportDesc.CompressFlags &= ~ECompressFlags::NormalMap;
	}
}

void TextureEditor::SetTargetResource(const std::filesystem::path& path)
{
	CancelAsyncOpen();
	if (IsConversionOutput(path))
	{
		NotificationManager::Get().AddNotification(GetDisplayName(), LOCAL("TextureEditor.ConversionTargetBusy"), ELogLevel::Warn);
		return;
	}
	auto handle = static_cast<ResourceHandle<Engine::Texture>>(Engine::ResourceManager::Get().LoadFile(path.wstring()));
	if (!handle.IsValid() || !handle.Get() || !handle.Get()->GetRHITexture())
	{
		NotificationManager::Get().AddNotification(GetDisplayName(),
			LOCAL_FMT("Notification.ResourceLoadFailed", Engine::Unicode::WideToUtf8(path.filename().wstring())), ELogLevel::Error);
		return;
	}
	ApplyLoadedTexture(path, std::move(handle));
}

bool TextureEditor::BeginAsyncOpen(const std::filesystem::path& path)
{
	if (!IsSupported(path) || IsConversionOutput(path))
		return false;

	CancelAsyncOpen();

	m_AsyncTargetPath = path;
	m_AsyncStartTime = std::chrono::steady_clock::now();
	m_AsyncLoadStart = m_AsyncStartTime;
	m_AsyncLoadDurationMs = 0.0;
	m_AsyncStage = EAsyncStage::WaitingBackground;
	m_AsyncProgress = 0.08f;

	Open();
	Focus();
	return true;
}

bool TextureEditor::StepAsyncOpen(float maxTimeSeconds)
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
		if (IsConversionOutput(m_AsyncTargetPath))
		{
			NotificationManager::Get().AddNotification(GetDisplayName(), LOCAL("TextureEditor.ConversionTargetBusy"), ELogLevel::Warn);
			m_AsyncPendingTexture.Reset();
			m_AsyncTargetPath.clear();
			m_AsyncProgress = 0.0f;
			m_AsyncStage = EAsyncStage::None;
			return true;
		}

		auto handle = static_cast<ResourceHandle<Engine::Texture>>(Engine::ResourceManager::Get().LoadFile(m_AsyncTargetPath.wstring()));
		if (!handle.IsValid() || !handle.Get() || !handle.Get()->GetRHITexture())
		{
			NotificationManager::Get().AddNotification(GetDisplayName(),
				LOCAL_FMT("Notification.ResourceLoadFailed", Engine::Unicode::WideToUtf8(m_AsyncTargetPath.filename().wstring())), ELogLevel::Error);
			m_AsyncPendingTexture.Reset();
			m_AsyncTargetPath.clear();
			m_AsyncProgress = 0.0f;
			m_AsyncStage = m_TextureHandle.Get() ? EAsyncStage::None : EAsyncStage::Failed;
			return true;
		}

		m_AsyncPendingTexture = std::move(handle);
		m_AsyncLoadDurationMs = Milliseconds(Clock::now() - m_AsyncLoadStart).count();
		m_AsyncStage = EAsyncStage::Finalizing;
		m_AsyncProgress = 0.75f;

		if (Clock::now() >= stepDeadline)
			return false;
	}

	if (m_AsyncStage == EAsyncStage::Finalizing)
	{
		auto targetPath = std::move(m_AsyncTargetPath);
		auto pendingTexture = std::move(m_AsyncPendingTexture);
		ApplyLoadedTexture(targetPath, std::move(pendingTexture));
		const double totalMs = Milliseconds(Clock::now() - m_AsyncStartTime).count();

		EDITOR_LOG_INFO("[ResourceOpenTiming] Texture {}: resource={:.2f}ms, total={:.2f}ms (async)",
			Engine::Unicode::WideToUtf8(targetPath.wstring()), m_AsyncLoadDurationMs, totalMs);

		m_AsyncProgress = 1.0f;
		m_AsyncStage = EAsyncStage::None;
		return true;
	}

	return true;
}

void TextureEditor::CancelAsyncOpen()
{
	AsyncResourceOpenService::Get().CancelRequestsForEditor(this);
	m_AsyncStage = EAsyncStage::None;
	m_AsyncProgress = 0.0f;
	m_AsyncTargetPath.clear();
	m_AsyncPendingTexture.Reset();
}

void TextureEditor::DrawAsyncLoadingOverlay()
{
	DrawStandardAsyncLoadingOverlay(m_AsyncTargetPath, m_AsyncProgress,
		m_AsyncStage == EAsyncStage::WaitingBackground, m_AsyncStage == EAsyncStage::Failed);
}

void TextureEditor::DrawCustomQuickAccess()
{
	EditorUI::ToolbarLayout toolbar;
	EditorUI::IconButtonOptions copyOptions;
	copyOptions.Enabled = !m_TargetResourcePath.empty() && !IsAsyncLoading();
	if (toolbar.Icon("CopyResourcePath", ICON_FA_COPY, "EditorCommon.CopyPath", copyOptions))
		ImGui::SetClipboardText(Engine::Unicode::WideToUtf8(m_TargetResourcePath.wstring()).c_str());
	ImGui::BeginDisabled(IsAsyncLoading() || !m_TextureHandle.Get() || IsConversionOutput(m_TargetResourcePath));
	if (toolbar.Button("TextureEditor.OpenSpriteEditor", ICON_FA_CROP_SIMPLE, "TextureEditor.OpenSpriteEditor"))
	{
		if (auto* editor = dynamic_cast<SpriteEditor*>(ImGuiManager::Get().GetImGuiPanel(L"Sprite Editor")))
			editor->OpenInSpriteMode(m_TargetResourcePath);
		else
			NotificationManager::Get().AddNotification(GetDisplayName(), LOCAL("TextureEditor.SpriteEditorUnavailable"), ELogLevel::Error);
	}
	ImGui::EndDisabled();
}

void TextureEditor::DrawCustomContent()
{
	if (m_TargetResourcePath.empty())
	{
		EditorUI::EmptyState("ResourceEditor.NoResource");
		return;
	}
	auto* texture = m_TextureHandle.Get();
	auto* rhiTexture = texture ? texture->GetRHITexture() : nullptr;
	AssetCache::ThumbnailView thumbnail{};
	bool useTexture = rhiTexture && rhiTexture->GetDimension() == Engine::ETextureDimension::Texture2D &&
		rhiTexture->GetArraySize() == 1 && rhiTexture->GetWidth() > 0 && rhiTexture->GetHeight() > 0;
	auto* previewTexture = useTexture ? rhiTexture : nullptr;
	auto id = ImGuiManager::Get().GetImGuiTextureID(previewTexture);
	if (id == ImTextureID{})
	{
		useTexture = false;
		thumbnail = AssetCache::Get().GetThumbnailView({}, m_TargetResourcePath);
		previewTexture = thumbnail.Texture;
		id = ImGuiManager::Get().GetImGuiTextureID(previewTexture);
	}
	if (previewTexture && id != ImTextureID{})
	{
		if (useTexture) ImGui::TextDisabled("%s", LOCAL_CSTR("TextureEditor.TexturePreview"));
		else ImGui::TextDisabled("%s", LOCAL_CSTR("TextureEditor.ThumbnailPreview"));
		ImGui::TextUnformatted(LOCAL_CSTR("TextureEditor.PreviewSize"));
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::SliderFloat("##TexturePreviewSize", &m_PreviewSize, 64.0f, 768.0f, "%.0f");
		const float width = useTexture ? static_cast<float>(rhiTexture->GetWidth())
			: static_cast<float>(thumbnail.Texture->GetWidth()) * (thumbnail.UV1.x - thumbnail.UV0.x);
		const float height = useTexture ? static_cast<float>(rhiTexture->GetHeight())
			: static_cast<float>(thumbnail.Texture->GetHeight()) * (thumbnail.UV1.y - thumbnail.UV0.y);
		const float availableWidth = (std::max)(1.0f, ImGui::GetContentRegionAvail().x);
		const float scale = (std::min)(m_PreviewSize / (std::max)(1.0f, (std::max)(width, height)),
			availableWidth / (std::max)(1.0f, width));
		ImGui::Image(id, ImVec2(width * scale, height * scale),
			useTexture ? ImVec2(0.0f, 0.0f) : thumbnail.UV0, useTexture ? ImVec2(1.0f, 1.0f) : thumbnail.UV1);
	}
	else
		ImGui::TextDisabled("%s", LOCAL_CSTR("TextureEditor.NoPreviewAvailable"));

	if (EditorUI::BeginPropertyTable("TextureInformation", EditorUI::InspectorPropertyTableStyle()))
	{
		EditorUI::PropertyRow("EditorCommon.Path");
		const auto pathText = Engine::Unicode::WideToUtf8(m_TargetResourcePath.wstring());
		const bool clipped = ImGui::CalcTextSize(pathText.c_str()).x > ImGui::GetContentRegionAvail().x;
		ImGui::TextUnformatted(pathText.c_str());
		if (clipped && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", pathText.c_str());
		if (rhiTexture)
		{
			EditorUI::PropertyRow("TextureEditor.Dimensions");
			ImGui::Text(LOCAL_CSTR("TextureEditor.DimensionValue"), rhiTexture->GetWidth(), rhiTexture->GetHeight());
		}
		EditorUI::EndPropertyTable();
	}
	DrawTextureConversion();
	DrawTextureConversionConfirmation();
}

void TextureEditor::DrawTextureConversion()
{
	ImGui::Separator();
	ImGui::TextUnformatted(LOCAL_CSTR("TextureEditor.ConversionSettings"));
	const bool readOnly = Engine::PlatformPath::IsReadOnlyTarget(m_TargetResourcePath);
	if (!IsTextureEditorConversionSource(m_TargetResourcePath))
	{
		ImGui::TextDisabled("%s", LOCAL_CSTR("TextureEditor.ConversionUnsupported"));
		return;
	}
	if (readOnly) ImGui::TextDisabled("%s", LOCAL_CSTR("ContentBrowser.EngineReadOnlyBadge"));
	ImGui::BeginDisabled(m_Conversion->InProgress || readOnly);
	const Engine::ETextureFormat values[] = {
		Engine::ETextureFormat::BC1_RGBA_UNORM, Engine::ETextureFormat::BC2_RGBA_UNORM,
		Engine::ETextureFormat::BC3_RGBA_UNORM, Engine::ETextureFormat::BC4_R_UNORM,
		Engine::ETextureFormat::BC5_RG_UNORM, Engine::ETextureFormat::BC7_RGBA_UNORM
	};
	const std::string labels[] = { "BC1 (DXT1)", "BC2 (DXT3)", "BC3 (DXT5)",
		LOCAL("TextureInspector.BC4"), LOCAL("TextureInspector.BC5"), LOCAL("TextureInspector.BC7") };
	const char* names[] = { labels[0].c_str(), labels[1].c_str(), labels[2].c_str(),
		labels[3].c_str(), labels[4].c_str(), labels[5].c_str() };
	int format = 0;
	for (int i = 0; i < IM_ARRAYSIZE(values); ++i)
		if (values[i] == m_ImportDesc.TargetFormat) format = i;
	const std::string modeLabels[] = { LOCAL("TextureInspector.CompressionCpuStandard"),
		LOCAL("TextureInspector.CompressionCpuFast"), LOCAL("TextureInspector.CompressionGpu") };
	const char* modes[] = { modeLabels[0].c_str(), modeLabels[1].c_str(), modeLabels[2].c_str() };
	int mode = static_cast<int>(m_CompressionMode);
	if (EditorUI::BeginPropertyTable("TextureConversionSettings", EditorUI::InspectorPropertyTableStyle()))
	{
		EditorUI::PropertyRow("TextureInspector.TargetFormat");
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (ImGui::Combo("##TextureFormat", &format, names, IM_ARRAYSIZE(names)))
		{
			m_ImportDesc.TargetFormat = values[format];
			if (values[format] == Engine::ETextureFormat::BC4_R_UNORM || values[format] == Engine::ETextureFormat::BC5_RG_UNORM)
				m_ImportDesc.CompressFlags &= ~ECompressFlags::SRGB;
		}
		EditorUI::PropertyRow("TextureInspector.CompressionMode");
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (ImGui::Combo("##TextureCompressionMode", &mode, modes, IM_ARRAYSIZE(modes)))
			m_CompressionMode = static_cast<ETextureCompressionMode>(mode);
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", LOCAL_CSTR("TextureInspector.CompressionModeTooltip"));
		const auto drawFlag = [this](const char* label, const char* id, ECompressFlags flag) {
			EditorUI::PropertyRow(label);
			bool enabled = (static_cast<uint32>(m_ImportDesc.CompressFlags) & static_cast<uint32>(flag)) != 0;
			if (ImGui::Checkbox(id, &enabled))
			{
				if (enabled) m_ImportDesc.CompressFlags |= flag;
				else m_ImportDesc.CompressFlags &= ~flag;
			}
		};
		drawFlag("TextureInspector.GenerateMipMaps", "##TextureMipmaps", ECompressFlags::GenerateMipMaps);
		drawFlag("TextureEditor.ParallelCompression", "##TextureHighQuality", ECompressFlags::HighQuality);
		ImGui::BeginDisabled(m_ImportDesc.TargetFormat == Engine::ETextureFormat::BC4_R_UNORM ||
			m_ImportDesc.TargetFormat == Engine::ETextureFormat::BC5_RG_UNORM);
		drawFlag("TextureInspector.SRGB", "##TextureSRGB", ECompressFlags::SRGB);
		ImGui::EndDisabled();
		EditorUI::PropertyRow("TextureInspector.AutoSprite");
		ImGui::Checkbox("##AutoCreateSprite", &m_AutoCreateSprite);
		EditorUI::EndPropertyTable();
	}
	if (m_AutoCreateSprite) ImGui::TextWrapped("%s", LOCAL_CSTR("TextureEditor.SpriteNoOverwrite"));
	if (Engine::ToLowerStr(Engine::Unicode::WideToUtf8(m_TargetResourcePath.extension().wstring())) == ".tga")
		ImGui::TextWrapped("%s", LOCAL_CSTR("TextureEditor.TgaCodecRequired"));
	if (ImGui::Button(LOCAL_LABEL("TextureEditor.Convert").c_str()))
	{
		m_PendingSource = m_TargetResourcePath;
		m_ShowConversionConfirmation = true;
	}
	ImGui::EndDisabled();
	if (m_Conversion->InProgress) ImGui::TextDisabled("%s", LOCAL_CSTR("TextureInspector.Importing"));
}

void TextureEditor::DrawTextureConversionConfirmation()
{
	const auto title = LOCAL_LABEL("TextureEditor.ConversionConfirmTitle");
	if (m_ShowConversionConfirmation && !ImGui::IsPopupOpen(title.c_str())) ImGui::OpenPopup(title.c_str());
	if (ImGui::BeginPopupModal(title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		if (!m_ShowConversionConfirmation)
		{
			ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
			return;
		}
		auto bamtex = m_PendingSource;
		auto dds = m_PendingSource;
		bamtex.replace_extension(".bamtex");
		dds.replace_extension(".dds");
		ImGui::TextWrapped("%s", LOCAL_CSTR("TextureEditor.ConversionConfirmMessage"));
		ImGui::TextUnformatted(Engine::Unicode::WideToUtf8(bamtex.wstring()).c_str());
		ImGui::TextUnformatted(Engine::Unicode::WideToUtf8(dds.wstring()).c_str());
		if (m_AutoCreateSprite)
		{
			auto sprite = m_PendingSource;
			sprite.replace_extension(".bamsprite");
			ImGui::TextUnformatted(Engine::Unicode::WideToUtf8(sprite.wstring()).c_str());
			ImGui::TextWrapped("%s", LOCAL_CSTR("TextureEditor.SpriteNoOverwrite"));
		}
		const EditorUI::DialogAction actions[] = { { "EditorCommon.Cancel" }, { "TextureEditor.Convert", true, true } };
		const int action = EditorUI::DialogButtonBar("TextureConversionActions", actions, IM_ARRAYSIZE(actions));
		if (action >= 0)
		{
			m_ShowConversionConfirmation = false;
			if (action == 1) StartTextureConversion();
			m_PendingSource.clear();
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

void TextureEditor::StartTextureConversion()
{
	const auto source = Engine::PlatformPath::ResolveUri(m_PendingSource);
	auto bamtex = source;
	auto dds = source;
	auto sprite = source;
	bamtex.replace_extension(".bamtex");
	dds.replace_extension(".dds");
	sprite.replace_extension(".bamsprite");
	std::error_code error;
	if (m_Conversion->InProgress || !SameResourceEditorPath(source, m_TargetResourcePath) ||
		!IsTextureEditorConversionSource(source) || !std::filesystem::is_regular_file(source, error) || error ||
		Engine::PlatformPath::IsReadOnlyTarget(source) || Engine::PlatformPath::IsReadOnlyTarget(bamtex) ||
		Engine::PlatformPath::IsReadOnlyTarget(dds) || (m_AutoCreateSprite && Engine::PlatformPath::IsReadOnlyTarget(sprite)))
	{
		NotificationManager::Get().AddNotification(GetDisplayName(), LOCAL("TextureEditor.ConversionRequestFailed"), ELogLevel::Error);
		return;
	}
	if (m_AutoCreateSprite && (std::filesystem::exists(sprite, error) || error))
	{
		NotificationManager::Get().AddNotification(GetDisplayName(), LOCAL("TextureEditor.SpriteAlreadyExists"), ELogLevel::Error);
		return;
	}
	if (auto* editor = dynamic_cast<SpriteEditor*>(ImGuiManager::Get().GetImGuiPanel(L"Sprite Editor")))
	{
		for (const auto& retained : editor->GetRetainedResourcePaths())
		{
			if (SameResourceEditorPath(retained, bamtex) || SameResourceEditorPath(retained, dds) ||
				(m_AutoCreateSprite && SameResourceEditorPath(retained, sprite)))
			{
				NotificationManager::Get().AddNotification(GetDisplayName(), LOCAL("TextureEditor.ConversionOutputInUse"), ELogLevel::Warn);
				return;
			}
		}
	}
	auto desc = m_ImportDesc;
	desc.CompressFlags = ApplyTextureCompressionMode(desc.CompressFlags, m_CompressionMode, desc.TargetFormat);
	const auto state = m_Conversion;
	state->InProgress = true;
	state->SourcePath = source;
	state->BamtexPath = bamtex;
	state->DdsPath = dds;
	state->SpritePath = m_AutoCreateSprite ? sprite : std::filesystem::path{};
	const bool createSprite = m_AutoCreateSprite;
	AssetManager::Get().ImportAsync(source, source.parent_path(), &desc, [state, source, bamtex, dds, sprite, createSprite](EResult result) {
		state->InProgress = false;
		if (result == EResult::Success && AssetCache::IsCreated())
		{
			AssetCache::Get().RequestInvalidate(bamtex);
			AssetCache::Get().RequestInvalidate(dds);
		}
		bool reloadFailed = false;
		if (result == EResult::Success && Engine::ResourceManager::IsCreated())
		{
			auto& manager = Engine::ResourceManager::Get();
			for (const auto& handle : manager.GetResourceHandles<Engine::Texture>())
			{
				auto* texture = dynamic_cast<Engine::Texture*>(manager.GetResource(handle));
				if (!texture) continue;
				const auto& texturePath = texture->GetPath().empty() ? texture->GetKey() : texture->GetPath();
				const auto* output = SameResourceEditorPath(texturePath, bamtex) ? &bamtex
					: SameResourceEditorPath(texturePath, dds) ? &dds : nullptr;
				if (output && !IsSuccess(texture->Reload(*output)))
				{
					reloadFailed = true;
					BAM_LOG(Error, "Editor", "Asset", "Failed to reload converted texture: {}", Engine::Unicode::WideToUtf8(output->wstring()));
				}
			}
		}
		const bool spriteFailed = result == EResult::Success && createSprite &&
			(!Engine::ResourceManager::IsCreated() || !CreateTextureEditorSprite(bamtex, sprite));
		if (NotificationManager::IsCreated())
		{
			NotificationManager::Get().AddNotification(LOCAL("ResourceEditor.TextureTitle"),
				result == EResult::Success ? LOCAL_FMT("TextureEditor.ConversionSucceeded", Engine::Unicode::WideToUtf8(source.filename().wstring()))
					: LOCAL_FMT("TextureEditor.ConversionFailed", Engine::Unicode::WideToUtf8(source.filename().wstring())),
				result == EResult::Success ? ELogLevel::Info : ELogLevel::Error);
			if (reloadFailed) NotificationManager::Get().AddNotification(LOCAL("ResourceEditor.TextureTitle"),
				LOCAL("TextureEditor.ConversionReloadFailed"), ELogLevel::Warn);
			if (spriteFailed) NotificationManager::Get().AddNotification(LOCAL("ResourceEditor.TextureTitle"),
				LOCAL("TextureEditor.SpriteGenerationFailed"), ELogLevel::Error);
			else if (result == EResult::Success && createSprite)
				NotificationManager::Get().AddNotification(LOCAL("ResourceEditor.TextureTitle"),
					LOCAL_FMT("TextureEditor.SpriteGenerationSucceeded", Engine::Unicode::WideToUtf8(sprite.filename().wstring())), ELogLevel::Info);
		}
	});
}
}
