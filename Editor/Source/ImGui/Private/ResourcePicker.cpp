#include "ResourcePicker.h"
#include "AssetCache.h"
#include "ImGuiManager.h"
#include "ContentBrowserPanel.h"
#include "NotificationManager.h"

namespace Editor::Detail
{
	Engine::RHITexture* GetResourcePickerThumbnail(const std::filesystem::path& assetPath)
	{
		return AssetCache::Get().GetThumbnail(assetPath);
	}

	ImTextureID GetResourcePickerTextureID(Engine::RHITexture* texture)
	{
		return ImGuiManager::Get().GetImGuiTextureID(texture);
	}

	bool DrawResourceContextMenu(const std::filesystem::path& resourcePath)
	{
		if (!ImGui::BeginPopupContextItem("##ResourceContext"))
			return false;

		auto& manager = ImGuiManager::Get();
		const auto resolvedTarget = manager.ResolveResourceEditorTarget(resourcePath);
		const auto effectivePath = resolvedTarget.empty() ? resourcePath : resolvedTarget;
		const bool canOpen = manager.CanOpenResourceEditor(effectivePath);

		const bool openRequested = ImGui::MenuItem(LOCAL_LABEL("ResourcePicker.OpenInEditor").c_str(),
			nullptr, false, canOpen) && manager.RequestOpenResourceEditor(effectivePath);

		if (ImGui::MenuItem(LOCAL_LABEL("ResourcePicker.RevealInContentBrowser").c_str()))
		{
			auto* panel = manager.GetImGuiPanel(L"Content Browser");
			auto* browser = dynamic_cast<ContentBrowserPanel*>(panel);
			if (browser == nullptr || !browser->RequestRevealAsset(effectivePath))
			{
				NotificationManager::Get().AddNotification(LOCAL("ResourcePicker.RevealInContentBrowser"),
					LOCAL("ResourcePicker.RevealFailed"), ELogLevel::Warn);
			}
		}

		if (ImGui::MenuItem(LOCAL_LABEL("EditorCommon.CopyPath").c_str()))
			ImGui::SetClipboardText(Engine::WStrToStr(effectivePath.wstring()).c_str());

		ImGui::EndPopup();
		return openRequested;
	}

	bool RequestOpenResourceEditor(const std::filesystem::path& resourcePath)
	{
		auto& manager = ImGuiManager::Get();
		const auto resolvedTarget = manager.ResolveResourceEditorTarget(resourcePath);
		const auto effectivePath = resolvedTarget.empty() ? resourcePath : resolvedTarget;
		if (manager.CanOpenResourceEditor(effectivePath))
			return manager.RequestOpenResourceEditor(effectivePath);
		return false;
	}
}
