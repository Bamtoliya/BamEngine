#pragma once

#include "ReflectionInspectorSupport.h"
#include "Functions.h"
#include "AssetCache.h"
#include "ImGuiManager.h"
#include "IconsFontAwesome7.h"
#include <imgui.h>
#include <fmt/format.h>
#include <string_view>
#include <cctype>
#include <algorithm>
#include <filesystem>
#include <unordered_set>

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
}

namespace Editor
{
	namespace Detail
	{
		template<typename T>
		inline bool IsResourceExtensionCompatible(std::string_view filenameOrExt)
		{
			std::string lower;
			lower.reserve(filenameOrExt.size());
			for (char c : filenameOrExt) lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
			auto endsWith = [&](std::string_view suffix) {
				return lower.size() >= suffix.size() &&
					std::string_view(lower).substr(lower.size() - suffix.size()) == suffix;
				};
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
				return endsWith(".fbx") || endsWith(".obj") || endsWith(".gltf") || endsWith(".glb") ||
					endsWith(".bammodel") || endsWith(".bammodel.json");
			}
			else if constexpr (std::is_same_v<T, Engine::Mesh>)
			{
				return endsWith(".fbx") || endsWith(".obj") || endsWith(".gltf") || endsWith(".glb") ||
					endsWith(".bammesh") || endsWith(".bammesh.json") ||
					endsWith(".bammodel") || endsWith(".bammodel.json");
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
				return endsWith(".bamanim") || endsWith(".bamanim.json") || endsWith(".fbx");
			}
			else if constexpr (std::is_same_v<T, Engine::Skeleton>)
			{
				return endsWith(".bamskel") || endsWith(".bamskel.json") || endsWith(".fbx");
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
			else
				return { ICON_FA_FILE, ImVec4(0.65f, 0.65f, 0.65f, 1.0f), "ASSET" };
		}
	}

