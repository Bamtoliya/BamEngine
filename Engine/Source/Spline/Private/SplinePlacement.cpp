#include "SplinePlacement.h"
#include "SplineFrameTable.h"

namespace Engine
{
	bool SplinePlacement::BuildMatrices(std::span<const vec3> points, bool closed, const mat4& localToWorld,
		const SplinePlacementSettings& settings, std::vector<mat4>& outMatrices,
		std::span<const f32> pointRollDegrees)
	{
		if (!std::isfinite(settings.Spacing) || settings.Spacing <= 0.0 ||
			!std::isfinite(settings.StartDistance) || settings.StartDistance < 0.0)
			return false;

		SplineFrameTable frames;
		if (!frames.Build(points, closed, localToWorld, settings.InitialUp, pointRollDegrees))
			return false;

		const f64 length = frames.GetLength();
		if (length == 0.0 || settings.StartDistance > length || (closed && settings.StartDistance == length))
		{
			outMatrices.clear();
			return true;
		}

		const f64 remainingSteps = (length - settings.StartDistance) / settings.Spacing;
		if (!std::isfinite(remainingSteps))
			return false;

		const f64 requestedCount = closed ?
			std::max(1.0, std::ceil(remainingSteps)) : std::floor(remainingSteps) + 1.0;
		if (requestedCount > static_cast<f64>(MaxInstanceCount))
			return false;

		const size_t count = static_cast<size_t>(requestedCount);
		std::vector<mat4> candidate;
		candidate.reserve(count);

		for (size_t i = 0; i < count; ++i)
		{
			const f64 distance = settings.StartDistance + static_cast<f64>(i) * settings.Spacing;
			if (distance > length || (closed && distance >= length))
				break;

			mat4 matrix;
			if (!frames.TryEvaluate(distance, matrix))
				return false;

			candidate.push_back(matrix);
		}

		outMatrices = std::move(candidate);
		return true;
	}
}