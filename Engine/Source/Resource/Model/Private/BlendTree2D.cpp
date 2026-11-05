#include "BlendTree2D.h"
#include <algorithm>
#include <cmath>

bool Engine::BlendTree2D::Evaluate(float x, float y, std::array<BlendClipWeight2D, 4>& out) const
{
	out = {};

	if (!std::isfinite(x) || !std::isfinite(y) ||
		!std::isfinite(minX) || !std::isfinite(maxX) || minX >= maxX ||
		!std::isfinite(minY) || !std::isfinite(maxY) || minY >= maxY)
	{
		return false;
	}

	const auto normalize = [](float value, float min, float max)
		{
			const double clamped = std::clamp(value, min, max);
			return static_cast<float>((clamped - min) / (static_cast<double>(max) - min));
		};

	const float u = normalize(x, minX, maxX);
	const float v = normalize(y, minY, maxY);
	const std::array<float, 4> weights{
		(1.0f - u) * (1.0f - v), u * (1.0f - v),
		(1.0f - u) * v, u * v
	};
	const std::array<const ResourceHandle<Animation>*, 4> handles{
		&clip00, &clip10, &clip01, &clip11
	};

	std::array<BlendClipWeight2D, 4> result{};
	for (size_t i = 0; i < weights.size(); ++i)
	{
		if (weights[i] == 0.0f) continue;

		const auto* clip = handles[i]->Get();
		if (!clip) return false;

		result[i] = { clip, weights[i] };
	}

	out = result;
	return true;
}