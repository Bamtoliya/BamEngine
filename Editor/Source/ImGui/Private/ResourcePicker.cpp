#include "ResourcePicker.h"
#include "AssetCache.h"
#include "ImGuiManager.h"

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
}