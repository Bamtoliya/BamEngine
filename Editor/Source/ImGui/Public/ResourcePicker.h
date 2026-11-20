#pragma once

#include "ReflectionInspectorSupport.h"
#include "PickerAssetCatalog.h"
#include "EditorUI.h"
#include "InputManager.h"
#include "Functions.h"
#include "IconsFontAwesome7.h"
#include <imgui.h>
#include <fmt/format.h>
#include <string_view>
#include <cctype>
#include <algorithm>
#include <filesystem>
#include <functional>
#include <unordered_set>
#include "RHITexture.h"

namespace Engine
{
	class Texture;
	class Material;
	class MaterialInstance;
	class MaterialInterface;
	class Model;
	class Mesh;
	class Sprite;
	class AudioClip;
	class Shader;
	class Animation;
	class Skeleton;
	class SpriteAnimation;
	class ParticleEffect;
	class CompositeEffect;
}

namespace Editor
{
	namespace Detail
	{
		struct ResourceFieldThumbnail
		{
			Engine::RHITexture* Texture = nullptr;
			ImVec2 UV0 = ImVec2(0.0f, 0.0f);
			ImVec2 UV1 = ImVec2(1.0f, 1.0f);
		};

		ResourceFieldThumbnail GetResourcePickerThumbnail(const std::filesystem::path& assetPath);
		ImTextureID GetResourcePickerTextureID(Engine::RHITexture* texture);

		template<typename T>
		inline bool IsResourceExtensionCompatible(std::string_view lowerFilename)
		{
			auto endsWith = [lowerFilename](std::string_view suffix) { return lowerFilename.ends_with(suffix); };
			if constexpr (std::is_same_v<T, Engine::Texture>)
			{
				return endsWith(".png") || endsWith(".jpg") || endsWith(".jpeg") ||
					endsWith(".dds") || endsWith(".tga") || endsWith(".bmp") || endsWith(".hdr") ||
					endsWith(".bamtex") || endsWith(".bamtex.json");
			}
			else if constexpr (std::is_same_v<T, Engine::Material>)
			{
				return endsWith(".bammat") || endsWith(".bammat.json") || endsWith(".mat");
			}
			else if constexpr (std::is_same_v<T, Engine::MaterialInstance>)
			{
				return endsWith(".bammatinst") || endsWith(".bammatinst.json");
			}
			else if constexpr (std::is_same_v<T, Engine::MaterialInterface>)
			{
				return endsWith(".bammat") || endsWith(".bammat.json") ||
					endsWith(".bammatinst") || endsWith(".bammatinst.json") || endsWith(".mat");
			}
			else if constexpr (std::is_same_v<T, Engine::Model>)
			{
				return endsWith(".bammodel") || endsWith(".bammodel.json");
			}
			else if constexpr (std::is_same_v<T, Engine::Mesh>)
			{
				return endsWith(".bammesh") || endsWith(".bammesh.json");
			}
			else if constexpr (std::is_same_v<T, Engine::Sprite>)
			{
				return endsWith(".bamsprite") || endsWith(".bamsprite.json");
			}
			else if constexpr (std::is_same_v<T, Engine::AudioClip>)
			{
				return endsWith(".wav") || endsWith(".mp3") || endsWith(".ogg") || endsWith(".flac");
			}
			else if constexpr (std::is_same_v<T, Engine::Shader>)
			{
				return endsWith(".hlsl") || endsWith(".cso") || endsWith(".bamshader") || endsWith(".bamshader.json");
			}
			else if constexpr (std::is_same_v<T, Engine::Animation>)
			{
				return endsWith(".bamanim") || endsWith(".bamanim.json");
			}
			else if constexpr (std::is_same_v<T, Engine::Skeleton>)
			{
				return endsWith(".bamskel") || endsWith(".bamskel.json") || endsWith(".bamskeleton");
			}
			else if constexpr (std::is_same_v<T, Engine::SpriteAnimation>)
			{
				return endsWith(".bamspriteanim") || endsWith(".bamspriteanim.json") || endsWith(".spriteanim") || endsWith(".spriteanim.json");
			}
			else if constexpr (std::is_same_v<T, Engine::ParticleEffect>)
			{
				return endsWith(".bameffect") || endsWith(".bameffect.json") || endsWith(".effect") || endsWith(".effect.json");
			}
			else if constexpr (std::is_same_v<T, Engine::CompositeEffect>)
			{
				return endsWith(".bamvfx") || endsWith(".bamvfx.json") || endsWith(".vfx") || endsWith(".vfx.json");
			}
			else
			{
				return true;
			}
		}

		inline std::wstring NormalizeResourceKey(std::wstring_view key)
		{
			std::wstring norm(key);
			std::replace(norm.begin(), norm.end(), L'/', L'\\');
			return norm;
		}

		struct ResourceVisual
		{
			const char* Icon = ICON_FA_FILE;
			ImVec4 Color = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
			const char* Badge = "ASSET";
		};