	template<typename T>
	bool DrawInspectorResourcePicker(Engine::ResourceHandle<T>& value, bool canEdit)
	{
		auto& manager = Engine::ResourceManager::Get();
		auto* current = manager.GetResource(value.GetRawHandle());
		const auto label = current != nullptr ? Engine::WStrToStr(current->GetKey()) :
			LocalizeInspectorText("UI_RESOURCE_NONE");

		bool changed = false;
		const auto errorId = ImGui::GetID("DropFailed");
		bool failed = ImGui::GetStateStorage()->GetBool(errorId, false);

		ImGui::BeginDisabled(!canEdit);

		const auto button = label + "###ResourcePicker";
		if (ImGui::Button(button.c_str(), ImVec2(-1.0f, 0.0f)))
			ImGui::OpenPopup("ResourceChoices");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", label.c_str());

		if (canEdit && ImGui::BeginDragDropTarget())
		{
			if (const auto* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
			{
				const auto path = DecodeInspectorResourcePath(payload->Data, payload->DataSize);
				const auto handle = path ? manager.LoadFile(*path) : Engine::Handle{};
				changed = TryAssignInspectorResource(value, handle);
				failed = !changed;
			}
			ImGui::EndDragDropTarget();
		}

		// 팝업 기본 크기를 740x580으로 넉넉하게 확장하고, 최대 1600x1200까지 자유롭게 리사이즈 지원
		ImGui::SetNextWindowSize(ImVec2(740.0f, 580.0f), ImGuiCond_Appearing);
		ImGui::SetNextWindowSizeConstraints(ImVec2(480.0f, 360.0f), ImVec2(1600.0f, 1200.0f));

		if (ImGui::BeginPopup("ResourceChoices"))
		{
			struct DiskAssetEntry
			{
				std::string Filename;
				std::string FullPath;
				std::wstring Key;
			};
			static std::vector<DiskAssetEntry> s_CachedDiskFiles;
			static char s_SearchBuffer[128] = "";
			static float s_ThumbnailSize = 64.0f; // 기본 64px (기존 36px 대비 1.8배 확대)

			const bool isAppearing = ImGui::IsWindowAppearing();

			// ── 1. 팝업 진입 시 1회만 디스크 스캔 및 캐싱 ──
			if (isAppearing)
			{
				s_SearchBuffer[0] = '\0';
				s_CachedDiskFiles.clear();

				std::unordered_set<std::wstring> loadedNormalizedKeys;
				for (const auto& entry : manager.GetDebugSnapshot())
				{
					const auto handle = manager.FindHandleByKey(entry.Key);
					if (dynamic_cast<T*>(manager.GetResource(handle)) != nullptr)
						loadedNormalizedKeys.insert(Detail::NormalizeResourceKey(entry.Key));
				}

				std::error_code ec;
				const std::filesystem::path resDir = Engine::PlatformPath::ResolveProjectRelative("Resources");
				if (std::filesystem::exists(resDir, ec))
				{
					for (const auto& fileEntry : std::filesystem::recursive_directory_iterator(resDir, std::filesystem::directory_options::skip_permission_denied, ec))
					{
						if (!fileEntry.is_regular_file()) continue;
						const auto filename = fileEntry.path().filename().string();
						if (!Detail::IsResourceExtensionCompatible<T>(filename)) continue;
						const auto normKey = Detail::NormalizeResourceKey(fileEntry.path().wstring());
						if (loadedNormalizedKeys.contains(normKey)) continue;
						s_CachedDiskFiles.push_back({
							fileEntry.path().filename().string(),
							Engine::WStrToStr(normKey),
							normKey
							});
					}
				}
			}

			// ── 2. 상단 툴바: [돋보기] [검색창] [X] [크기 슬라이더] ──
			ImGui::AlignTextToFramePadding();
			ImGui::Text(ICON_FA_MAGNIFYING_GLASS);
			ImGui::SameLine();

			if (isAppearing) ImGui::SetKeyboardFocusHere();

			const float clearBtnWidth = 26.0f;
			const float sliderWidth = 110.0f;
			const float spacing = ImGui::GetStyle().ItemSpacing.x;
			const float searchInputWidth = ImGui::GetContentRegionAvail().x - clearBtnWidth - sliderWidth - (spacing * 2.0f);

			ImGui::SetNextItemWidth(searchInputWidth);
			ImGui::InputTextWithHint("##ResourceSearchInput", "Search resources...", s_SearchBuffer, sizeof(s_SearchBuffer));

			ImGui::SameLine();
			if (ImGui::Button(ICON_FA_XMARK "##ClearSearch", ImVec2(clearBtnWidth, 0.0f)))
				s_SearchBuffer[0] = '\0';

			ImGui::SameLine();
			ImGui::SetNextItemWidth(sliderWidth);
			ImGui::SliderFloat("##ThumbSizeSlider", &s_ThumbnailSize, 40.0f, 120.0f, "%.0f px");
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Thumbnail Size (40px ~ 120px)");

			const std::string searchStr = s_SearchBuffer;
			auto matchesFilter = [&](std::string_view name, std::string_view path) -> bool
				{
					if (searchStr.empty()) return true;
					auto toLower = [](std::string_view s) {
						std::string res;
						res.reserve(s.size());
						for (char c : s) res.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
						return res;
						};
					const std::string sLower = toLower(searchStr);
					return toLower(name).find(sLower) != std::string::npos || toLower(path).find(sLower) != std::string::npos;
				};

			ImGui::Separator();

			// ── 3. 대형 썸네일 + 3단 텍스트 항목 렌더링 람다 ──
			auto drawResourceItem = [&](const std::string& uniqueId, const std::string& displayName,
				const std::string& subPath, const std::filesystem::path& assetPath, bool isSelected, auto onSelect)
				{
					const float thumbSize = std::clamp(s_ThumbnailSize, 40.0f, 120.0f);
					const float itemHeight = thumbSize + 8.0f;
					const ImVec2 cursorPos = ImGui::GetCursorScreenPos();
					const float availWidth = ImGui::GetContentRegionAvail().x;

					ImGui::PushID(uniqueId.c_str());
					const bool clicked = ImGui::Selectable("##ItemSelectable", isSelected,
						ImGuiSelectableFlags_AllowDoubleClick, ImVec2(availWidth, itemHeight));
					const bool isHovered = ImGui::IsItemHovered();

					ImDrawList* drawList = ImGui::GetWindowDrawList();

					// 1) 좌측 대형 썸네일 박스
					const ImVec2 thumbMin = ImVec2(cursorPos.x + 4.0f, cursorPos.y + 4.0f);
					const ImVec2 thumbMax = ImVec2(thumbMin.x + thumbSize, thumbMin.y + thumbSize);
					drawList->AddRectFilled(thumbMin, thumbMax, ImGui::GetColorU32(ImVec4(0.12f, 0.12f, 0.12f, 0.95f)), 6.0f);

					ImTextureID texID = 0;
					if (!assetPath.empty())
					{
						if (auto* rhiTex = AssetCache::Get().GetThumbnail(assetPath))
							texID = ImGuiManager::Get().GetImGuiTextureID(rhiTex);
					}

					if (texID)
					{
						drawList->AddImage(texID, thumbMin, thumbMax);
					}
					else
					{
						const auto visual = Detail::GetResourceVisual<T>(displayName);
						ImFont* font = ImGui::GetFont();
						const float iconFontSize = thumbSize * 0.45f;
						const ImVec2 iconTextSize = font->CalcTextSizeA(iconFontSize, FLT_MAX, 0.0f, visual.Icon);
						const ImVec2 iconPos = ImVec2(thumbMin.x + (thumbSize - iconTextSize.x) * 0.5f,
							thumbMin.y + (thumbSize - iconTextSize.y) * 0.5f);
						drawList->AddText(font, iconFontSize, iconPos, ImGui::GetColorU32(visual.Color), visual.Icon);
					}
					drawList->AddRect(thumbMin, thumbMax, ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.15f)), 6.0f);

					// 2) 우측 정보 레이아웃 (우측 배지 정렬로 세로 겹침 원천 차단)
					const float textStartX = thumbMax.x + 12.0f;
					const auto visual = Detail::GetResourceVisual<T>(displayName);
					const float lineHeight = ImGui::GetTextLineHeight();
					// 우측 타입 배지 크기 및 위치 동적 정밀 계산 (상하/좌우 여백 확대 및 아이콘 넘침 방지)
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

					// 타입 배지 렌더링 (썸네일 크기 48px 이상일 때 우측에 넉넉한 캡슐 박스로 렌더링)
					if (thumbSize >= 48.0f)
					{
						const ImVec2 badgeBoxMin = ImVec2(badgeStartX, badgeStartY);
						const ImVec2 badgeBoxMax = ImVec2(badgeBoxMin.x + totalBadgeWidth, badgeBoxMin.y + badgeHeight);

						drawList->AddRectFilled(badgeBoxMin, badgeBoxMax,
							ImGui::GetColorU32(ImVec4(visual.Color.x, visual.Color.y, visual.Color.z, 0.18f)), 6.0f);
						drawList->AddRect(badgeBoxMin, badgeBoxMax,
							ImGui::GetColorU32(ImVec4(visual.Color.x, visual.Color.y, visual.Color.z, 0.45f)), 6.0f, 0, 1.2f);

						// 아이콘과 텍스트를 배지 세로 정중앙에 정확히 배치하여 테두리 밖 넘침 원천 차단
						const ImVec2 iconPos = ImVec2(badgeBoxMin.x + badgePadX, badgeBoxMin.y + (badgeHeight - iconHeight) * 0.5f);
						const ImVec2 textPos = ImVec2(iconPos.x + iconWidth + badgeSpacing, badgeBoxMin.y + (badgeHeight - badgeTextHeight) * 0.5f);

						drawList->AddText(iconPos, ImGui::GetColorU32(visual.Color), visual.Icon);
						drawList->AddText(textPos,
							ImGui::GetColorU32(ImVec4(visual.Color.x, visual.Color.y, visual.Color.z, 0.95f)), visual.Badge);
					}
					// 중앙 텍스트 (썸네일 높이 중앙을 기준으로 2행 완벽 정렬)
					const float textCenterY = cursorPos.y + (itemHeight * 0.5f);
					const float spacingY = 3.0f;
					const float nameY = subPath.empty() ? (textCenterY - lineHeight * 0.5f) : (textCenterY - lineHeight - spacingY * 0.5f);
					const float pathY = textCenterY + spacingY * 0.5f;
					// 1행: 파일명
					drawList->AddText(ImVec2(textStartX, nameY),
						ImGui::GetColorU32(isSelected ? ImVec4(1.0f, 0.82f, 0.35f, 1.0f) : ImGui::GetStyleColorVec4(ImGuiCol_Text)),
						displayName.c_str());
					// 2행: 상대 경로
					if (!subPath.empty())
					{
						drawList->AddText(ImVec2(textStartX, pathY),
							ImGui::GetColorU32(ImGuiCol_TextDisabled), subPath.c_str());
					}
					// 3) 선택 상태 체크마크 (최우측)
					if (isSelected)
					{
						const ImVec2 checkPos = ImVec2(cursorPos.x + availWidth - 24.0f, cursorPos.y + (itemHeight - 16.0f) * 0.5f);
						drawList->AddText(checkPos, ImGui::GetColorU32(ImVec4(0.35f, 0.95f, 0.45f, 1.0f)), ICON_FA_CHECK);
					}

					// 4) 마우스 호버 시 128x128 대형 프리뷰 툴팁 제공
					if (isHovered)
					{
						ImGui::BeginTooltip();
						if (texID)
						{
							ImGui::Image(texID, ImVec2(128.0f, 128.0f));
							ImGui::Separator();
						}
						ImGui::Text("%s", displayName.c_str());
						ImGui::TextDisabled("%s", subPath.c_str());
						ImGui::EndTooltip();
					}

					if (clicked)
						onSelect();

					ImGui::PopID();
				};

			// ── 4. 리스트 스크롤 영역 ──
			if (ImGui::BeginChild("##ResourceListRegion", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar))
			{
				// 최상단: 리소스 할당 해제 (None / Clear)
				{
					const bool isNoneSelected = (current == nullptr);
					const float thumbSize = std::clamp(s_ThumbnailSize, 40.0f, 120.0f);
					const float itemHeight = thumbSize + 8.0f;
					const ImVec2 cursorPos = ImGui::GetCursorScreenPos();
					const float availWidth = ImGui::GetContentRegionAvail().x;

					ImGui::PushID("##ClearResourceItem");
					if (ImGui::Selectable("##ClearSelectable", isNoneSelected, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(availWidth, itemHeight)))
					{
						value.Reset();
						changed = true;
						failed = false;
						s_SearchBuffer[0] = '\0';
						ImGui::CloseCurrentPopup();
					}
					if (ImGui::IsItemHovered())
						ImGui::SetTooltip("%s", LocalizeInspectorText("UI_RESOURCE_CLEAR").c_str());

					ImDrawList* drawList = ImGui::GetWindowDrawList();
					const ImVec2 thumbMin = ImVec2(cursorPos.x + 4.0f, cursorPos.y + 4.0f);
					const ImVec2 thumbMax = ImVec2(thumbMin.x + thumbSize, thumbMin.y + thumbSize);
					drawList->AddRectFilled(thumbMin, thumbMax, ImGui::GetColorU32(ImVec4(0.35f, 0.15f, 0.15f, 0.85f)), 6.0f);
					drawList->AddRect(thumbMin, thumbMax, ImGui::GetColorU32(ImVec4(0.85f, 0.35f, 0.35f, 0.6f)), 6.0f);

					ImFont* font = ImGui::GetFont();
					const float iconFontSize = thumbSize * 0.45f;
					const ImVec2 iconTextSize = font->CalcTextSizeA(iconFontSize, FLT_MAX, 0.0f, ICON_FA_BAN);
					const ImVec2 iconPos = ImVec2(thumbMin.x + (thumbSize - iconTextSize.x) * 0.5f,
						thumbMin.y + (thumbSize - iconTextSize.y) * 0.5f);
					drawList->AddText(font, iconFontSize, iconPos, ImGui::GetColorU32(ImVec4(0.95f, 0.40f, 0.40f, 1.0f)), ICON_FA_BAN);

					const float textStartX = thumbMax.x + 12.0f;
					drawList->AddText(ImVec2(textStartX, cursorPos.y + 6.0f),
						ImGui::GetColorU32(isNoneSelected ? ImVec4(1.0f, 0.82f, 0.35f, 1.0f) : ImGui::GetStyleColorVec4(ImGuiCol_Text)),
						LocalizeInspectorText("UI_RESOURCE_CLEAR").c_str());
					drawList->AddText(ImVec2(textStartX, cursorPos.y + 26.0f),
						ImGui::GetColorU32(ImGuiCol_TextDisabled), "Unbind / Clear current resource");

					if (isNoneSelected)
					{
						const ImVec2 checkPos = ImVec2(cursorPos.x + availWidth - 26.0f, cursorPos.y + (itemHeight - 18.0f) * 0.5f);
						drawList->AddText(checkPos, ImGui::GetColorU32(ImVec4(0.35f, 0.95f, 0.45f, 1.0f)), ICON_FA_CHECK);
					}
					ImGui::PopID();
				}
				ImGui::Separator();

				// ── 5. 메모리에 로드된 리소스 섹션 ──
				ImGui::PushID("LoadedInMemory_Section");

				size_t loadedMatchCount = 0;
				for (const auto& entry : manager.GetDebugSnapshot())
				{
					const auto handle = manager.FindHandleByKey(entry.Key);
					if (dynamic_cast<T*>(manager.GetResource(handle)) == nullptr) continue;
					const auto normStr = Engine::WStrToStr(Detail::NormalizeResourceKey(entry.Key));
					const std::filesystem::path p(normStr);
					if (matchesFilter(p.filename().string(), p.parent_path().generic_string()))
						++loadedMatchCount;
				}

				const std::string loadedHeader = fmt::format("{} ({})", LocalizeInspectorText("ResourcePicker.LoadedInMemory"), loadedMatchCount);
				ImGui::TextDisabled("%s", loadedHeader.c_str());

				for (const auto& entry : manager.GetDebugSnapshot())
				{
					const auto handle = manager.FindHandleByKey(entry.Key);
					if (dynamic_cast<T*>(manager.GetResource(handle)) == nullptr) continue;

					const auto normKey = Detail::NormalizeResourceKey(entry.Key);
					const auto normStr = Engine::WStrToStr(normKey);
					const std::filesystem::path p(normStr);
					const std::string filename = p.filename().string();
					const std::string parentPath = p.parent_path().generic_string();

					if (!matchesFilter(filename, parentPath)) continue;

					const bool isSelected = (handle == value.GetRawHandle());
					const std::string uniqueId = "Mem_" + normStr;
					drawResourceItem(uniqueId, filename, parentPath, p, isSelected, [&]()
						{
							changed = TryAssignInspectorResource(value, handle);
							failed = !changed;
							s_SearchBuffer[0] = '\0';
							ImGui::CloseCurrentPopup();
						});
				}

				ImGui::PopID(); // LoadedInMemory_Section

				ImGui::Separator();

				// ── 6. 디스크 상의 프로젝트 파일 섹션 ──
				ImGui::PushID("DiskFiles_Section");

				size_t diskMatchCount = 0;
				for (const auto& file : s_CachedDiskFiles)
				{
					const std::filesystem::path p(file.FullPath);
					if (matchesFilter(file.Filename, p.parent_path().generic_string()))
						++diskMatchCount;
				}

				const std::string diskHeader = fmt::format("{} ({})", LocalizeInspectorText("ResourcePicker.ProjectFiles"), diskMatchCount);
				ImGui::TextDisabled("%s", diskHeader.c_str());

				for (const auto& file : s_CachedDiskFiles)
				{
					const std::filesystem::path p(file.FullPath);
					const std::string parentPath = p.parent_path().generic_string();
					if (!matchesFilter(file.Filename, parentPath)) continue;

					const bool isSelected = false;
					const std::string uniqueId = "Disk_" + file.FullPath;
					drawResourceItem(uniqueId, file.Filename, parentPath, p, isSelected, [&]()
						{
							const auto handle = manager.LoadFile(file.Key);
							changed = TryAssignInspectorResource(value, handle);
							failed = !changed;
							s_SearchBuffer[0] = '\0';
							ImGui::CloseCurrentPopup();
						});
				}

				ImGui::PopID(); // DiskFiles_Section
			}
			ImGui::EndChild();

			ImGui::EndPopup();
		}

		ImGui::EndDisabled();
		ImGui::GetStateStorage()->SetBool(errorId, failed);

		if (failed)
			ImGui::TextDisabled("%s", LocalizeInspectorText("UI_RESOURCE_REJECTED").c_str());

		return changed;
	}
}