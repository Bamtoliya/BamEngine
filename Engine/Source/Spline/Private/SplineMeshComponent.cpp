#include "SplineMeshComponent.h"
#include "SplineComponent.h"
#include "SplinePlacement.h"

namespace Engine
{
	bool SplineMeshComponent::BuildMatrices(const SplineComponent& spline, const mat4& worldMatrix,
		std::vector<mat4>& outMatrices) const
	{
		if (!spline.IsValid())
			return false;

		for (int axis = 0; axis < 3; ++axis)
		{
			if (!std::isfinite(InstanceScale[axis]) || InstanceScale[axis] <= 0.0f)
				return false;
		}

		std::vector<vec3> points;
		std::vector<f32> rolls;
		points.reserve(spline.Points.size());
		rolls.reserve(spline.Points.size());

		for (const auto& point : spline.Points)
		{
			points.push_back(point.Position);
			rolls.push_back(point.RollDegrees);
		}

		SplinePlacementSettings settings;
		settings.Spacing = Spacing;
		settings.StartDistance = StartDistance;
		settings.InitialUp = InitialUp;

		std::vector<mat4> candidate;
		if (!SplinePlacement::BuildMatrices(points, spline.Closed, worldMatrix, settings, candidate, rolls))
			return false;

		for (auto& matrix : candidate)
		{
			for (int axis = 0; axis < 3; ++axis)
			{
				matrix[axis] *= InstanceScale[axis];

				for (int row = 0; row < 4; ++row)
				{
					if (!std::isfinite(matrix[axis][row]))
						return false;
				}
			}
		}

		outMatrices = std::move(candidate);
		return true;
	}
}