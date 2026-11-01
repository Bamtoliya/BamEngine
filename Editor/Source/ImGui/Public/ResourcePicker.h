#pragma once

#include "ReflectionInspectorSupport.h"
#include "Functions.h"
#include <imgui.h>
#include <fmt/format.h>
#include <string_view>
#include <cctype>
#include <algorithm>
#include <filesystem>

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
				return endsWith(".hlsl") || endsWith(".cso") || endsWith(".bamshader");
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

		// 팝업 기본 크기를 500x460으로 확대하고, 최대 1000x800까지 리사이즈 가능하도록 제약 완화
		ImGui::SetNextWindowSize(ImVec2(500.0f, 460.0f), ImGuiCond_Appearing);
		ImGui::SetNextWindowSizeConstraints(ImVec2(360.0f, 260.0f), ImVec2(1000.0f, 800.0f));

		if (ImGui::BeginPopup("ResourceChoices"))
		{
			struct DiskAssetEntry
			{
				std::string Filename;
				std::string FullPath;
				std::wstring Key;
			};
			static std::vector<DiskAssetEntry> s_CachedDiskFiles;
			static ImGuiTextFilter s_Filter;

			const bool isAppearing = ImGui::IsWindowAppearing();

			// ── 1. 팝업 진입 시 1회만 디스크 스캔 및 캐싱 (매 프레임 I/O 병목 완전 차단) ──
			if (isAppearing)
			{
				s_Filter.Clear();
				s_CachedDiskFiles.clear();

				std::unordered_set<std::wstring> loadedNormalizedKeys;
				for (const auto& entry : manager.GetDebugSnapshot())
				{
					const auto handle = manager.FindHandleByKey(entry.Key);
					if (dynamic_cast<T*>(manager.GetResource(handle)) != nullptr)
						loadedNormalizedKeys.insert(Detail::NormalizeResourceKey(entry.Key));
				}

				std::error_code ec;
				const std::filesystem::path resDir = "Resources";
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

			// ── 2. 실시간 검색 필터 (팝업 진입 시 자동 키보드 포커스) ──
			if (isAppearing) ImGui::SetKeyboardFocusHere();
			s_Filter.Draw("##ResourceSearchFilter", -1.0f);
			ImGui::Separator();

			// ── 3. 수평 스크롤바(Horizontal Scrollbar)를 활성화한 스크롤 영역 ──
			if (ImGui::BeginChild("##ResourceListRegion", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar))
			{
				// 할당 해제 (Clear) 옵션
				if (ImGui::Selectable(LocalizeInspectorText("UI_RESOURCE_CLEAR").c_str()))
				{
					value.Reset();
					changed = true;
					failed = false;
					s_Filter.Clear();
					ImGui::CloseCurrentPopup();
				}
				ImGui::Separator();

				// ── 4. 메모리에 이미 로드된 호환 리소스 목록 ──
				ImGui::TextDisabled("%s", LocalizeInspectorText("ResourcePicker.LoadedInMemory").c_str());
				for (const auto& entry : manager.GetDebugSnapshot())
				{
					const auto handle = manager.FindHandleByKey(entry.Key);
					if (dynamic_cast<T*>(manager.GetResource(handle)) == nullptr)
						continue;

					const auto text = Engine::WStrToStr(entry.Key);
					if (!s_Filter.PassFilter(text.c_str()))
						continue;

					ImGui::PushID(text.c_str());
					if (ImGui::Selectable(text.c_str(), handle == value.GetRawHandle()))
					{
						changed = TryAssignInspectorResource(value, handle);
						failed = !changed;
						s_Filter.Clear();
						ImGui::CloseCurrentPopup();
					}
					if (ImGui::IsItemHovered())
						ImGui::SetTooltip("%s", text.c_str());
					ImGui::PopID();
				}

				ImGui::Separator();

				// ── 5. 디스크 상의 미로드 프로젝트 에셋 목록 (1회 캐시에서 즉각 렌더링) ──
				ImGui::TextDisabled("%s", LocalizeInspectorText("ResourcePicker.ProjectFiles").c_str());
				for (const auto& file : s_CachedDiskFiles)
				{
					if (!s_Filter.PassFilter(file.Filename.c_str()) && !s_Filter.PassFilter(file.FullPath.c_str()))
						continue;

					ImGui::PushID(file.FullPath.c_str());
					const auto label = fmt::format(fmt::runtime(LocalizeInspectorText("ResourcePicker.DiskFile")), file.Filename) +
						"###DiskResource";
					if (ImGui::Selectable(label.c_str(), false))
					{
						const auto handle = manager.LoadFile(file.Key);
						changed = TryAssignInspectorResource(value, handle);
						failed = !changed;
						s_Filter.Clear();
						ImGui::CloseCurrentPopup();
					}
					if (ImGui::IsItemHovered())
						ImGui::SetTooltip("%s", file.FullPath.c_str());
					ImGui::PopID();
				}
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
