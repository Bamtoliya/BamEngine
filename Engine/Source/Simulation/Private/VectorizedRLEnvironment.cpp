#include "VectorizedRLEnvironment.h"

#include "CartPoleEnvironment.h"
#include "SceneRLEnvironment.h"

namespace Engine
{
	VectorizedRLEnvironment::VectorizedRLEnvironment(std::size_t numEnvs, EnvFactory factory, bool autoReset)
		: m_AutoReset(autoReset)
	{
		m_Envs.reserve(numEnvs);
		m_EpisodeCounts.assign(numEnvs, 0ull);
		if (factory)
		{
			for (std::size_t i = 0; i < numEnvs; ++i)
			{
				auto env = factory(i);
				if (env)
				{
					m_Envs.push_back(std::move(env));
				}
			}
		}
		m_EpisodeCounts.resize(m_Envs.size(), 0ull);
	}

	VectorizedRLEnvironment::VectorizedRLEnvironment(std::vector<std::unique_ptr<IRLEnvironment>> envs, bool autoReset)
		: m_AutoReset(autoReset)
	{
		m_Envs.reserve(envs.size());
		for (auto& env : envs)
		{
			if (env)
			{
				m_Envs.push_back(std::move(env));
			}
		}
		m_EpisodeCounts.assign(m_Envs.size(), 0ull);
	}

	VectorizedRLEnvironment::~VectorizedRLEnvironment()
	{
		CloseAll();
	}

	std::shared_ptr<VectorizedRLEnvironment> VectorizedRLEnvironment::CreateCartPole(std::size_t numEnvs, bool autoReset)
	{
		return std::make_shared<VectorizedRLEnvironment>(
			numEnvs,
			[](std::size_t) -> std::unique_ptr<IRLEnvironment>
			{
				return std::make_unique<CartPoleEnvironment>();
			},
			autoReset);
	}

	std::shared_ptr<VectorizedRLEnvironment> VectorizedRLEnvironment::CreateSceneNavigation(std::size_t numEnvs, bool autoReset)
	{
		return CreateSceneNavigation(numEnvs, SceneRLEnvConfig{}, autoReset);
	}

	std::shared_ptr<VectorizedRLEnvironment> VectorizedRLEnvironment::CreateSceneNavigation(std::size_t numEnvs, const SceneRLEnvConfig& config, bool autoReset)
	{
		return std::make_shared<VectorizedRLEnvironment>(
			numEnvs,
			[config](std::size_t) -> std::unique_ptr<IRLEnvironment>
			{
				return std::make_unique<SceneRLEnvironment>(config);
			},
			autoReset);
	}

	void VectorizedRLEnvironment::Initialize()
	{
		for (auto& env : m_Envs)
		{
			if (env) env->Initialize();
		}
		m_EpisodeCounts.assign(m_Envs.size(), 0ull);
	}

	std::vector<RLObservation> VectorizedRLEnvironment::ResetAll(std::optional<uint64_t> baseSeed)
	{
		m_BaseSeed = baseSeed;
		m_EpisodeCounts.assign(m_Envs.size(), 0ull);

		std::vector<RLObservation> observations;
		observations.reserve(m_Envs.size());

		for (std::size_t i = 0; i < m_Envs.size(); ++i)
		{
			std::optional<uint64_t> envSeed = std::nullopt;
			if (baseSeed.has_value())
			{
				envSeed = baseSeed.value() + static_cast<uint64_t>(i);
			}
			observations.push_back(m_Envs[i]->Reset(envSeed));
		}

		return observations;
	}

	RLVectorStepResult VectorizedRLEnvironment::StepAll(const std::vector<RLAction>& actions)
	{
		const std::size_t count = m_Envs.size();
		RLVectorStepResult batch;
		batch.Observations.resize(count);
		batch.Rewards.resize(count, 0.0f);
		batch.Terminated.resize(count, false);
		batch.Truncated.resize(count, false);
		batch.InfoJsons.resize(count, "{}");
		batch.FinalObservations.resize(count);
		batch.FinalInfoJsons.resize(count);

		for (std::size_t i = 0; i < count; ++i)
		{
			const RLAction action = (i < actions.size()) ? actions[i] : RLAction{};
			RLStepResult stepRes = m_Envs[i]->Step(action);

			batch.Rewards[i] = stepRes.Reward;
			batch.Terminated[i] = stepRes.Terminated;
			batch.Truncated[i] = stepRes.Truncated;
			batch.InfoJsons[i] = stepRes.InfoJson;

			if (stepRes.Terminated || stepRes.Truncated)
			{
				batch.FinalObservations[i] = stepRes.Observation;
				batch.FinalInfoJsons[i] = stepRes.InfoJson;
				++m_EpisodeCounts[i];

				if (m_AutoReset)
				{
					std::optional<uint64_t> nextSeed = std::nullopt;
					if (m_BaseSeed.has_value())
					{
						nextSeed = m_BaseSeed.value() + static_cast<uint64_t>(i) + m_EpisodeCounts[i] * 1000003ull;
					}
					batch.Observations[i] = m_Envs[i]->Reset(nextSeed);
				}
				else
				{
					batch.Observations[i] = std::move(stepRes.Observation);
				}
			}
			else
			{
				batch.Observations[i] = std::move(stepRes.Observation);
			}
		}

		return batch;
	}

	RLVectorStepResult VectorizedRLEnvironment::StepAll(const std::vector<std::vector<f32>>& continuousActions)
	{
		std::vector<RLAction> actions;
		actions.reserve(continuousActions.size());
		for (const auto& vecAct : continuousActions)
		{
			RLAction act;
			act.Type = ERLActionType::Continuous;
			act.ContinuousActions = vecAct;
			actions.push_back(std::move(act));
		}
		return StepAll(actions);
	}

	RLVectorStepResult VectorizedRLEnvironment::StepAllDiscrete(const std::vector<int64_t>& discreteActions)
	{
		std::vector<RLAction> actions;
		actions.reserve(discreteActions.size());
		for (int64_t discreteAct : discreteActions)
		{
			RLAction act;
			act.Type = ERLActionType::Discrete;
			act.DiscreteAction = discreteAct;
			actions.push_back(std::move(act));
		}
		return StepAll(actions);
	}

	void VectorizedRLEnvironment::CloseAll()
	{
		for (auto& env : m_Envs)
		{
			if (env) env->Close();
		}
	}

	RLSpace VectorizedRLEnvironment::GetSingleObservationSpace() const
	{
		if (m_Envs.empty()) return {};
		return m_Envs[0]->GetObservationSpace();
	}

	RLSpace VectorizedRLEnvironment::GetSingleActionSpace() const
	{
		if (m_Envs.empty()) return {};
		return m_Envs[0]->GetActionSpace();
	}

	IRLEnvironment* VectorizedRLEnvironment::GetEnvironment(std::size_t index) const
	{
		if (index >= m_Envs.size()) return nullptr;
		return m_Envs[index].get();
	}

	uint64_t VectorizedRLEnvironment::GetEpisodeCount(std::size_t index) const
	{
		if (index >= m_EpisodeCounts.size()) return 0ull;
		return m_EpisodeCounts[index];
	}
}
