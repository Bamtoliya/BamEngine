#include "BlendTree1D.h"
#include <algorithm>

void BlendTree1D::AddSample(float threshold, const ResourceHandle<Animation>& clip)
{
	samples.push_back(BlendSample1D{ threshold, clip });
	SortSamples();
}

void BlendTree1D::SortSamples()
{
	std::sort(samples.begin(), samples.end(), [](const BlendSample1D& a, const BlendSample1D& b) {
		return a.threshold < b.threshold;
		});
}

bool BlendTree1D::Evaluate(float paramValue, const Animation*& outClipA, const Animation*& outClipB, float& outAlpha) const
{
	outClipA = nullptr;
	outClipB = nullptr;
	outAlpha = 0.0f;

	if (samples.empty()) return false;
	if (samples.size() == 1)
	{
		outClipA = samples[0].clip.Get();
		return outClipA != nullptr;
	}

	// 파라미터 값이 최솟값 이하인 경우 첫 번째 클립 100%
	if (paramValue <= samples.front().threshold)
	{
		outClipA = samples.front().clip.Get();
		return outClipA != nullptr;
	}

	// 파라미터 값이 최댓값 이상인 경우 마지막 클립 100%
	if (paramValue >= samples.back().threshold)
	{
		outClipA = samples.back().clip.Get();
		return outClipA != nullptr;
	}

	// 인접한 두 샘플 구간 탐색
	for (size_t i = 0; i + 1 < samples.size(); ++i)
	{
		if (paramValue >= samples[i].threshold && paramValue <= samples[i + 1].threshold)
		{
			outClipA = samples[i].clip.Get();
			outClipB = samples[i + 1].clip.Get();
			const float range = samples[i + 1].threshold - samples[i].threshold;
			outAlpha = range > 0.0f ? ((paramValue - samples[i].threshold) / range) : 0.0f;
			return outClipA != nullptr;
		}
	}

	outClipA = samples.back().clip.Get();
	return outClipA != nullptr;
}