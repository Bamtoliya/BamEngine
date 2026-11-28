#include "AssetPathPicker.h"
#include "NotificationManager.h"
#include "PlatformPath.h"

namespace Editor
{
	namespace
	{
		struct PathEntry
		{
			std::string Filename;
			std::string SubPath;
			std::string LowerReference;
			std::string Reference;
			std::filesystem::path FullPath;
		};

		// 마운트된 루트(Project·Engine·Editor) 밖의 파일이면 빈 문자열을 돌려준다.
		std::string ToAssetReference(const std::filesystem::path& path)
		{
			const std::wstring uri = Engine::PlatformPath::ToVirtualUri(path);
			return Engine::PlatformPath::IsVirtualUri(uri) ? Engine::Unicode::WideToUtf8(uri) : std::string{};
		}

		void CollectEntries(const AssetPathPickerDesc& desc, const Detail::PickerAssetCatalog& catalog,
			std::vector<PathEntry>& entries)
		{
			entries.clear();
			std::unordered_set<std::string> seen;
			for (const auto& file : catalog.Files)
			{
				if (desc.Accepts && !desc.Accepts(file.Path))
					continue;

				std::string reference = ToAssetReference(file.Path);
				if (reference.empty() || !seen.insert(reference).second)
					continue;

				const auto separator = reference.find_last_of('/');
				auto subPath = separator == std::string::npos ? std::string{} : reference.substr(0, separator);
				auto lowerReference = Engine::ToLowerStr(reference);
				entries.push_back({ file.Filename, std::move(subPath), std::move(lowerReference), std::move(reference), file.Path });
			}
			std::sort(entries.begin(), entries.end(), [](const PathEntry& a, const PathEntry& b) { return a.Reference < b.Reference; });
		}

		void DrawFieldContents(const std::string& current, const AssetPathPickerDesc& desc, float thumbnailSize, float fieldHeight)
		{
			if (!ImGui::IsItemVisible()) return;

			const auto& style = ImGui::GetStyle();
			const auto min = ImGui::GetItemRectMin();
			const auto max = ImGui::GetItemRectMax();
			auto* drawList = ImGui::GetWindowDrawList();

			const float availableWidth = std::max(0.0f, max.x - min.x - style.FramePadding.x * 2.0f);
			const float side = std::min(thumbnailSize, availableWidth * 0.3f);
			const ImVec2 imageMin(min.x + style.FramePadding.x, min.y + (fieldHeight - side) * 0.5f);
			const ImVec2 imageMax(imageMin.x + side, imageMin.y + side);

			drawList->PushClipRect(min, max, true);
			drawList->AddRectFilled(imageMin, imageMax, ImGui::GetColorU32(ImGuiCol_FrameBg), style.FrameRounding);
			const auto iconSize = ImGui::CalcTextSize(desc.Visual.Icon);
			drawList->AddText(ImVec2(imageMin.x + (side - iconSize.x) * 0.5f, imageMin.y + (side - iconSize.y) * 0.5f), ImGui::GetColorU32(desc.Visual.Color), desc.Visual.Icon);

			const auto separator = current.find_last_of("/\\");
			const std::string name = current.empty() ? LocalizeInspectorText("ResourcePicker.None") : (separator == std::string::npos ? current : current.substr(separator + 1));

			const float textX = imageMax.x + style.ItemInnerSpacing.x;
			const float textY = min.y + style.FramePadding.y;
			drawList->PushClipRect(ImVec2(textX, min.y), ImVec2(std::max(textX, max.x - style.FramePadding.x), max.y), true);
			drawList->AddText(ImVec2(textX, textY), ImGui::GetColorU32(ImGuiCol_Text), name.c_str());
			if (!current.empty())
				drawList->AddText(ImVec2(textX, textY + ImGui::GetTextLineHeight() + style.ItemInnerSpacing.y), ImGui::GetColorU32(ImGuiCol_TextDisabled), current.c_str());
			drawList->PopClipRect();
			drawList->PopClipRect();
		}
	}

	bool DrawInspectorAssetPathPicker(const std::string& current, std::string& selected, const AssetPathPickerDesc& desc, bool canEdit)
	{
		ImGui::PushID(&current);

		bool picked = false;
		const auto errorId = ImGui::GetID("DropFailed");
		bool failed = ImGui::GetStateStorage()->GetBool(errorId, false);
		const std::filesystem::path resolved = current.empty() ? std::filesystem::path{} : Engine::PlatformPath::ResolveUri(Engine::Utf8ToPath(current));

		ImGui::BeginDisabled(!canEdit);

		const auto& style = ImGui::GetStyle();
		const float thumbnailSize = ImGui::GetTextLineHeight() * 2.0f + style.ItemInnerSpacing.y;
		const float fieldHeight = thumbnailSize + style.FramePadding.y * 2.0f;

		if (ImGui::Button("###PathPickerButton", ImVec2(-1.0f, fieldHeight)))
			ImGui::OpenPopup("PathChoices");

		const bool fieldHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled);
		if (!current.empty() && fieldHovered && InputManager::Get().IsMouseButtonDoubleClicked(EMouseButton::Left))
			Detail::RequestOpenResourceEditor(resolved);

