#pragma once

#include "RLEnvironment.h"
#include <random>

namespace Engine
{
	class ENGINE_API CartPoleEnvironment final : public IRLEnvironment
	{
	public:
		CartPoleEnvironment();
		virtual ~CartPoleEnvironment() override = default;

		virtual void Initialize() override;
		virtual RLObservation Reset(std::optional<uint64_t> seed = std::nullopt) override;
		virtual RLStepResult Step(const RLAction& action) override;
		virtual RLSpace GetObservationSpace() const override;
		virtual RLSpace GetActionSpace() const override;
		virtual void Close() override;

		virtual const std::string& GetName() const override { return m_Name; }

	private:
		// ── 물리 파라미터 (Gymnasium CartPole-v1 표준) ──
		static constexpr float Gravity = 9.8f;
		static constexpr float MassCart = 1.0f;
		static constexpr float MassPole = 0.1f;
		static constexpr float TotalMass = MassCart + MassPole;
		static constexpr float Length = 0.5f; // 반(half) 길이 (질량 중심까지)
		static constexpr float PoleMassLength = MassPole * Length;
		static constexpr float ForceMag = 10.0f;
		static constexpr float Tau = 0.02f; // 고정 물리 dt (50Hz)

		// ── 종료 한계값 ──
		static constexpr float ThetaThresholdRadians = 12.0f * 3.14159265358979323846f / 180.0f; // ~0.2095 rad
		static constexpr float XThreshold = 2.4f;
		static constexpr int32_t MaxSteps = 500; // CartPole-v1 기준 500스텝

		std::string m_Name = "CartPole-v1";
		std::mt19937_64 m_Rng;

		// ── 상태 변수: [x, x_dot, theta, theta_dot] ──
		float m_X = 0.0f;
		float m_XDot = 0.0f;
		float m_Theta = 0.0f;
		float m_ThetaDot = 0.0f;
		int32_t m_StepCount = 0;
	};
}