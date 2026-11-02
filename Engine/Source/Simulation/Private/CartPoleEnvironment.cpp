#include "CartPoleEnvironment.h"
#include <cmath>
#include <algorithm>

namespace Engine
{
	CartPoleEnvironment::CartPoleEnvironment()
	{
		Initialize();
	}

	void CartPoleEnvironment::Initialize()
	{
		m_Rng.seed(std::random_device{}());
		m_X = 0.0f;
		m_XDot = 0.0f;
		m_Theta = 0.0f;
		m_ThetaDot = 0.0f;
		m_StepCount = 0;
	}

	RLObservation CartPoleEnvironment::Reset(std::optional<uint64_t> seed)
	{
		if (seed.has_value())
		{
			m_Rng.seed(seed.value());
		}

		// 초기 상태: [-0.05, 0.05] 균등 분포 난수
		std::uniform_real_distribution<float> dist(-0.05f, 0.05f);
		m_X = dist(m_Rng);
		m_XDot = dist(m_Rng);
		m_Theta = dist(m_Rng);
		m_ThetaDot = dist(m_Rng);
		m_StepCount = 0;

		return { { m_X, m_XDot, m_Theta, m_ThetaDot } };
	}

	RLStepResult CartPoleEnvironment::Step(const RLAction& action)
	{
		// 행동 해석: 0 = 좌측 힘(-10N), 1 = 우측 힘(+10N)
		const float force = (action.DiscreteAction == 1) ? ForceMag : -ForceMag;

		const float costheta = std::cos(m_Theta);
		const float sintheta = std::sin(m_Theta);

		// 카트폴 운동 방정식 (Gymnasium 호환 세미 임시 적분)
		const float temp = (force + PoleMassLength * m_ThetaDot * m_ThetaDot * sintheta) / TotalMass;
		const float thetaAcc = (Gravity * sintheta - costheta * temp) /
			(Length * (4.0f / 3.0f - MassPole * costheta * costheta / TotalMass));
		const float xAcc = temp - PoleMassLength * thetaAcc * costheta / TotalMass;

		// 오일러(Euler) 적분
		m_X += Tau * m_XDot;
		m_XDot += Tau * xAcc;
		m_Theta += Tau * m_ThetaDot;
		m_ThetaDot += Tau * thetaAcc;
		++m_StepCount;

		// 종료(Terminated) 조건 검사
		const bool terminated = (m_X < -XThreshold || m_X > XThreshold ||
			m_Theta < -ThetaThresholdRadians || m_Theta > ThetaThresholdRadians);

		// 시간 초과 중단(Truncated) 조건 검사
		const bool truncated = (m_StepCount >= MaxSteps);

		// 카트폴 표준 보상: 쓰러지지 않고 살아남은 매 스텝마다 +1.0
		const float reward = terminated ? 0.0f : 1.0f;

		RLStepResult result;
		result.Observation = { { m_X, m_XDot, m_Theta, m_ThetaDot } };
		result.Reward = reward;
		result.Terminated = terminated;
		result.Truncated = truncated;
		result.InfoJson = "{\"steps\":" + std::to_string(m_StepCount) + "}";
		return result;
	}

	RLSpace CartPoleEnvironment::GetObservationSpace() const
	{
		RLSpace space;
		space.Type = ERLActionType::Continuous;
		space.Box.Shape = { 4 };
		// x: [-4.8, 4.8], x_dot: [-inf, inf], theta: [-0.418, 0.418], theta_dot: [-inf, inf]
		space.Box.Low = { -XThreshold * 2.0f, -std::numeric_limits<float>::infinity(),
			-ThetaThresholdRadians * 2.0f, -std::numeric_limits<float>::infinity() };
		space.Box.High = { XThreshold * 2.0f, std::numeric_limits<float>::infinity(),
			ThetaThresholdRadians * 2.0f, std::numeric_limits<float>::infinity() };
		return space;
	}

	RLSpace CartPoleEnvironment::GetActionSpace() const
	{
		RLSpace space;
		space.Type = ERLActionType::Discrete;
		space.Discrete.N = 2; // 0 = Push left, 1 = Push right
		return space;
	}

	void CartPoleEnvironment::Close()
	{
		m_StepCount = 0;
	}
}