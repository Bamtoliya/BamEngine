#pragma once

#include "Engine_API.h"
#include <vector>
#include <string>
#include <cstdint>
#include <variant>

namespace Engine
{
	struct ENGINE_API RLObservation
	{
		std::vector<float> Data;

		std::size_t Size() const { return Data.size(); }
		const float* DataPtr() const { return Data.data(); }
	};

	enum class ERLActionType { Discrete, Continuous };

	struct ENGINE_API RLAction
	{
		ERLActionType Type = ERLActionType::Discrete;
		int64_t DiscreteAction = 0;
		std::vector<float> ContinuousActions;
	};

	struct ENGINE_API RLStepResult
	{
		RLObservation Observation;
		float Reward = 0.0f;
		bool Terminated = false;
		bool Truncated = false;
		std::string InfoJson = "{}";
	};

	struct ENGINE_API RLBoxSpace
	{
		std::vector<int64_t> Shape;
		std::vector<float> Low;
		std::vector<float> High;
	};

	struct ENGINE_API RLDiscreteSpace
	{
		int64_t N = 0;
	};

	struct ENGINE_API RLSpace
	{
		ERLActionType Type = ERLActionType::Discrete;
		RLDiscreteSpace Discrete;
		RLBoxSpace Box;
	};

	class ENGINE_API IRLEnvironment
	{
	public:
		virtual ~IRLEnvironment() = default;

		virtual void Initialize() = 0;
		virtual RLObservation Reset(uint64_t seed = 0) = 0;
		virtual RLStepResult Step(const RLAction& action) = 0;
		virtual RLSpace GetObservationSpace() const = 0;
		virtual RLSpace GetActionSpace() const = 0;
		virtual void Close() = 0;

		virtual const std::string& GetName() const = 0;
	};
}