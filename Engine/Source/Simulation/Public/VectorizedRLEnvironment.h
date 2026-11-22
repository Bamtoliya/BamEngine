#pragma once

#include "Engine_API.h"
#include "RLEnvironment.h"
#include "Types.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Engine
{
	struct SceneRLEnvConfig;

	struct ENGINE_API RLVectorStepResult
	{
		std::vector<RLObservation> Observations;
		std::vector<f32> Rewards;
		std::vector<bool> Terminated;
		std::vector<bool> Truncated;
		std::vector<std::string> InfoJsons;
		std::vector<std::optional<RLObservation>> FinalObservations;
		std::vector<std::optional<std::string>> FinalInfoJsons;
	};

	class ENGINE_API VectorizedRLEnvironment
	{
	public:
		using EnvFactory = std::function<std::unique_ptr<IRLEnvironment>(std::size_t index)>;

		VectorizedRLEnvironment(std::size_t numEnvs, EnvFactory factory, bool autoReset = true);
		explicit VectorizedRLEnvironment(std::vector<std::unique_ptr<IRLEnvironment>> envs, bool autoReset = true);
		~VectorizedRLEnvironment();

		VectorizedRLEnvironment(const VectorizedRLEnvironment&) = delete;
		VectorizedRLEnvironment& operator=(const VectorizedRLEnvironment&) = delete;

		static std::shared_ptr<VectorizedRLEnvironment> CreateCartPole(std::size_t numEnvs, bool autoReset = true);
		static std::shared_ptr<VectorizedRLEnvironment> CreateSceneNavigation(std::size_t numEnvs, bool autoReset = true);
		static std::shared_ptr<VectorizedRLEnvironment> CreateSceneNavigation(std::size_t numEnvs, const SceneRLEnvConfig& config, bool autoReset = true);

		void Initialize();
		std::vector<RLObservation> ResetAll(std::optional<uint64_t> baseSeed = std::nullopt);
		RLVectorStepResult StepAll(const std::vector<RLAction>& actions);
		RLVectorStepResult StepAll(const std::vector<std::vector<f32>>& continuousActions);
		RLVectorStepResult StepAllDiscrete(const std::vector<int64_t>& discreteActions);
		void CloseAll();

		std::size_t GetNumEnvs() const { return m_Envs.size(); }
		bool IsAutoResetEnabled() const { return m_AutoReset; }
		void SetAutoReset(bool enabled) { m_AutoReset = enabled; }

		RLSpace GetSingleObservationSpace() const;
		RLSpace GetSingleActionSpace() const;
		IRLEnvironment* GetEnvironment(std::size_t index) const;
		uint64_t GetEpisodeCount(std::size_t index) const;

	private:
		std::vector<std::unique_ptr<IRLEnvironment>> m_Envs;
		std::vector<uint64_t> m_EpisodeCounts;
		std::optional<uint64_t> m_BaseSeed;
		bool m_AutoReset = true;
	};
}
