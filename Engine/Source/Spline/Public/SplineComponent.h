#pragma once

#include "Engine_Includes.h"
#include "ComponentAnnotations.h"

namespace Engine
{
	STRUCT(DISPLAY_NAME("STRUCT_SPLINE_POINT"))
		struct ENGINE_API SplinePoint
	{
		REFLECT_BODY()

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_POSITION"))
		vec3 Position{ 0.0f };

		PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_SPLINE_ROLL"))
		f32 RollDegrees = 0.0f;

		bool operator==(const SplinePoint& other) const = default;
	};

	STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_SPLINE"), CATEGORY("CATEGORY_SPLINE"),
		REQUIRE_COMPONENTS(TransformComponent, WorldTransformComponent))
		struct ENGINE_API SplineComponent
	{
		REFLECT_BODY()

			static constexpr size_t MaxControlPoints = 64;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_SPLINE_POINTS"))
			std::vector<SplinePoint> Points{
				SplinePoint{ vec3(0.0f) },
				SplinePoint{ vec3(2.0f, 0.0f, 2.0f) },
				SplinePoint{ vec3(4.0f, 0.0f, 0.0f) }
		};

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_SPLINE_CLOSED"))
			bool Closed = false;

		bool IsValid() const
		{
			if (Points.empty() || Points.size() > MaxControlPoints || (Closed && Points.size() < 3))
				return false;

			for (const auto& point : Points)
			{
				const auto& position = point.Position;
				if (!std::isfinite(position.x) || !std::isfinite(position.y) ||
					!std::isfinite(position.z) || !std::isfinite(point.RollDegrees))
					return false;
			}

			return true;
		}
	};
}