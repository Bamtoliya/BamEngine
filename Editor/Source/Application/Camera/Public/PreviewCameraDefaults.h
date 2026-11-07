#pragma once

#include "BoundingBox.h"
#include <algorithm>
#include <cmath>
#include <glm/gtc/quaternion.hpp>
#include <limits>
#include <optional>

namespace Editor::PreviewCameraDefaults
{
	inline constexpr float PitchDegrees = 15.0f;
	inline constexpr float YawDegrees = 160.0f;
	inline constexpr float FieldOfViewDegrees = 45.0f;
	inline constexpr double DefaultFramingMargin = 1.35;
	inline constexpr double ThumbnailFramingMargin = 1.08;

	struct Framing
	{
		glm::vec3 Center;
		float Distance;
		float NearPlane;
		float FarPlane;
	};

	inline glm::vec3 GetForward()
	{
		const glm::vec3 angles = glm::radians(glm::vec3(PitchDegrees, YawDegrees, 0.0f));
		return glm::quat(angles) * glm::vec3(0.0f, 0.0f, 1.0f);
	}

	inline std::optional<Framing> CalculateFraming(const Engine::BoundingBox& bounds, float aspectRatio, float fieldOfView, double margin = DefaultFramingMargin)
	{
		if (!bounds.IsValid() || !std::isfinite(aspectRatio) || aspectRatio <= 0.0f ||
			!std::isfinite(fieldOfView) || fieldOfView <= 0.0f || fieldOfView >= 180.0f ||
			!std::isfinite(margin) || margin <= 0.0)
			return std::nullopt;

		const glm::dvec3 lower(bounds.min);
		const glm::dvec3 upper(bounds.max);
		const double radius = std::max(glm::length((upper - lower) * 0.5), 0.0001);
		const double verticalHalfFov = glm::radians(static_cast<double>(fieldOfView) * 0.5);
		const double limitingHalfFov = std::atan(std::tan(verticalHalfFov) * std::min(1.0, static_cast<double>(aspectRatio)));
		const double distance = radius * margin / std::sin(limitingHalfFov);
		const double farPlane = distance * 15.0;
		if (!std::isfinite(farPlane) || farPlane > (std::numeric_limits<float>::max)())
			return std::nullopt;

		return Framing{ glm::vec3((lower + upper) * 0.5), static_cast<float>(distance),
			static_cast<float>(distance * 0.01), static_cast<float>(farPlane) };
	}
}
