#pragma once

#include "GraphMathLibrary.h"
#include <array>

namespace Engine::GraphVector
{
using Float2 = std::array<float, 2>;
using Float3 = std::array<float, 3>;
using Float4 = std::array<float, 4>;

template<std::size_t N>
inline double LengthSquared(std::array<float, N> value)
{
	double result = 0.0;
	for (float item : value) result += static_cast<double>(item) * item;
	return result;
}

template<std::size_t N>
inline std::array<float, N> Normalize(std::array<float, N> value)
{
	const double length = std::sqrt(LengthSquared(value));
	if (length == 0.0) throw std::domain_error("Cannot normalize a zero-length vector.");
	for (float& item : value) item = GraphMath::Checked(item / length);
	return value;
}

FUNCTION(GRAPH_PURE("Math.MakeFloat2"), CATEGORY("Math/Vector2"),
	GRAPH_PIN_DEFAULT("x", 0.0f), GRAPH_PIN_DEFAULT("y", 0.0f))
inline Float2 MakeFloat2(float x, float y)
{
	return { x, y };
}

FUNCTION(GRAPH_PURE("Math.AddFloat2"), CATEGORY("Math/Vector2"))
inline Float2 AddFloat2(Float2 a, Float2 b)
{
	for (std::size_t i = 0; i < a.size(); ++i) a[i] = GraphMath::AddFloat(a[i], b[i]);
	return a;
}

FUNCTION(GRAPH_PURE("Math.SubtractFloat2"), CATEGORY("Math/Vector2"))
inline Float2 SubtractFloat2(Float2 a, Float2 b)
{
	for (std::size_t i = 0; i < a.size(); ++i) a[i] = GraphMath::SubtractFloat(a[i], b[i]);
	return a;
}

FUNCTION(GRAPH_PURE("Math.ScaleFloat2"), CATEGORY("Math/Vector2"),
	GRAPH_PIN_DEFAULT("scale", 1.0f))
inline Float2 ScaleFloat2(Float2 value, float scale)
{
	for (float& item : value) item = GraphMath::MultiplyFloat(item, scale);
	return value;
}

FUNCTION(GRAPH_PURE("Math.DotFloat2"), CATEGORY("Math/Vector2"))
inline float DotFloat2(Float2 a, Float2 b)
{
	double result = 0.0;
	for (std::size_t i = 0; i < a.size(); ++i) result += static_cast<double>(a[i]) * b[i];
	return GraphMath::Checked(result);
}

FUNCTION(GRAPH_PURE("Math.LengthFloat2"), CATEGORY("Math/Vector2"))
inline float LengthFloat2(Float2 value)
{
	return GraphMath::Checked(std::sqrt(LengthSquared(value)));
}

FUNCTION(GRAPH_PURE("Math.DistanceFloat2"), CATEGORY("Math/Vector2"))
inline float DistanceFloat2(Float2 a, Float2 b)
{
	double squared = 0.0;
	for (std::size_t i = 0; i < a.size(); ++i)
	{
		const double delta = static_cast<double>(a[i]) - b[i];
		squared += delta * delta;
	}
	return GraphMath::Checked(std::sqrt(squared));
}

FUNCTION(GRAPH_PURE("Math.NormalizeFloat2"), CATEGORY("Math/Vector2"))
inline Float2 NormalizeFloat2(Float2 value)
{
	return Normalize(value);
}

FUNCTION(GRAPH_PURE("Math.LerpFloat2"), CATEGORY("Math/Vector2"),
	GRAPH_PIN_DEFAULT("t", 0.5f))
inline Float2 LerpFloat2(Float2 a, Float2 b, float t)
{
	for (std::size_t i = 0; i < a.size(); ++i) a[i] = GraphMath::LerpFloat(a[i], b[i], t);
	return a;
}

FUNCTION(GRAPH_PURE("Math.XFloat2"), CATEGORY("Math/Vector2"))
inline float XFloat2(Float2 value)
{
	return value[0];
}

FUNCTION(GRAPH_PURE("Math.YFloat2"), CATEGORY("Math/Vector2"))
inline float YFloat2(Float2 value)
{
	return value[1];
}

FUNCTION(GRAPH_PURE("Math.MakeFloat3"), CATEGORY("Math/Vector3"),
	GRAPH_PIN_DEFAULT("x", 0.0f), GRAPH_PIN_DEFAULT("y", 0.0f),
	GRAPH_PIN_DEFAULT("z", 0.0f))
inline Float3 MakeFloat3(float x, float y, float z)
{
	return { x, y, z };
}

FUNCTION(GRAPH_PURE("Math.AddFloat3"), CATEGORY("Math/Vector3"))
inline Float3 AddFloat3(Float3 a, Float3 b)
{
	for (std::size_t i = 0; i < a.size(); ++i) a[i] = GraphMath::AddFloat(a[i], b[i]);
	return a;
}

FUNCTION(GRAPH_PURE("Math.SubtractFloat3"), CATEGORY("Math/Vector3"))
inline Float3 SubtractFloat3(Float3 a, Float3 b)
{
	for (std::size_t i = 0; i < a.size(); ++i) a[i] = GraphMath::SubtractFloat(a[i], b[i]);
	return a;
}

FUNCTION(GRAPH_PURE("Math.ScaleFloat3"), CATEGORY("Math/Vector3"),
	GRAPH_PIN_DEFAULT("scale", 1.0f))
inline Float3 ScaleFloat3(Float3 value, float scale)
{
	for (float& item : value) item = GraphMath::MultiplyFloat(item, scale);
	return value;
}

FUNCTION(GRAPH_PURE("Math.DotFloat3"), CATEGORY("Math/Vector3"))
inline float DotFloat3(Float3 a, Float3 b)
{
	double result = 0.0;
	for (std::size_t i = 0; i < a.size(); ++i) result += static_cast<double>(a[i]) * b[i];
	return GraphMath::Checked(result);
}

FUNCTION(GRAPH_PURE("Math.LengthFloat3"), CATEGORY("Math/Vector3"))
inline float LengthFloat3(Float3 value)
{
	return GraphMath::Checked(std::sqrt(LengthSquared(value)));
}

FUNCTION(GRAPH_PURE("Math.DistanceFloat3"), CATEGORY("Math/Vector3"))
inline float DistanceFloat3(Float3 a, Float3 b)
{
	double squared = 0.0;
	for (std::size_t i = 0; i < a.size(); ++i)
	{
		const double delta = static_cast<double>(a[i]) - b[i];
		squared += delta * delta;
	}
	return GraphMath::Checked(std::sqrt(squared));
}

FUNCTION(GRAPH_PURE("Math.NormalizeFloat3"), CATEGORY("Math/Vector3"))
inline Float3 NormalizeFloat3(Float3 value)
{
	return Normalize(value);
}

FUNCTION(GRAPH_PURE("Math.LerpFloat3"), CATEGORY("Math/Vector3"),
	GRAPH_PIN_DEFAULT("t", 0.5f))
inline Float3 LerpFloat3(Float3 a, Float3 b, float t)
{
	for (std::size_t i = 0; i < a.size(); ++i) a[i] = GraphMath::LerpFloat(a[i], b[i], t);
	return a;
}

FUNCTION(GRAPH_PURE("Math.XFloat3"), CATEGORY("Math/Vector3"))
inline float XFloat3(Float3 value)
{
	return value[0];
}

FUNCTION(GRAPH_PURE("Math.YFloat3"), CATEGORY("Math/Vector3"))
inline float YFloat3(Float3 value)
{
	return value[1];
}

FUNCTION(GRAPH_PURE("Math.ZFloat3"), CATEGORY("Math/Vector3"))
inline float ZFloat3(Float3 value)
{
	return value[2];
}

FUNCTION(GRAPH_PURE("Math.CrossFloat3"), CATEGORY("Math/Vector3"))
inline Float3 CrossFloat3(Float3 a, Float3 b)
{
	return {
		GraphMath::Checked(static_cast<double>(a[1]) * b[2] - static_cast<double>(a[2]) * b[1]),
		GraphMath::Checked(static_cast<double>(a[2]) * b[0] - static_cast<double>(a[0]) * b[2]),
		GraphMath::Checked(static_cast<double>(a[0]) * b[1] - static_cast<double>(a[1]) * b[0]) };
}

FUNCTION(GRAPH_PURE("Math.MakeFloat4"), CATEGORY("Math/Vector4"),
	GRAPH_PIN_DEFAULT("x", 0.0f), GRAPH_PIN_DEFAULT("y", 0.0f),
	GRAPH_PIN_DEFAULT("z", 0.0f), GRAPH_PIN_DEFAULT("w", 0.0f))
inline Float4 MakeFloat4(float x, float y, float z, float w)
{
	return { x, y, z, w };
}

FUNCTION(GRAPH_PURE("Math.AddFloat4"), CATEGORY("Math/Vector4"))
inline Float4 AddFloat4(Float4 a, Float4 b)
{
	for (std::size_t i = 0; i < a.size(); ++i) a[i] = GraphMath::AddFloat(a[i], b[i]);
	return a;
}

FUNCTION(GRAPH_PURE("Math.SubtractFloat4"), CATEGORY("Math/Vector4"))
inline Float4 SubtractFloat4(Float4 a, Float4 b)
{
	for (std::size_t i = 0; i < a.size(); ++i) a[i] = GraphMath::SubtractFloat(a[i], b[i]);
	return a;
}

FUNCTION(GRAPH_PURE("Math.ScaleFloat4"), CATEGORY("Math/Vector4"),
	GRAPH_PIN_DEFAULT("scale", 1.0f))
inline Float4 ScaleFloat4(Float4 value, float scale)
{
	for (float& item : value) item = GraphMath::MultiplyFloat(item, scale);
	return value;
}

FUNCTION(GRAPH_PURE("Math.DotFloat4"), CATEGORY("Math/Vector4"))
inline float DotFloat4(Float4 a, Float4 b)
{
	double result = 0.0;
	for (std::size_t i = 0; i < a.size(); ++i) result += static_cast<double>(a[i]) * b[i];
	return GraphMath::Checked(result);
}

FUNCTION(GRAPH_PURE("Math.LengthFloat4"), CATEGORY("Math/Vector4"))
inline float LengthFloat4(Float4 value)
{
	return GraphMath::Checked(std::sqrt(LengthSquared(value)));
}

FUNCTION(GRAPH_PURE("Math.DistanceFloat4"), CATEGORY("Math/Vector4"))
inline float DistanceFloat4(Float4 a, Float4 b)
{
	double squared = 0.0;
	for (std::size_t i = 0; i < a.size(); ++i)
	{
		const double delta = static_cast<double>(a[i]) - b[i];
		squared += delta * delta;
	}
	return GraphMath::Checked(std::sqrt(squared));
}

FUNCTION(GRAPH_PURE("Math.NormalizeFloat4"), CATEGORY("Math/Vector4"))
inline Float4 NormalizeFloat4(Float4 value)
{
	return Normalize(value);
}

FUNCTION(GRAPH_PURE("Math.LerpFloat4"), CATEGORY("Math/Vector4"),
	GRAPH_PIN_DEFAULT("t", 0.5f))
inline Float4 LerpFloat4(Float4 a, Float4 b, float t)
{
	for (std::size_t i = 0; i < a.size(); ++i) a[i] = GraphMath::LerpFloat(a[i], b[i], t);
	return a;
}

FUNCTION(GRAPH_PURE("Math.XFloat4"), CATEGORY("Math/Vector4"))
inline float XFloat4(Float4 value)
{
	return value[0];
}

FUNCTION(GRAPH_PURE("Math.YFloat4"), CATEGORY("Math/Vector4"))
inline float YFloat4(Float4 value)
{
	return value[1];
}

FUNCTION(GRAPH_PURE("Math.ZFloat4"), CATEGORY("Math/Vector4"))
inline float ZFloat4(Float4 value)
{
	return value[2];
}

FUNCTION(GRAPH_PURE("Math.WFloat4"), CATEGORY("Math/Vector4"))
inline float WFloat4(Float4 value)
{
	return value[3];
}

}

