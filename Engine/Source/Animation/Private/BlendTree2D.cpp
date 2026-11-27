#include "BlendTree2D.h"
#include <algorithm>
#include <cmath>
#include <limits>

bool Engine::BlendTree2D::EvaluateGrid(float x, float y, std::array<BlendClipWeight2D, 4>& out) const
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

bool Engine::BlendTree2D::EvaluateGradientBand(float x, float y, std::vector<BlendClipWeight2D>& out) const
{
	out.clear();
	if (!std::isfinite(x) || !std::isfinite(y))
		return false;

	struct ValidSample
	{
		double X = 0.0;
		double Y = 0.0;
		const Animation* Clip = nullptr;
	};

	std::vector<ValidSample> valid;
	valid.reserve(samples.size());
	for (const auto& sample : samples)
	{
		const auto* clip = sample.clip.Get();
		if (!clip || !std::isfinite(sample.x) || !std::isfinite(sample.y))
			continue;
		valid.push_back({ static_cast<double>(sample.x), static_cast<double>(sample.y), clip });
	}

	if (valid.empty())
		return false;

	if (valid.size() == 1)
	{
		out.push_back({ valid[0].Clip, 1.0f });
		return true;
	}

	const double px = static_cast<double>(x);
	const double py = static_cast<double>(y);
	constexpr double epsilonSq = 1e-10;

	// 정확히 샘플 위치에 있는 경우 해당 샘플 가중치 1.0 보장
	for (size_t i = 0; i < valid.size(); ++i)
	{
		const double dx = px - valid[i].X;
		const double dy = py - valid[i].Y;
		if (dx * dx + dy * dy <= epsilonSq)
		{
			out.push_back({ valid[i].Clip, 1.0f });
			return true;
		}
	}

	// Johansen (2009) Freeform Cartesian Gradient Band Interpolation
	std::vector<double> weights(valid.size(), 0.0);
	double totalWeight = 0.0;
	for (size_t i = 0; i < valid.size(); ++i)
	{
		const double ix = px - valid[i].X;
		const double iy = py - valid[i].Y;
		double minBand = std::numeric_limits<double>::infinity();

		for (size_t j = 0; j < valid.size(); ++j)
		{
			if (i == j) continue;
			const double dx = valid[j].X - valid[i].X;
			const double dy = valid[j].Y - valid[i].Y;
			const double distSq = dx * dx + dy * dy;
			if (distSq <= epsilonSq) continue;

			const double band = 1.0 - (ix * dx + iy * dy) / distSq;
			if (band < minBand)
				minBand = band;
		}

		const double w = std::isfinite(minBand) ? std::max(0.0, minBand) : 0.0;
		weights[i] = w;
		totalWeight += w;
	}

	if (totalWeight <= epsilonSq)
	{
		size_t nearestIndex = 0;
		double nearestDistSq = std::numeric_limits<double>::infinity();
		for (size_t i = 0; i < valid.size(); ++i)
		{
			const double dx = px - valid[i].X;
			const double dy = py - valid[i].Y;
			const double d2 = dx * dx + dy * dy;
			if (d2 < nearestDistSq)
			{
				nearestDistSq = d2;
				nearestIndex = i;
			}
		}
		out.push_back({ valid[nearestIndex].Clip, 1.0f });
		return true;
	}

	out.reserve(valid.size());
	for (size_t i = 0; i < valid.size(); ++i)
	{
		if (weights[i] <= 0.0) continue;
		out.push_back({ valid[i].Clip, static_cast<float>(weights[i] / totalWeight) });
	}
	return !out.empty();
}

bool Engine::BlendTree2D::Evaluate(float x, float y, std::array<BlendClipWeight2D, 4>& out) const
{
	if (samples.empty())
		return EvaluateGrid(x, y, out);

	out = {};
	std::vector<BlendClipWeight2D> evaluated;
	if (!EvaluateGradientBand(x, y, evaluated))
		return false;

	std::sort(evaluated.begin(), evaluated.end(), [](const BlendClipWeight2D& a, const BlendClipWeight2D& b) {
		return a.Weight > b.Weight;
	});

	const size_t count = std::min<size_t>(evaluated.size(), 4);
	float sum = 0.0f;
	for (size_t i = 0; i < count; ++i)
		sum += evaluated[i].Weight;
	if (sum <= 0.0f)
		return false;

	for (size_t i = 0; i < count; ++i)
		out[i] = { evaluated[i].Clip, evaluated[i].Weight / sum };
	return true;
}

bool Engine::BlendTree2D::Evaluate(float x, float y, std::vector<BlendClipWeight2D>& out) const
{
	out.clear();
	if (!samples.empty())
		return EvaluateGradientBand(x, y, out);

	std::array<BlendClipWeight2D, 4> grid{};
	if (!EvaluateGrid(x, y, grid))
		return false;

	for (const auto& entry : grid)
	{
		if (entry.Clip && entry.Weight > 0.0f)
			out.push_back(entry);
	}
	return !out.empty();
}