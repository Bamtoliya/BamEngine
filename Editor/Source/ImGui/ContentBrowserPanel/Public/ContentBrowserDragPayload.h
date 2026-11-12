#pragma once

#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

namespace Editor::ContentBrowser
{
	inline constexpr char SingleItemPayload[] = "CONTENT_BROWSER_ITEM";
	inline constexpr char MultipleItemsPayload[] = "CONTENT_BROWSER_ITEMS";

	// The payload contains a NUL-terminated wide string for each path.
	inline std::vector<std::wstring> DecodeDragPaths(const void* data, int byteCount)
	{
		if (!data || byteCount <= 0 || byteCount % sizeof(wchar_t) != 0)
			return {};

		std::wstring buffer(static_cast<std::size_t>(byteCount) / sizeof(wchar_t), L'\0');
		std::memcpy(buffer.data(), data, static_cast<std::size_t>(byteCount));
		std::vector<std::wstring> paths;
		for (std::size_t start = 0; start < buffer.size();)
		{
			const std::size_t end = buffer.find(L'\0', start);
			if (end == start || end == std::wstring::npos)
				return {};
			paths.emplace_back(buffer, start, end - start);
			start = end + 1;
		}
		return paths;
	}
}
