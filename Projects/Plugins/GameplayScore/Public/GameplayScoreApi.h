#pragma once

#include <cstdint>

namespace GameplayScore
{
	inline constexpr wchar_t ModuleName[] = L"GameplayScore";
	inline constexpr wchar_t DllName[] = L"GameplayScore.dll";
	inline constexpr char EntryPoint[] = "BamGetGameplayScoreV1";

	using CalculateComboScoreFn = std::uint32_t (*)(std::uint32_t baseScore, std::uint32_t combo) noexcept;

	struct ApiV1
	{
		std::uint32_t Size;
		CalculateComboScoreFn CalculateComboScore;
	};

	using GetApiV1Fn = const ApiV1* (*)() noexcept;
}
