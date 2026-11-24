#include "ResourcePicker.h"
#include "AssetCache.h"
#include "ImGuiManager.h"
#include "AssetFileActions.h"

namespace Editor::Detail
{
	ResourceFieldThumbnail GetResourcePickerThumbnail(const std::filesystem::path& assetPath)
	{
		const auto thumbnail = AssetCache::Get().GetThumbnailView({}, assetPath);
		return { thumbnail.Texture, thumbnail.UV0, thumbnail.UV1 };
	}

	ImTextureID GetResourcePickerTextureID(Engine::RHITexture* texture)
	{
		if (texture == nullptr ||
			texture->GetDimension() != Engine::ETextureDimension::Texture2D ||
			texture->GetArraySize() != 1)
			return ImTextureID{};
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

		if (ImGui::MenuItem(LOCAL_LABEL("EditorCommon.RevealInContentBrowser").c_str()))
			AssetFileActions::RevealInContentBrowser(effectivePath);

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
