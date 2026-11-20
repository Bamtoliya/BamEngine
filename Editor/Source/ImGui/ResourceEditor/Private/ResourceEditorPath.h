#pragma once

#include "PlatformPath.h"

namespace Editor
{
	inline bool SameResourceEditorPath(const std::filesystem::path& a, const std::filesystem::path& b)
	{
		if (a.empty() || b.empty()) return false;
		const auto first = Engine::PlatformPath::ResolveUri(a);
		const auto second = Engine::PlatformPath::ResolveUri(b);
		std::error_code error;
		return Engine::PlatformPath::IsSamePath(first, second) || std::filesystem::equivalent(first, second, error);
	}
}
