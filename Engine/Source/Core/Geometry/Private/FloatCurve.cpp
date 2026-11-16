#include "FloatCurve.h"
#include <algorithm>
#include <cmath>

namespace Engine
{
	float FloatCurve::Evaluate(float time, float defaultValue) const
	{
		if (keys.empty()) return defaultValue;
		if (keys.size() == 1) return keys[0].value;
		if (time <= keys.front().time) return keys.front().value;
		if (time >= keys.back().time) return keys.back().value;

		// 시간 기준으로 우측 키프레임 탐색
		auto it = std::upper_bound(keys.begin(), keys.end(), time,
			[](float t, const CurveKey& k) { return t < k.time; });

		const auto& k1 = *it;
		const auto& k0 = *(it - 1);

		const float dt = k1.time - k0.time;
		if (dt <= 1e-6f) return k0.value;

		const float u = (time - k0.time) / dt;

		switch (k0.interpMode)
		{
		case ECurveInterpMode::Constant:
			return k0.value;

		case ECurveInterpMode::Linear:
			return k0.value + u * (k1.value - k0.value);

		case ECurveInterpMode::CubicBezier:
		default:
		{
			const float u2 = u * u;
			const float u3 = u2 * u;

			// 3차 에르미트 기저 다항식
			const float h00 = 2.0f * u3 - 3.0f * u2 + 1.0f;
			const float h10 = u3 - 2.0f * u2 + u;
			const float h01 = -2.0f * u3 + 3.0f * u2;
			const float h11 = u3 - u2;

			const float m0 = k0.outTangent * dt;
			const float m1 = k1.inTangent * dt;

			return h00 * k0.value + h10 * m0 + h01 * k1.value + h11 * m1;
		}
		}
	}

	int FloatCurve::AddKey(float time, float value, float inTangent, float outTangent, ECurveInterpMode interp)
	{
		// 동일 시간대(1e-4초 이내)의 키가 이미 존재하면 갱신
		for (std::size_t i = 0; i < keys.size(); ++i)
		{
			if (std::abs(keys[i].time - time) <= 1e-4f)
			{
				keys[i].value = value;
				keys[i].inTangent = inTangent;
				keys[i].outTangent = outTangent;
				keys[i].interpMode = interp;
				return static_cast<int>(i);
			}
		}

		keys.emplace_back(time, value, inTangent, outTangent, interp);
		SortKeys();

		for (std::size_t i = 0; i < keys.size(); ++i)
		{
			if (std::abs(keys[i].time - time) <= 1e-4f)
				return static_cast<int>(i);
		}
		return static_cast<int>(keys.size() - 1);
	}

	bool FloatCurve::RemoveKey(std::size_t index)
	{
		if (index >= keys.size()) return false;
		keys.erase(keys.begin() + index);
		return true;
	}

	void FloatCurve::SortKeys()
	{
		std::stable_sort(keys.begin(), keys.end(),
			[](const CurveKey& a, const CurveKey& b) { return a.time < b.time; });
	}

	void FloatCurve::Clear()
	{
		keys.clear();
	}

	FloatCurve FloatCurve::CreateConstant(float value)
	{
		FloatCurve curve;
		curve.keys.emplace_back(0.0f, value, 0.0f, 0.0f, ECurveInterpMode::Constant);
		curve.keys.emplace_back(1.0f, value, 0.0f, 0.0f, ECurveInterpMode::Constant);
		return curve;
	}

	FloatCurve FloatCurve::CreateLinear(float startValue, float endValue)
	{
		FloatCurve curve;
		const float slope = endValue - startValue;
		curve.keys.emplace_back(0.0f, startValue, slope, slope, ECurveInterpMode::Linear);
		curve.keys.emplace_back(1.0f, endValue, slope, slope, ECurveInterpMode::Linear);
		return curve;
	}

	FloatCurve FloatCurve::CreateEaseIn(float startValue, float endValue)
	{
		FloatCurve curve;
		const float delta = endValue - startValue;
		curve.keys.emplace_back(0.0f, startValue, 0.0f, 0.0f, ECurveInterpMode::CubicBezier);
		curve.keys.emplace_back(1.0f, endValue, 2.0f * delta, 0.0f, ECurveInterpMode::CubicBezier);
		return curve;
	}

	FloatCurve FloatCurve::CreateEaseOut(float startValue, float endValue)
	{
		FloatCurve curve;
		const float delta = endValue - startValue;
		// 초반에 급격히 팽창(기울기 2.5배)하고 끝에서 완만하게 수렴 (폭발 표준)
		curve.keys.emplace_back(0.0f, startValue, 0.0f, 2.5f * delta, ECurveInterpMode::CubicBezier);
		curve.keys.emplace_back(1.0f, endValue, 0.0f, 0.0f, ECurveInterpMode::CubicBezier);
		return curve;
	}

	FloatCurve FloatCurve::CreateEaseInOut(float startValue, float endValue)
	{
		FloatCurve curve;
		curve.keys.emplace_back(0.0f, startValue, 0.0f, 0.0f, ECurveInterpMode::CubicBezier);
		curve.keys.emplace_back(1.0f, endValue, 0.0f, 0.0f, ECurveInterpMode::CubicBezier);
		return curve;
	}

	FloatCurve FloatCurve::CreateSpike(float peakValue, float peakTime)
	{
		FloatCurve curve;
		const float clampedPeak = std::clamp(peakTime, 0.01f, 0.99f);
		curve.keys.emplace_back(0.0f, 0.0f, 0.0f, (peakValue / clampedPeak) * 1.5f, ECurveInterpMode::CubicBezier);
		curve.keys.emplace_back(clampedPeak, peakValue, 0.0f, 0.0f, ECurveInterpMode::CubicBezier);
		curve.keys.emplace_back(1.0f, 0.0f, 0.0f, 0.0f, ECurveInterpMode::CubicBezier);
		return curve;
	}
}