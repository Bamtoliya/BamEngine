#include "ViewportImage.h"

#include "ChannelFilter.h"
#include "ImGuiManager.h"
#include "RenderTarget.h"
#include "RenderTargetManager.h"

namespace Editor
{
	ViewportImage ResolveViewportImage(const std::wstring& rtName)
	{
		const auto* target = Engine::RenderTargetManager::Get().GetRenderTarget(rtName);
		auto* texture = target ? target->GetTexture() : nullptr;
		if (!texture)
			return {};

		if (!ChannelFilter::CanSampleAsTexture2D(texture))
			return { EViewportImageStatus::Unsupported };

		const auto textureID = ImGuiManager::Get().GetImGuiTextureID(texture);
		if (!textureID)
			return {};

		const ImVec2 sourceSize(static_cast<float>(target->GetWidth()), static_cast<float>(target->GetHeight()));
		return { EViewportImageStatus::Ready, textureID, sourceSize };
	}

	ImVec2 DrawViewportImage(const ViewportImage& image, const ImVec2& size, ImU32 backgroundColor)
	{
		const ImVec2 pos = ImGui::GetCursorScreenPos();
		if (backgroundColor != 0)
		{
			ImGui::GetWindowDrawList()->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), backgroundColor);
		}

		ImGui::Image(image.TextureID, size);
		return pos;
	}
}