		template<typename T>
		inline ResourceVisual GetResourceVisual(std::string_view filename)
		{
			if constexpr (std::is_same_v<T, Engine::Texture>)
				return { ICON_FA_IMAGE, ImVec4(0.20f, 0.82f, 0.45f, 1.0f), "TEXTURE" };
			else if constexpr (std::is_same_v<T, Engine::Material> || std::is_same_v<T, Engine::MaterialInstance> || std::is_same_v<T, Engine::MaterialInterface>)
				return { ICON_FA_CIRCLE_HALF_STROKE, ImVec4(0.95f, 0.42f, 0.32f, 1.0f), "MATERIAL" };
			else if constexpr (std::is_same_v<T, Engine::Model> || std::is_same_v<T, Engine::Mesh>)
				return { ICON_FA_CUBE, ImVec4(0.35f, 0.65f, 1.0f, 1.0f), "3D MODEL" };
			else if constexpr (std::is_same_v<T, Engine::Sprite>)
				return { ICON_FA_IMAGES, ImVec4(0.30f, 0.88f, 0.60f, 1.0f), "SPRITE" };
			else if constexpr (std::is_same_v<T, Engine::AudioClip>)
				return { ICON_FA_MUSIC, ImVec4(0.98f, 0.78f, 0.22f, 1.0f), "AUDIO" };
			else if constexpr (std::is_same_v<T, Engine::Animation>)
				return { ICON_FA_PERSON_RUNNING, ImVec4(0.95f, 0.45f, 0.75f, 1.0f), "ANIMATION" };
			else if constexpr (std::is_same_v<T, Engine::Skeleton>)
				return { ICON_FA_BONE, ImVec4(0.88f, 0.85f, 0.75f, 1.0f), "SKELETON" };
			else if constexpr (std::is_same_v<T, Engine::Shader>)
				return { ICON_FA_CODE, ImVec4(0.78f, 0.48f, 0.98f, 1.0f), "SHADER" };
			else if constexpr (std::is_same_v<T, Engine::SpriteAnimation>)
				return { ICON_FA_FILM, ImVec4(0.35f, 0.90f, 0.65f, 1.0f), "SPRITE ANIM" };
			else if constexpr (std::is_same_v<T, Engine::ParticleEffect>)
				return { ICON_FA_WAND_MAGIC_SPARKLES, ImVec4(1.0f, 0.55f, 0.10f, 1.0f), "VFX" };
			else if constexpr (std::is_same_v<T, Engine::CompositeEffect>)
				return { ICON_FA_WAND_MAGIC_SPARKLES, ImVec4(0.95f, 0.40f, 0.90f, 1.0f), "COMPOSITE VFX" };
			else
				return { ICON_FA_FILE, ImVec4(0.65f, 0.65f, 0.65f, 1.0f), "ASSET" };
		}

		inline ResourceFieldThumbnail GetResourceFieldThumbnail(Engine::Resource* resource)
		{
			ResourceFieldThumbnail result;
			if (resource == nullptr) return result;

			if (auto* texture = dynamic_cast<Engine::Texture*>(resource))
			{
				result.Texture = texture->GetRHITexture();
			}
			else if (auto* sprite = dynamic_cast<Engine::Sprite*>(resource))
			{
				auto* texture = sprite->GetTexture();
				result.Texture = texture != nullptr ? texture->GetRHITexture() : nullptr;

				const auto uv = sprite->GetUVTransform();
				if (!std::isfinite(uv.x) || !std::isfinite(uv.y) ||
					!std::isfinite(uv.z) || !std::isfinite(uv.w) || uv.z <= 0.0f || uv.w <= 0.0f)
					return {};

				result.UV0 = ImVec2(uv.x, uv.y);
				result.UV1 = ImVec2(uv.x + uv.z, uv.y + uv.w);
			}
			else if (dynamic_cast<Engine::MaterialInterface*>(resource) != nullptr ||
				dynamic_cast<Engine::Model*>(resource) != nullptr ||
				dynamic_cast<Engine::Mesh*>(resource) != nullptr)
			{
				const auto& path = resource->GetPath().empty() ? resource->GetKey() : resource->GetPath();
				if (!path.empty())
					result = GetResourcePickerThumbnail(Engine::PlatformPath::ResolveUri(path));
			}

			if (result.Texture != nullptr &&
				(result.Texture->GetDimension() != Engine::ETextureDimension::Texture2D ||
					result.Texture->GetArraySize() != 1))
				return {};

			return result;
		}

		bool DrawResourceContextMenu(const std::filesystem::path& resourcePath);
		bool RequestOpenResourceEditor(const std::filesystem::path& resourcePath);

		// 피커 목록의 한 줄(썸네일·이름·경로·배지)을 그린다. 리소스 피커와 경로 피커가 같은 모양을 쓰도록 공유한다.
		struct PickerItemView
		{
			std::string_view Id;
			std::string_view Filename;
			std::string_view SubPath;
			ResourceVisual Visual;
			std::filesystem::path ContextPath;
			ImTextureID Texture{};
			ImVec2 UV0 = ImVec2(0.0f, 0.0f);
			ImVec2 UV1 = ImVec2(1.0f, 1.0f);
			ImVec2 CropSize = ImVec2(0.0f, 0.0f); // 0보다 크면 비율을 유지해 맞춘다(스프라이트). 아니면 칸 전체를 채운다.
		};

		inline void ApplyPickerThumbnail(PickerItemView& item)
		{
			const auto thumbnail = GetResourcePickerThumbnail(item.ContextPath);
			if (!thumbnail.Texture)
				return;

			item.Texture = GetResourcePickerTextureID(thumbnail.Texture);
			item.UV0 = thumbnail.UV0;
			item.UV1 = thumbnail.UV1;
		}