		DrawFieldContents(current, desc, thumbnailSize, fieldHeight);

		if (canEdit && ImGui::BeginDragDropTarget())
		{
			if (const auto* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
			{
				const auto dropped = DecodeInspectorResourcePath(payload->Data, payload->DataSize);
				failed = !dropped || (desc.Accepts && !desc.Accepts(*dropped));
				if (!failed)
				{
					selected = ToAssetReference(*dropped);
					if (selected.empty())
					{
						NotificationManager::Get().AddNotification(Engine::Unicode::WideToUtf8(std::filesystem::path(*dropped).filename().wstring()), LocalizeInspectorText("ResourcePicker.OutsideRoots"), ELogLevel::Warn);
						failed = true;
					}
					else
					{
						picked = true;
					}
				}
			}
			ImGui::EndDragDropTarget();
		}

		if (!current.empty())
			Detail::DrawResourceContextMenu(resolved);

		if (fieldHovered)
			ImGui::SetTooltip("%s", current.empty() ? LocalizeInspectorText("ResourcePicker.None").c_str() : current.c_str());

		ImGui::SetNextWindowSize(ImVec2(560.0f, 440.0f), ImGuiCond_Appearing);
		ImGui::SetNextWindowSizeConstraints(ImVec2(400.0f, 280.0f), ImVec2(1600.0f, 1200.0f));
		if (ImGui::BeginPopup("PathChoices"))
		{
			static std::vector<PathEntry> s_Entries;
			static std::vector<size_t> s_Filtered;
			static char s_SearchBuffer[128] = "";
			static std::string s_LastQuery;
			static std::uint64_t s_CatalogRevision = 0;
			static decltype(desc.Accepts) s_Accepts = nullptr;

			const bool appearing = ImGui::IsWindowAppearing();
			const auto& catalog = Detail::GetPickerAssetCatalog();
			const bool entriesChanged = catalog.Revision != s_CatalogRevision || desc.Accepts != s_Accepts;
			if (entriesChanged)
			{
				CollectEntries(desc, catalog, s_Entries);
				s_CatalogRevision = catalog.Revision;
				s_Accepts = desc.Accepts;
			}
			if (appearing)
				s_SearchBuffer[0] = '\0';

			EditorUI::SearchBarOptions searchOptions;
			searchOptions.FocusOnDraw = appearing;
			EditorUI::DrawSearchBar("PathPickerSearch", s_SearchBuffer, sizeof(s_SearchBuffer), "ResourcePicker.SearchHint", searchOptions);

			if (entriesChanged || s_LastQuery != s_SearchBuffer)
			{
				s_LastQuery = s_SearchBuffer;
				const std::string query = Engine::ToLowerStr(s_LastQuery);
				s_Filtered.clear();
				for (size_t i = 0; i < s_Entries.size(); ++i)
				{
					if (query.empty() || s_Entries[i].LowerReference.find(query) != std::string::npos)
						s_Filtered.push_back(i);
				}
			}

			ImGui::Separator();

			const float thumbSize = 64.0f;
			const float itemHeight = std::max(thumbSize + 8.0f, ImGui::GetTextLineHeight() * 2.0f + 10.0f);
			const float itemStride = itemHeight + style.ItemSpacing.y;
			bool openEditorRequested = false;

			if (ImGui::BeginChild("##PathListRegion", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar))
			{
				if (Detail::DrawPickerClearItem(current.empty(), thumbSize, itemHeight))
				{
					selected.clear();
					picked = true;
					failed = false;
					ImGui::CloseCurrentPopup();
				}

				ImGui::Spacing();
				ImGui::Separator();
				ImGui::Spacing();

				const std::string header = fmt::format("{} ({})", LocalizeInspectorText("ResourcePicker.ProjectFiles"), s_Filtered.size());
				ImGui::TextDisabled("%s", header.c_str());

				if (!catalog.IsReady)
					EditorUI::EmptyState("ResourcePicker.IndexingFiles");
				else if (s_Filtered.empty())
				{
					EditorUI::EmptyState("ResourcePicker.NoMatchingFiles");
				}
				else
				{
					ImGuiListClipper clipper;
					clipper.Begin(static_cast<int>(s_Filtered.size()), itemStride);
					while (clipper.Step())
					{
						for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
						{
							const auto& entry = s_Entries[s_Filtered[i]];
							Detail::PickerItemView view{ entry.Reference, entry.Filename, entry.SubPath, desc.Visual, entry.FullPath };
							Detail::ApplyPickerThumbnail(view);

							if (Detail::DrawPickerItem(view, entry.Reference == current, thumbSize, itemHeight, openEditorRequested))
							{
								std::error_code error;
								if (!std::filesystem::is_regular_file(entry.FullPath, error))
								{
									Detail::InvalidatePickerAssetCatalog();
									failed = true;
									continue;
								}
								selected = entry.Reference;
								picked = true;
								failed = false;
								ImGui::CloseCurrentPopup();
							}
						}
					}
				}
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
		return picked;
	}
}
