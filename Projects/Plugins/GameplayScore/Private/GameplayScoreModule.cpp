#include "GameplayScoreApi.h"
#include <algorithm>
#include <limits>
#include "ModuleInterface.h"

namespace
{
	class GameplayScoreModule final : public Engine::IModule
	{
	public:
		void OnLoad() override {}
		void OnUnload() override {}
	};

	std::uint32_t CalculateComboScore(std::uint32_t baseScore, std::uint32_t combo) noexcept
	{
		const auto multiplier = std::clamp(combo, std::uint32_t{ 1 }, std::uint32_t{ 5 });
		const auto score = static_cast<std::uint64_t>(baseScore) * multiplier;
		const auto maxScore = std::numeric_limits<std::uint32_t>::max();
		return static_cast<std::uint32_t>(std::min(score, static_cast<std::uint64_t>(maxScore)));
	}

	const GameplayScore::ApiV1 ScoreApi{ sizeof(GameplayScore::ApiV1), &CalculateComboScore };
}

IMPLEMENT_MODULE(GameplayScoreModule)
EXPORT_PLUGIN_API_ENTRY(BamGetGameplayScoreV1, GameplayScore::ApiV1, ScoreApi)