		// 클릭되면 true를 돌려준다. 컨텍스트 메뉴에서 에디터 열기를 요청하면 openEditorRequested를 켠다.
		inline bool DrawPickerItem(const PickerItemView& item, bool isSelected, float thumbSize, float itemHeight, bool& openEditorRequested)
		{
			const ImVec2 cursorPos = ImGui::GetCursorScreenPos();
			const float availWidth = ImGui::GetContentRegionAvail().x;

			ImGui::PushID(item.Id.data(), item.Id.data() + item.Id.size());
			const bool clicked = ImGui::Selectable("##ItemSelectable", isSelected, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(availWidth, itemHeight));
			const bool isHovered = ImGui::IsItemHovered();

			if (DrawResourceContextMenu(item.ContextPath)) openEditorRequested = true;

			ImDrawList* drawList = ImGui::GetWindowDrawList();

			// 1) 좌측 대형 썸네일 박스 (itemHeight 세로 중앙 정렬)
			const float thumbOffsetY = (itemHeight - thumbSize) * 0.5f;
			const ImVec2 thumbMin = ImVec2(cursorPos.x + 4.0f, cursorPos.y + thumbOffsetY);
			const ImVec2 thumbMax = ImVec2(thumbMin.x + thumbSize, thumbMin.y + thumbSize);
			drawList->AddRectFilled(thumbMin, thumbMax, ImGui::GetColorU32(ImVec4(0.12f, 0.12f, 0.12f, 0.95f)), 6.0f);

			ImVec2 imgMin = thumbMin;
			ImVec2 imgMax = thumbMax;
			ImVec2 tooltipImgSize(128.0f, 128.0f);
			if (item.Texture && item.CropSize.x > 0.0f && item.CropSize.y > 0.0f)
			{
				const float scale = thumbSize / std::max(item.CropSize.x, item.CropSize.y);
				const ImVec2 fitSize(item.CropSize.x * scale, item.CropSize.y * scale);
				imgMin = ImVec2(thumbMin.x + (thumbSize - fitSize.x) * 0.5f, thumbMin.y + (thumbSize - fitSize.y) * 0.5f);
				imgMax = ImVec2(imgMin.x + fitSize.x, imgMin.y + fitSize.y);

				const float tipScale = 128.0f / std::max(item.CropSize.x, item.CropSize.y);
				tooltipImgSize = ImVec2(item.CropSize.x * tipScale, item.CropSize.y * tipScale);
			}

			if (item.Texture)
			{
				drawList->AddImage(item.Texture, imgMin, imgMax, item.UV0, item.UV1);
			}
			else
			{
				ImFont* font = ImGui::GetFont();
				const float iconFontSize = thumbSize * 0.45f;
				const ImVec2 iconTextSize = font->CalcTextSizeA(iconFontSize, FLT_MAX, 0.0f, item.Visual.Icon);
				const ImVec2 iconPos = ImVec2(thumbMin.x + (thumbSize - iconTextSize.x) * 0.5f, thumbMin.y + (thumbSize - iconTextSize.y) * 0.5f);
				drawList->AddText(font, iconFontSize, iconPos, ImGui::GetColorU32(item.Visual.Color), item.Visual.Icon);
			}
			drawList->AddRect(thumbMin, thumbMax, ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.15f)), 6.0f);

			// 2) 우측 정보 레이아웃
			const auto& visual = item.Visual;
			const float textStartX = thumbMax.x + 12.0f;
			const float lineHeight = ImGui::GetTextLineHeight();
			const float iconWidth = ImGui::CalcTextSize(visual.Icon).x;
			const float iconHeight = ImGui::CalcTextSize(visual.Icon).y;
			const float badgeTextWidth = ImGui::CalcTextSize(visual.Badge).x;
			const float badgeTextHeight = ImGui::CalcTextSize(visual.Badge).y;

			const float badgePadX = 10.0f;
			const float badgePadY = 5.0f;
			const float badgeSpacing = 7.0f;
			const float totalBadgeWidth = badgePadX * 2.0f + iconWidth + badgeSpacing + badgeTextWidth;
			const float badgeHeight = std::max(iconHeight, badgeTextHeight) + badgePadY * 2.0f;

			const float rightReserved = isSelected ? 34.0f : 12.0f;
			const float badgeStartX = cursorPos.x + availWidth - totalBadgeWidth - rightReserved;
			const float badgeStartY = cursorPos.y + (itemHeight - badgeHeight) * 0.5f;

			if (thumbSize >= 48.0f)
			{
				const ImVec2 badgeBoxMin = ImVec2(badgeStartX, badgeStartY);
				const ImVec2 badgeBoxMax = ImVec2(badgeBoxMin.x + totalBadgeWidth, badgeBoxMin.y + badgeHeight);

				drawList->AddRectFilled(badgeBoxMin, badgeBoxMax, ImGui::GetColorU32(ImVec4(visual.Color.x, visual.Color.y, visual.Color.z, 0.18f)), 6.0f);
				drawList->AddRect(badgeBoxMin, badgeBoxMax, ImGui::GetColorU32(ImVec4(visual.Color.x, visual.Color.y, visual.Color.z, 0.45f)), 6.0f, 0, 1.2f);

				const ImVec2 iconPos = ImVec2(badgeBoxMin.x + badgePadX, badgeBoxMin.y + (badgeHeight - iconHeight) * 0.5f);
				const ImVec2 textPos = ImVec2(iconPos.x + iconWidth + badgeSpacing, badgeBoxMin.y + (badgeHeight - badgeTextHeight) * 0.5f);

				drawList->AddText(iconPos, ImGui::GetColorU32(visual.Color), visual.Icon);
				drawList->AddText(textPos, ImGui::GetColorU32(ImVec4(visual.Color.x, visual.Color.y, visual.Color.z, 0.95f)), visual.Badge);
			}

