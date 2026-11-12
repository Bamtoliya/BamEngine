#pragma once

#include <string>
#include <imgui.h>

namespace Editor
{
	enum class EViewportImageStatus
	{
		Unavailable,
		Unsupported,
		Ready,
	};

	struct ViewportImage
	{
		EViewportImageStatus Status = EViewportImageStatus::Unavailable;
		ImTextureID TextureID{};
		ImVec2 SourceSize{};
	};

	[[nodiscard]] ViewportImage ResolveViewportImage(const std::wstring& rtName);
	ImVec2 DrawViewportImage(const ViewportImage& image, const ImVec2& size, ImU32 backgroundColor = 0);
}