#pragma once
#include "Engine_API.h"
#include "Reflection/ReflectionMacro.h"
#include <cmath>
#include <limits>

namespace Engine
{
	STRUCT(DISPLAY_NAME("STRUCT_BOUNDING_BOX"), CATEGORY("CATEGORY_BOUNDS"))
	struct ENGINE_API BoundingBox
	{
		REFLECT_BODY()

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_MIN"))
		vec3 min = { 0.f, 0.f, 0.f };
		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_MAX"))
		vec3 max = { 0.f, 0.f, 0.f };

		BoundingBox() {}
		BoundingBox(const vec3& min, const vec3& max)
			: min(min), max(max) {
		}
		vec3 Center() const { return (min + max) * 0.5f; }
		vec3 Extent() const { return (max - min) * 0.5f; }
        bool IsValid() const
        {
            return std::isfinite(min.x) && std::isfinite(min.y) && std::isfinite(min.z) &&
                std::isfinite(max.x) && std::isfinite(max.y) && std::isfinite(max.z) &&
                min.x <= max.x && min.y <= max.y && min.z <= max.z;
        }
        bool TryTransformToAABB(const mat4& matrix, BoundingBox& result) const
        {
            if (!IsValid())
                return false;

            for (int column = 0; column < 4; ++column)
            {
                for (int row = 0; row < 4; ++row)
                {
                    if (!std::isfinite(matrix[column][row]))
                        return false;
                }
            }

            // 스케일과 전단을 포함한 아핀 변환을 지원합니다.
            if (matrix[0][3] != 0.f || matrix[1][3] != 0.f ||
                matrix[2][3] != 0.f || matrix[3][3] != 1.f)
                return false;

            const glm::dmat4 transform(matrix);
            const double limit = std::numeric_limits<f32>::max();
            glm::dvec3 lower(std::numeric_limits<double>::max());
            glm::dvec3 upper(std::numeric_limits<double>::lowest());

            for (int index = 0; index < 8; ++index)
            {
                const glm::dvec4 corner(
                    (index & 1) ? max.x : min.x,
                    (index & 2) ? max.y : min.y,
                    (index & 4) ? max.z : min.z, 1.0);
                const glm::dvec3 point(transform * corner);

                for (int axis = 0; axis < 3; ++axis)
                {
                    if (!std::isfinite(point[axis]) || point[axis] < -limit || point[axis] > limit)
                        return false;
                }

                lower = glm::min(lower, point);
                upper = glm::max(upper, point);
            }

            BoundingBox candidate{ vec3(lower), vec3(upper) };
            for (int axis = 0; axis < 3; ++axis)
            {
                // float 변환 시 반올림으로 경계가 안쪽으로 줄어들지 않도록 합니다.
                if (static_cast<double>(candidate.min[axis]) > lower[axis])
                    candidate.min[axis] = std::nextafter(candidate.min[axis], -std::numeric_limits<f32>::infinity());

                if (static_cast<double>(candidate.max[axis]) < upper[axis])
                    candidate.max[axis] = std::nextafter(candidate.max[axis], std::numeric_limits<f32>::infinity());
            }

            result = candidate;
            return true;
        }
		bool Contains(const vec3& point) const
		{
			return point.x >= min.x && point.x <= max.x &&
				point.y >= min.y && point.y <= max.y &&
				point.z >= min.z && point.z <= max.z;
		}
	};
}