			const float textCenterY = cursorPos.y + (itemHeight * 0.5f);
			const float spacingY = 3.0f;
			const float nameY = item.SubPath.empty() ? (textCenterY - lineHeight * 0.5f) : (textCenterY - lineHeight - spacingY * 0.5f);
			const float pathY = textCenterY + spacingY * 0.5f;
			const ImU32 nameColor = ImGui::GetColorU32(isSelected ? ImVec4(1.0f, 0.82f, 0.35f, 1.0f) : ImGui::GetStyleColorVec4(ImGuiCol_Text));

			drawList->AddText(ImVec2(textStartX, nameY), nameColor, item.Filename.data(), item.Filename.data() + item.Filename.size());
			if (!item.SubPath.empty())
				drawList->AddText(ImVec2(textStartX, pathY), ImGui::GetColorU32(ImGuiCol_TextDisabled), item.SubPath.data(), item.SubPath.data() + item.SubPath.size());

			if (isSelected)
			{
				const ImVec2 checkPos = ImVec2(cursorPos.x + availWidth - 24.0f, cursorPos.y + (itemHeight - 16.0f) * 0.5f);
				drawList->AddText(checkPos, ImGui::GetColorU32(ImVec4(0.35f, 0.95f, 0.45f, 1.0f)), ICON_FA_CHECK);
			}

			if (isHovered && !openEditorRequested)
			{
				ImGui::BeginTooltip();
				if (item.Texture)
				{
					ImGui::Image(item.Texture, tooltipImgSize, item.UV0, item.UV1);
					ImGui::Separator();
				}
				ImGui::TextUnformatted(item.Filename.data(), item.Filename.data() + item.Filename.size());
				ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
				ImGui::TextUnformatted(item.SubPath.data(), item.SubPath.data() + item.SubPath.size());
				ImGui::PopStyleColor();
				ImGui::EndTooltip();
			}

