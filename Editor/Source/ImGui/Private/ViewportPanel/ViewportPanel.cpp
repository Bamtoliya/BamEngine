#pragma once

#include "ViewportPanel.h"

#include "imgui.h"


#pragma region Contructor&Destructor
void ViewportPanel::Initialize(void* arg)
{
}
#pragma endregion

void ViewportPanel::Draw()
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	bool opened = true;
	if (ImGui::Begin("Viewport", &opened))
	{
		ImVec2 panelSize = ImGui::GetContentRegionAvail();
		uint32 width = (uint32)panelSize.x;
		uint32 height = (uint32)panelSize.y;
		f32 panelAspectRatio = width / height;

		RenderTarget* target = RenderTargetManager::Get().GetRenderTarget(L"RenderTarget_1");
		if (target)
		{
			RHITexture* texture = target->GetTexture(0);
			if (texture)
			{
				f32 imageWidth = (f32)target->GetWidth();
				f32 imageHeight = (f32)target->GetHeight();

				f32 imageRatio = imageWidth / imageHeight;

				ImVec2 finalSize = panelSize;

				if (panelAspectRatio > imageRatio)
				{
					finalSize.y = panelSize.y;
					finalSize.x = finalSize.y * imageRatio;
				}
				else
				{
					finalSize.x = panelSize.x;
					finalSize.y = finalSize.x / imageRatio;
				}

				if (panelSize.x > 0 && panelSize.y > 0)
				{
					Renderer::Get().GetRHI()->Resize((uint32)panelSize.x, (uint32)panelSize.y);
				}

				ImVec2 cursorStart = ImGui::GetCursorPos();
				float offsetX = (panelSize.x - finalSize.x) * 0.5f;
				float offsetY = (panelSize.y - finalSize.y) * 0.5f;
				ImGui::SetCursorPos(ImVec2(cursorStart.x + offsetX, cursorStart.y + offsetY));

				ImTextureID textureID = (ImTextureID)(size_t)texture->GetNativeHandle();
				ImGui::Image(textureID, finalSize);
			}
		}
	}
	ImGui::End();
	ImGui::PopStyleVar();
}