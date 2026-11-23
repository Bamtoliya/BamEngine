#pragma once

#include "Engine_Includes.h"
#include <algorithm>
#include <cmath>
#include <span>

BEGIN(Engine)

// 자동 노출 계산식. autoExposureHistogramCS·autoExposureAdaptCS가 같은 식을 쓰므로 값을 바꾸면 셰이더도 함께 고친다.
// 노출은 EV(2의 거듭제곱) 단위로 다루고 색에는 exp2(EV)를 곱한다.
namespace AutoExposure
{
	inline constexpr uint32 HistogramBinCount = 64;
	// 측정하는 밝기 범위(log2). 범위 밖은 양 끝 구간에 넣는다.
	inline constexpr f32 MinLogLuminance = -10.0f;
	inline constexpr f32 MaxLogLuminance = 10.0f;
	// 가장 어두운 비율과 가장 밝은 비율의 화소는 평균에서 뺀다. 태양·발광체 같은 작은 극단값이 노출을 끌지 않게 한다.
	inline constexpr f32 LowPercent = 0.1f;
	inline constexpr f32 HighPercent = 0.9f;
	// 평균 밝기를 이 값(중간 회색)으로 맞춘다.
	inline constexpr f32 MiddleGray = 0.18f;

	inline f32 Luminance(f32 r, f32 g, f32 b) { return r * 0.2126f + g * 0.7152f + b * 0.0722f; }

	inline uint32 LuminanceToBin(f32 luminance)
	{
		// 0·음수·NaN은 가장 어두운 구간으로 보낸다.
		if (!(luminance > 0.0f))
			return 0;
		const f32 t = (std::log2(luminance) - MinLogLuminance) / (MaxLogLuminance - MinLogLuminance);
		const f32 clamped = std::clamp(t, 0.0f, 1.0f);
		return std::min(static_cast<uint32>(clamped * HistogramBinCount), HistogramBinCount - 1);
	}

	inline f32 BinToLogLuminance(uint32 bin)
	{
		return MinLogLuminance + (static_cast<f32>(bin) + 0.5f) / HistogramBinCount * (MaxLogLuminance - MinLogLuminance);
	}

	// 아래·위 비율을 뺀 화소들의 평균 log2 밝기. 화소가 없으면 중간 회색을 돌려 노출이 0 EV가 되게 한다.
	inline f32 AverageLogLuminance(std::span<const uint32> bins)
	{
		f64 total = 0.0;
		for (const uint32 count : bins)
			total += count;
		const f64 low = total * LowPercent;
		const f64 high = total * HighPercent;

		f64 seen = 0.0;
		f64 weight = 0.0;
		f64 sum = 0.0;
		for (uint32 bin = 0; bin < bins.size(); ++bin)
		{
			const f64 begin = seen;
			seen += bins[bin];
			// 이 구간에서 [low, high]에 들어가는 화소 수만 더한다.
			const f64 used = std::max(0.0, std::min(seen, high) - std::max(begin, low));
			weight += used;
			sum += used * BinToLogLuminance(bin);
		}
		return weight > 0.0 ? static_cast<f32>(sum / weight) : std::log2(MiddleGray);
	}

	// 평균 밝기를 중간 회색으로 맞추는 노출. 최소·최대로 자른 뒤 보정을 더한다.
	inline f32 TargetExposureEV(f32 averageLogLuminance, f32 minEV, f32 maxEV, f32 compensationEV)
	{
		const f32 low = std::min(minEV, maxEV);
		const f32 high = std::max(minEV, maxEV);
		return std::clamp(std::log2(MiddleGray) - averageLogLuminance, low, high) + compensationEV;
	}

	// 이전 노출을 목표로 지수적으로 옮긴다. 장면이 밝아져 노출을 낮출 때와 어두워져 올릴 때의 속도(1/초)가 다르다.
	inline f32 AdaptExposureEV(f32 previousEV, f32 targetEV, f32 deltaTime, f32 speedToLight, f32 speedToDark)
	{
		const f32 speed = targetEV < previousEV ? speedToLight : speedToDark;
		if (!(deltaTime > 0.0f) || !(speed > 0.0f))
			return previousEV;
		return previousEV + (targetEV - previousEV) * (1.0f - std::exp(-deltaTime * speed));
	}
}

END