			ImGui::PopID();
			return clicked;
		}

		// 피커 목록 맨 위의 '연결 해제' 항목을 그린다. 클릭되면 true를 돌려준다.
		inline bool DrawPickerClearItem(bool isSelected, float thumbSize, float itemHeight)
		{
			const ImVec2 cursorPos = ImGui::GetCursorScreenPos();
			const float availWidth = ImGui::GetContentRegionAvail().x;

			ImGui::PushID("##ClearResourceItem");
			const bool clicked = ImGui::Selectable("##ClearSelectable", isSelected, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(availWidth, itemHeight));
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", LocalizeInspectorText("ResourcePicker.Clear").c_str());

			ImDrawList* drawList = ImGui::GetWindowDrawList();
			const float thumbOffsetY = (itemHeight - thumbSize) * 0.5f;
			const ImVec2 thumbMin = ImVec2(cursorPos.x + 4.0f, cursorPos.y + thumbOffsetY);
			const ImVec2 thumbMax = ImVec2(thumbMin.x + thumbSize, thumbMin.y + thumbSize);
			drawList->AddRectFilled(thumbMin, thumbMax, ImGui::GetColorU32(ImVec4(0.35f, 0.15f, 0.15f, 0.85f)), 6.0f);
			drawList->AddRect(thumbMin, thumbMax, ImGui::GetColorU32(ImVec4(0.85f, 0.35f, 0.35f, 0.6f)), 6.0f);

			ImFont* font = ImGui::GetFont();
			const float iconFontSize = thumbSize * 0.45f;
			const ImVec2 iconTextSize = font->CalcTextSizeA(iconFontSize, FLT_MAX, 0.0f, ICON_FA_BAN);
			const ImVec2 iconPos = ImVec2(thumbMin.x + (thumbSize - iconTextSize.x) * 0.5f, thumbMin.y + (thumbSize - iconTextSize.y) * 0.5f);
			drawList->AddText(font, iconFontSize, iconPos, ImGui::GetColorU32(ImVec4(0.95f, 0.40f, 0.40f, 1.0f)), ICON_FA_BAN);

			const float textStartX = thumbMax.x + 12.0f;
			drawList->AddText(ImVec2(textStartX, cursorPos.y + 6.0f), ImGui::GetColorU32(isSelected ? ImVec4(1.0f, 0.82f, 0.35f, 1.0f) : ImGui::GetStyleColorVec4(ImGuiCol_Text)), LocalizeInspectorText("ResourcePicker.Clear").c_str());
			drawList->AddText(ImVec2(textStartX, cursorPos.y + 26.0f), ImGui::GetColorU32(ImGuiCol_TextDisabled), LocalizeInspectorText("ResourcePicker.ClearHint").c_str());

			if (isSelected)
			{
				const ImVec2 checkPos = ImVec2(cursorPos.x + availWidth - 26.0f, cursorPos.y + (itemHeight - 18.0f) * 0.5f);
				drawList->AddText(checkPos, ImGui::GetColorU32(ImVec4(0.35f, 0.95f, 0.45f, 1.0f)), ICON_FA_CHECK);
			}
			ImGui::PopID();
			return clicked;
		}
	}

	template<typename T, typename LoadedFilter = std::nullptr_t, typename DiskFilter = std::nullptr_t>
	bool DrawInspectorResourcePicker(Engine::ResourceHandle<T>& value, bool canEdit, bool showThumbnail = true,
		LoadedFilter loadedFilter = nullptr,
		DiskFilter diskFilter = nullptr)
	{
		ImGui::PushID(&value);
		auto& manager = Engine::ResourceManager::Get();
		auto* current = manager.GetResource(value.GetRawHandle());
		const auto label = current != nullptr ? Engine::WStrToStr(current->GetKey()) :
			LocalizeInspectorText("ResourcePicker.None");

		bool changed = false;
		const auto errorId = ImGui::GetID("DropFailed");
		bool failed = ImGui::GetStateStorage()->GetBool(errorId, false);

		ImGui::BeginDisabled(!canEdit);

		const auto& style = ImGui::GetStyle();
		const float thumbnailSize = ImGui::GetTextLineHeight() * 2.0f + style.ItemInnerSpacing.y;
		const float fieldHeight = showThumbnail ? thumbnailSize + style.FramePadding.y * 2.0f : 0.0f;
		const auto button = showThumbnail ? std::string("###PickerButton") : label + "###PickerButton";

		if (ImGui::Button(button.c_str(), ImVec2(-1.0f, fieldHeight)))
			ImGui::OpenPopup("ResourceChoices");

		const bool fieldHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled);

		if (current != nullptr && fieldHovered && InputManager::Get().IsMouseButtonDoubleClicked(EMouseButton::Left))
		{
			const auto resPath = current->GetPath().empty() ? current->GetKey() : current->GetPath();
			Detail::RequestOpenResourceEditor(resPath);
		}

		if (showThumbnail && ImGui::IsItemVisible())
		{
			const auto min = ImGui::GetItemRectMin();
			const auto max = ImGui::GetItemRectMax();
			auto* drawList = ImGui::GetWindowDrawList();
			const auto visual = Detail::GetResourceVisual<T>(label);

			const float availableWidth = std::max(0.0f, max.x - min.x - style.FramePadding.x * 2.0f);
			const float side = std::min(thumbnailSize, availableWidth * 0.3f);
			const ImVec2 imageMin(min.x + style.FramePadding.x, min.y + (fieldHeight - side) * 0.5f);
			const ImVec2 imageMax(imageMin.x + side, imageMin.y + side);

			drawList->PushClipRect(min, max, true);
			drawList->AddRectFilled(imageMin, imageMax, ImGui::GetColorU32(ImGuiCol_FrameBg), style.FrameRounding);

			const auto thumbnail = Detail::GetResourceFieldThumbnail(current);
			const auto textureID = thumbnail.Texture != nullptr ?
				Detail::GetResourcePickerTextureID(thumbnail.Texture) : ImTextureID{};

			const float imageWidth = thumbnail.Texture != nullptr ?
				thumbnail.Texture->GetWidth() * (thumbnail.UV1.x - thumbnail.UV0.x) : 0.0f;
			const float imageHeight = thumbnail.Texture != nullptr ?
				thumbnail.Texture->GetHeight() * (thumbnail.UV1.y - thumbnail.UV0.y) : 0.0f;

			if (textureID != ImTextureID{} && imageWidth > 0.0f && imageHeight > 0.0f)
			{
				const float scale = side / std::max(imageWidth, imageHeight);
				const ImVec2 size(imageWidth * scale, imageHeight * scale);
				const ImVec2 start(imageMin.x + (side - size.x) * 0.5f, imageMin.y + (side - size.y) * 0.5f);
				const ImVec2 end(start.x + size.x, start.y + size.y);

				drawList->AddImage(textureID, start, end, thumbnail.UV0, thumbnail.UV1,
					ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f)));
			}
			else
			{
				const auto iconSize = ImGui::CalcTextSize(visual.Icon);
				const ImVec2 iconPosition(imageMin.x + (side - iconSize.x) * 0.5f,
					imageMin.y + (side - iconSize.y) * 0.5f);

				drawList->AddText(iconPosition, ImGui::GetColorU32(visual.Color), visual.Icon);
			}

			const auto separator = label.find_last_of("/\\");
			const auto name = current != nullptr && separator != std::string::npos ?
				label.substr(separator + 1) : label;

			const float textX = imageMax.x + style.ItemInnerSpacing.x;
			const float textY = min.y + style.FramePadding.y;
			const ImVec2 textMin(textX, min.y);
			const ImVec2 textMax(std::max(textX, max.x - style.FramePadding.x), max.y);

			drawList->PushClipRect(textMin, textMax, true);
			drawList->AddText(ImVec2(textX, textY), ImGui::GetColorU32(ImGuiCol_Text), name.c_str());

			if (current != nullptr)
			{
				const float pathY = textY + ImGui::GetTextLineHeight() + style.ItemInnerSpacing.y;
				drawList->AddText(ImVec2(textX, pathY), ImGui::GetColorU32(ImGuiCol_TextDisabled), label.c_str());
			}

			drawList->PopClipRect();
			drawList->PopClipRect();
		}

		if (canEdit && ImGui::BeginDragDropTarget())
		{
			if (const auto* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
			{
				const auto path = DecodeInspectorResourcePath(payload->Data, payload->DataSize);
				std::wstring targetPath = path ? *path : L"";
				// FBX 등 모델 소스 파일이 드롭된 경우, 이미 변환된 .bammodel이 있으면 스마트 연결
				if constexpr (std::is_same_v<T, Engine::Model>)
				{
					std::filesystem::path p(targetPath);
					std::string ext = p.extension().string();
					std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
					if (ext == ".fbx" || ext == ".obj" || ext == ".gltf" || ext == ".glb")
					{
						auto cand1 = p.parent_path() / (p.stem().wstring() + L".bammodel");
						auto cand2 = p.parent_path() / (p.stem().wstring() + L".bammodel.json");
						if (std::filesystem::exists(cand1)) targetPath = cand1.wstring();
						else if (std::filesystem::exists(cand2)) targetPath = cand2.wstring();
					}
				}
				const auto handle = !targetPath.empty() ? manager.LoadFile(targetPath) : Engine::Handle{};
				if constexpr (!std::is_same_v<LoadedFilter, std::nullptr_t>)
				{
					if (handle.IsValid() && loadedFilter)
					{
						auto* res = dynamic_cast<T*>(manager.GetResource(handle));
						if (res && !loadedFilter(res)) { changed = false; failed = true; }
						else changed = TryAssignInspectorResource(value, handle);
					}
					else changed = TryAssignInspectorResource(value, handle);
				}
				else
				{
					changed = TryAssignInspectorResource(value, handle);
				}
				failed = !changed;
			}
			ImGui::EndDragDropTarget();
		}

		if (current != nullptr)
			Detail::DrawResourceContextMenu(current->GetPath().empty() ? current->GetKey() : current->GetPath());

		if (fieldHovered)
			ImGui::SetTooltip("%s", label.c_str());

		// 팝업 기본 크기를 740x580으로 넉넉하게 확장하고, 최대 1600x1200까지 자유롭게 리사이즈 지원
		ImGui::SetNextWindowSize(ImVec2(740.0f, 580.0f), ImGuiCond_Appearing);
		ImGui::SetNextWindowSizeConstraints(ImVec2(480.0f, 360.0f), ImVec2(1600.0f, 1200.0f));

		if (ImGui::BeginPopup("ResourceChoices"))
		{
			struct CachedAssetEntry
			{
				std::string Filename;
				std::string SubPath;
				std::string LowerFilename;
				std::string LowerSubPath;
				std::filesystem::path FullPath;
				std::wstring Key;
				std::string UniqueId;
				bool IsEngineAsset = false;
				Engine::Handle Handle;
			};
			static std::vector<CachedAssetEntry> s_CachedLoadedEntries;
			static std::vector<CachedAssetEntry> s_CachedDiskFiles;
			static std::unordered_set<std::wstring> s_LoadedKeys;
			static std::uint64_t s_DiskRevision = 0;
			static std::vector<size_t> s_FilteredLoadedIndices;
			static std::vector<size_t> s_FilteredDiskIndices;
			static std::string s_LastSearchQuery;
			static int s_LastCategoryScope = -1;
			static char s_SearchBuffer[128] = "";
			static float s_ThumbnailSize = 64.0f;
			static int s_CategoryScope = 0; // 0: All, 1: Project, 2: Engine Built-in

			auto isLoadedEntryEngineBuiltIn = [](const std::wstring& key, const std::wstring& path) -> bool
				{
					const auto& target = path.empty() ? key : path;
					if (Engine::PlatformPath::IsReadOnlyTarget(target)) return true;
					std::error_code ec;
					const auto resolved = Engine::PlatformPath::ResolveUri(target);
					if (resolved.empty() || !std::filesystem::exists(resolved, ec)) return true;
					return false;
				};

			const bool isAppearing = ImGui::IsWindowAppearing();
			const auto& fileCatalog = Detail::GetPickerAssetCatalog();

			// Refresh loaded resources when the popup opens. The disk catalog has its own revision.
			if (isAppearing)
			{
				s_SearchBuffer[0] = '\0';
				s_LastSearchQuery.clear();
				s_LastCategoryScope = -1;
				s_CachedLoadedEntries.clear();
				s_LoadedKeys.clear();
				for (const auto& entry : manager.GetDebugSnapshot())
				{
					const auto handle = manager.FindHandleByKey(entry.Key);
					auto* resource = manager.GetResource(handle);
					auto* typedRes = dynamic_cast<T*>(resource);
					if (typedRes == nullptr) continue;
					if constexpr (!std::is_same_v<LoadedFilter, std::nullptr_t>)
					{
						if (loadedFilter && !loadedFilter(typedRes)) continue;
					}

					const auto& resourcePath = entry.Path.empty() ? entry.Key : entry.Path;
					const auto absolutePath = Engine::PlatformPath::ResolveUri(resourcePath);
					const auto relativePath = Engine::PlatformPath::ToProjectRelative(absolutePath);
					const auto normKey = Detail::NormalizeResourceKey(relativePath.wstring());
					s_LoadedKeys.insert(normKey);

					const bool isEngine = isLoadedEntryEngineBuiltIn(entry.Key, entry.Path);
					const std::filesystem::path p(normKey);
					const std::string filename = Engine::WStrToStr(p.filename().wstring());
					const std::string parentPath = Engine::WStrToStr(p.parent_path().generic_wstring());
					const std::string displaySubPath = isEngine ? (parentPath.empty() ? "[Engine]" : ("[Engine] " + parentPath)) : parentPath;

					std::string lowerName = filename;
					for (char& c : lowerName) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
					std::string lowerSub = displaySubPath;
					for (char& c : lowerSub) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

					s_CachedLoadedEntries.push_back({
						filename, displaySubPath, std::move(lowerName), std::move(lowerSub),
						absolutePath, normKey, "Mem_" + Engine::WStrToStr(normKey), isEngine, handle
						});
				}
			}

			if (s_DiskRevision != fileCatalog.Revision)
			{
				s_DiskRevision = fileCatalog.Revision;
				s_LastCategoryScope = -1;
				s_CachedDiskFiles.clear();
				std::unordered_set<std::wstring> diskKeys;
				for (const auto& file : fileCatalog.Files)
				{
					if (!Detail::IsResourceExtensionCompatible<T>(file.LowerFilename))
						continue;
					if constexpr (!std::is_same_v<DiskFilter, std::nullptr_t>)
					{
						if (diskFilter && !diskFilter(file.LowerFilename))
							continue;
					}

					const auto relativePath = Engine::PlatformPath::ToProjectRelative(file.Path);
					auto key = Detail::NormalizeResourceKey(relativePath.wstring());
					if (!diskKeys.insert(key).second)
						continue;

					const bool isEngine = Engine::PlatformPath::IsReadOnlyTarget(file.Path);
					const std::string parentPath = Engine::WStrToStr(file.Path.parent_path().generic_wstring());
					const std::string subPath = isEngine ? (parentPath.empty() ? "[Engine]" : "[Engine] " + parentPath) : parentPath;
					auto lowerSubPath = subPath;
					for (char& ch : lowerSubPath)
						ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));

					s_CachedDiskFiles.push_back({ file.Filename, subPath, file.LowerFilename, std::move(lowerSubPath),
						file.Path, std::move(key), "Disk_" + Engine::WStrToStr(file.Path.wstring()), isEngine, {} });
				}
			}

			// 검색 바 오른쪽에 썸네일 크기 조절 공간을 예약한다.
			const float sliderWidth = 110.0f;
			EditorUI::SearchBarOptions searchOptions;
			searchOptions.Width = std::max(1.0f, ImGui::GetContentRegionAvail().x - sliderWidth - ImGui::GetStyle().ItemSpacing.x);
			searchOptions.FocusOnDraw = isAppearing;
			EditorUI::DrawSearchBar("ResourcePickerSearch", s_SearchBuffer, sizeof(s_SearchBuffer), "ResourcePicker.SearchHint", searchOptions);
			ImGui::SameLine();
			ImGui::SetNextItemWidth(sliderWidth);
			ImGui::SliderFloat("##ThumbSizeSlider", &s_ThumbnailSize, 40.0f, 120.0f, "%.0f px");
			if (ImGui::IsItemHovered())
			{
				const std::string thumbnailHint = fmt::format(fmt::runtime(LocalizeInspectorText("ResourcePicker.ThumbnailSizeHint")), 40, 120);
				ImGui::SetTooltip("%s", thumbnailHint.c_str());
			}

			// ── 2-B. 카테고리 필터 버튼 바 (All / Project / Engine Built-in) ──
			{
				const std::string scopeAllLabel = LocalizeInspectorText("ResourcePicker.ScopeAll") + "###ScopeAll";
				const std::string scopeProjLabel = ICON_FA_DATABASE " " + LocalizeInspectorText("ResourcePicker.ScopeProject") + "###ScopeProject";
				const std::string scopeEngLabel = ICON_FA_GEARS " " + LocalizeInspectorText("ResourcePicker.ScopeEngine") + "###ScopeEngine";

				auto drawScopeTab = [&](const std::string& btnLabel, int scopeIndex)
					{
						const bool active = (s_CategoryScope == scopeIndex);
						if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
						if (ImGui::SmallButton(btnLabel.c_str())) s_CategoryScope = scopeIndex;
						if (active) ImGui::PopStyleColor();
					};

				drawScopeTab(scopeAllLabel, 0);
				ImGui::SameLine();
				drawScopeTab(scopeProjLabel, 1);
				ImGui::SameLine();
				drawScopeTab(scopeEngLabel, 2);
			}

			// ── 2-C. 더티 플래그 기반 필터링 인덱스 갱신 ──
			const std::string searchStr = s_SearchBuffer;
			if (searchStr != s_LastSearchQuery || s_CategoryScope != s_LastCategoryScope)
			{
				s_LastSearchQuery = searchStr;
				s_LastCategoryScope = s_CategoryScope;
				s_FilteredLoadedIndices.clear();
				s_FilteredDiskIndices.clear();

				std::string sLower = searchStr;
				for (char& c : sLower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

				auto passesFilter = [&](const CachedAssetEntry& item) -> bool
					{
						if (s_CategoryScope == 1 && item.IsEngineAsset) return false;
						if (s_CategoryScope == 2 && !item.IsEngineAsset) return false;
						if (sLower.empty()) return true;
						return item.LowerFilename.find(sLower) != std::string::npos || item.LowerSubPath.find(sLower) != std::string::npos;
					};

				for (size_t i = 0; i < s_CachedLoadedEntries.size(); ++i)
				{
					if (passesFilter(s_CachedLoadedEntries[i])) s_FilteredLoadedIndices.push_back(i);
				}
				for (size_t i = 0; i < s_CachedDiskFiles.size(); ++i)
				{
					const auto& entry = s_CachedDiskFiles[i];
					if (!s_LoadedKeys.contains(entry.Key) && passesFilter(entry))
						s_FilteredDiskIndices.push_back(i);
				}
			}

			ImGui::Separator();

			// ── 3. 대형 썸네일 + 3단 텍스트 항목 렌더링 규격 및 람다 ──
			const auto& style = ImGui::GetStyle();
			const float thumbSize = std::clamp(s_ThumbnailSize, 40.0f, 120.0f);
			const float minTextHeight = ImGui::GetTextLineHeight() * 2.0f + 10.0f;
			const float itemHeight = std::max(thumbSize + 8.0f, minTextHeight);
			const float itemStride = itemHeight + style.ItemSpacing.y;

			bool openEditorRequested = false;
			auto drawResourceItem = [&](const CachedAssetEntry& entry, bool isSelected, auto onSelect)
				{
					Detail::PickerItemView view{ entry.UniqueId, entry.Filename, entry.SubPath, Detail::GetResourceVisual<T>(entry.Filename), entry.FullPath };
					if constexpr (std::is_same_v<T, Engine::Texture>)
					{
						if (entry.Handle.IsValid())
						{
							const auto* texture = dynamic_cast<Engine::Texture*>(manager.GetResource(entry.Handle));
							if (texture && texture->GetRHITexture())
								view.Texture = Detail::GetResourcePickerTextureID(texture->GetRHITexture());
						}
					}
					if (!view.Texture && !entry.FullPath.empty())
					{
						if constexpr (std::is_same_v<T, Engine::Sprite>)
						{
							Engine::Handle spHandle = entry.Handle.IsValid() ? entry.Handle : manager.FindHandleByFilePath(entry.FullPath);
							if (!spHandle.IsValid())
							{
								const std::wstring rel = Engine::PlatformPath::ToProjectRelative(entry.FullPath).wstring();
								spHandle = manager.FindHandleByKey(rel);
								if (!spHandle.IsValid() && rel.size() > 5 && rel.ends_with(L".json"))
									spHandle = manager.FindHandleByKey(rel.substr(0, rel.size() - 5));
							}
							if (auto* sp = dynamic_cast<Engine::Sprite*>(manager.GetResource(spHandle)))
							{
								const auto fieldThumb = Detail::GetResourceFieldThumbnail(sp);
								if (fieldThumb.Texture)
								{
									view.Texture = Detail::GetResourcePickerTextureID(fieldThumb.Texture);
									view.UV0 = fieldThumb.UV0;
									view.UV1 = fieldThumb.UV1;
									view.CropSize = ImVec2(static_cast<float>(fieldThumb.Texture->GetWidth()) * (view.UV1.x - view.UV0.x), static_cast<float>(fieldThumb.Texture->GetHeight()) * (view.UV1.y - view.UV0.y));
								}
							}
						}
						if (!view.Texture)
							Detail::ApplyPickerThumbnail(view);
					}
					if (Detail::DrawPickerItem(view, isSelected, thumbSize, itemHeight, openEditorRequested))
						onSelect();
				};

			// ── 4. 리스트 스크롤 영역 (ImGuiListClipper 기반 가상 스크롤) ──
			if (ImGui::BeginChild("##ResourceListRegion", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar))
			{
				// 최상단: 리소스 할당 해제 (None / Clear)
				if (Detail::DrawPickerClearItem(current == nullptr, thumbSize, itemHeight))
				{
					value.Reset();
					changed = true;
					failed = false;
					s_SearchBuffer[0] = '\0';
					ImGui::CloseCurrentPopup();
				}

				ImGui::Spacing();
				ImGui::Separator();
				ImGui::Spacing();

				// ── 5. 메모리에 로드된 리소스 섹션 (가상 스크롤) ──
				ImGui::PushID("LoadedInMemory_Section");
				const std::string loadedHeader = fmt::format("{} ({})", LocalizeInspectorText("ResourcePicker.LoadedInMemory"), s_FilteredLoadedIndices.size());
				ImGui::TextDisabled("%s", loadedHeader.c_str());

				if (!s_FilteredLoadedIndices.empty())
				{
					ImGuiListClipper memClipper;
					memClipper.Begin(static_cast<int>(s_FilteredLoadedIndices.size()), itemStride);
					while (memClipper.Step())
					{
						for (int i = memClipper.DisplayStart; i < memClipper.DisplayEnd; ++i)
						{
							const auto& entry = s_CachedLoadedEntries[s_FilteredLoadedIndices[i]];
							const bool isSelected = (entry.Handle == value.GetRawHandle());
							drawResourceItem(entry, isSelected, [&]()
								{
									changed = TryAssignInspectorResource(value, entry.Handle);
									failed = !changed;
									s_SearchBuffer[0] = '\0';
									ImGui::CloseCurrentPopup();
								});
						}
					}
				}
				ImGui::PopID(); // LoadedInMemory_Section

				ImGui::Spacing();
				ImGui::Separator();
				ImGui::Spacing();

				// ── 6. 디스크 상의 프로젝트 파일 섹션 (가상 스크롤) ──
				ImGui::PushID("DiskFiles_Section");
				const std::string diskHeader = fmt::format("{} ({})", LocalizeInspectorText("ResourcePicker.ProjectFiles"), s_FilteredDiskIndices.size());
				ImGui::TextDisabled("%s", diskHeader.c_str());

				if (!fileCatalog.IsReady)
					EditorUI::EmptyState("ResourcePicker.IndexingFiles");
				else if (!s_FilteredDiskIndices.empty())
				{
					ImGuiListClipper diskClipper;
					diskClipper.Begin(static_cast<int>(s_FilteredDiskIndices.size()), itemStride);
					while (diskClipper.Step())
					{
						for (int i = diskClipper.DisplayStart; i < diskClipper.DisplayEnd; ++i)
						{
							const auto& file = s_CachedDiskFiles[s_FilteredDiskIndices[i]];
							const bool isSelected = false;
							drawResourceItem(file, isSelected, [&]()
								{
									const auto handle = manager.LoadFile(file.Key);
									changed = TryAssignInspectorResource(value, handle);
									failed = !changed;
									s_SearchBuffer[0] = '\0';
									ImGui::CloseCurrentPopup();
								});
						}
					}
				}
				ImGui::PopID(); // DiskFiles_Section
			}
			ImGui::EndChild();

			if (openEditorRequested) ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		ImGui::EndDisabled();
		ImGui::GetStateStorage()->SetBool(errorId, failed);

		if (failed)
			ImGui::TextDisabled("%s", LocalizeInspectorText("ResourcePicker.Rejected").c_str());

		ImGui::PopID();
		return changed;
	}
}
