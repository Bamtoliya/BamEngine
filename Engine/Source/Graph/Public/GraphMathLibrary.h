#pragma once

#include "GraphAnnotations.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace Engine::GraphMath
{

inline float Checked(double value)
{
	if (!std::isfinite(value) || std::abs(value) > std::numeric_limits<float>::max())
		throw std::domain_error("Math result is not a finite Float.");
	return static_cast<float>(value);
}

inline float AddFloat(float a, float b)
{
	return Checked(static_cast<double>(a) + b);
}

inline float MultiplyFloat(float a, float b)
{
	return Checked(static_cast<double>(a) * b);
}

FUNCTION(GRAPH_PURE("Math.SubtractFloat"), CATEGORY("Math/Float"),
	GRAPH_PIN_DEFAULT("a", 0.0f), GRAPH_PIN_DEFAULT("b", 0.0f))
inline float SubtractFloat(float a, float b)
{
	return Checked(static_cast<double>(a) - b);
}

FUNCTION(GRAPH_PURE("Math.DivideFloat"), CATEGORY("Math/Float"),
	GRAPH_PIN_DEFAULT("a", 0.0f), GRAPH_PIN_DEFAULT("b", 1.0f))
inline float DivideFloat(float a, float b)
{
	if (b == 0.0f) throw std::domain_error("Division by zero.");
	return Checked(static_cast<double>(a) / b);
}

FUNCTION(GRAPH_PURE("Math.RemainderFloat"), CATEGORY("Math/Float"),
	GRAPH_PIN_DEFAULT("a", 0.0f), GRAPH_PIN_DEFAULT("b", 1.0f))
inline float RemainderFloat(float a, float b)
{
	if (b == 0.0f) throw std::domain_error("Remainder by zero.");
	return Checked(std::fmod(a, b));
}

FUNCTION(GRAPH_PURE("Math.NegateFloat"), CATEGORY("Math/Float"),
	GRAPH_PIN_DEFAULT("value", 0.0f))
inline float NegateFloat(float value)
{
	return -value;
}

FUNCTION(GRAPH_PURE("Math.MinFloat"), CATEGORY("Math/Float"),
	GRAPH_PIN_DEFAULT("a", 0.0f), GRAPH_PIN_DEFAULT("b", 0.0f))
inline float MinFloat(float a, float b)
{
	return std::min(a, b);
}

FUNCTION(GRAPH_PURE("Math.MaxFloat"), CATEGORY("Math/Float"),
	GRAPH_PIN_DEFAULT("a", 0.0f), GRAPH_PIN_DEFAULT("b", 0.0f))
inline float MaxFloat(float a, float b)
{
	return std::max(a, b);
}

FUNCTION(GRAPH_PURE("Math.ClampFloat"), CATEGORY("Math/Float"),
	GRAPH_PIN_DEFAULT("value", 0.0f), GRAPH_PIN_DEFAULT("minimum", 0.0f), GRAPH_PIN_DEFAULT("maximum", 1.0f))
inline float ClampFloat(float value, float minimum, float maximum)
{
	if (minimum > maximum) throw std::domain_error("Clamp minimum exceeds maximum.");
	return std::clamp(value, minimum, maximum);
}

FUNCTION(GRAPH_PURE("Math.SaturateFloat"), CATEGORY("Math/Float"),
	GRAPH_PIN_DEFAULT("value", 0.0f))
inline float SaturateFloat(float value)
{
	return std::clamp(value, 0.0f, 1.0f);
}

FUNCTION(GRAPH_PURE("Math.LerpFloat"), CATEGORY("Math/Float"),
	GRAPH_PIN_DEFAULT("a", 0.0f), GRAPH_PIN_DEFAULT("b", 1.0f), GRAPH_PIN_DEFAULT("t", 0.5f))
inline float LerpFloat(float a, float b, float t)
{
	return Checked(std::lerp(static_cast<double>(a), static_cast<double>(b), static_cast<double>(t)));
}

FUNCTION(GRAPH_PURE("Math.InverseLerpFloat"), CATEGORY("Math/Float"),
	GRAPH_PIN_DEFAULT("a", 0.0f), GRAPH_PIN_DEFAULT("b", 1.0f), GRAPH_PIN_DEFAULT("value", 0.0f))
inline float InverseLerpFloat(float a, float b, float value)
{
	if (a == b) throw std::domain_error("InverseLerp interval has zero width.");
	return Checked((static_cast<double>(value) - a) / (static_cast<double>(b) - a));
}

FUNCTION(GRAPH_PURE("Math.RemapFloat"), CATEGORY("Math/Float"),
	GRAPH_PIN_DEFAULT("value", 0.0f), GRAPH_PIN_DEFAULT("inMin", 0.0f), GRAPH_PIN_DEFAULT("inMax", 1.0f),
	GRAPH_PIN_DEFAULT("outMin", 0.0f), GRAPH_PIN_DEFAULT("outMax", 1.0f))
inline float RemapFloat(float value, float inMin, float inMax, float outMin, float outMax)
{
	if (inMin == inMax) throw std::domain_error("Remap input interval has zero width.");
	const double t = (static_cast<double>(value) - inMin) / (static_cast<double>(inMax) - inMin);
	return Checked(std::lerp(static_cast<double>(outMin), static_cast<double>(outMax), t));
}

FUNCTION(GRAPH_PURE("Math.SmoothStepFloat"), CATEGORY("Math/Float"),
	GRAPH_PIN_DEFAULT("edge0", 0.0f), GRAPH_PIN_DEFAULT("edge1", 1.0f), GRAPH_PIN_DEFAULT("value", 0.0f))
inline float SmoothStepFloat(float edge0, float edge1, float value)
{
	if (edge0 >= edge1) throw std::domain_error("SmoothStep requires edge0 < edge1.");
	const double t = std::clamp((static_cast<double>(value) - edge0) / (static_cast<double>(edge1) - edge0), 0.0, 1.0);
	return static_cast<float>(t * t * (3.0 - 2.0 * t));
}

FUNCTION(GRAPH_PURE("Math.SqrtFloat"), CATEGORY("Math/Float"),
	GRAPH_PIN_DEFAULT("value", 0.0f))
inline float SqrtFloat(float value)
{
	if (value < 0.0f) throw std::domain_error("Sqrt requires a non-negative input.");
	return std::sqrt(value);
}

FUNCTION(GRAPH_PURE("Math.PowFloat"), CATEGORY("Math/Float"),
	GRAPH_PIN_DEFAULT("base", 1.0f), GRAPH_PIN_DEFAULT("exponent", 1.0f))
inline float PowFloat(float base, float exponent)
{
	return Checked(std::pow(static_cast<double>(base), static_cast<double>(exponent)));
}

FUNCTION(GRAPH_PURE("Math.ExpFloat"), CATEGORY("Math/Float"),
	GRAPH_PIN_DEFAULT("value", 0.0f))
inline float ExpFloat(float value)
{
	return Checked(std::exp(static_cast<double>(value)));
}

FUNCTION(GRAPH_PURE("Math.LogFloat"), CATEGORY("Math/Float"),
	GRAPH_PIN_DEFAULT("value", 1.0f))
inline float LogFloat(float value)
{
	if (value <= 0.0f) throw std::domain_error("Log requires a positive input.");
	return std::log(value);
}

FUNCTION(GRAPH_PURE("Math.Log2Float"), CATEGORY("Math/Float"),
	GRAPH_PIN_DEFAULT("value", 1.0f))
inline float Log2Float(float value)
{
	if (value <= 0.0f) throw std::domain_error("Log2 requires a positive input.");
	return std::log2(value);
}

FUNCTION(GRAPH_PURE("Math.FloorFloat"), CATEGORY("Math/Float"),
	GRAPH_PIN_DEFAULT("value", 0.0f))
inline float FloorFloat(float value)
{
	return Checked(std::floor(static_cast<double>(value)));
}

FUNCTION(GRAPH_PURE("Math.CeilFloat"), CATEGORY("Math/Float"),
	GRAPH_PIN_DEFAULT("value", 0.0f))
inline float CeilFloat(float value)
{
	return Checked(std::ceil(static_cast<double>(value)));
}

FUNCTION(GRAPH_PURE("Math.RoundFloat"), CATEGORY("Math/Float"),
	GRAPH_PIN_DEFAULT("value", 0.0f))
inline float RoundFloat(float value)
{
	return Checked(std::round(static_cast<double>(value)));
}

FUNCTION(GRAPH_PURE("Math.TruncFloat"), CATEGORY("Math/Float"),
	GRAPH_PIN_DEFAULT("value", 0.0f))
inline float TruncFloat(float value)
{
	return Checked(std::trunc(static_cast<double>(value)));
}

FUNCTION(GRAPH_PURE("Math.SinFloat"), CATEGORY("Math/Float"), TOOLTIP("Angles use radians."),
	GRAPH_PIN_DEFAULT("value", 0.0f))
inline float SinFloat(float value)
{
	return Checked(std::sin(static_cast<double>(value)));
}

FUNCTION(GRAPH_PURE("Math.CosFloat"), CATEGORY("Math/Float"), TOOLTIP("Angles use radians."),
	GRAPH_PIN_DEFAULT("value", 0.0f))
inline float CosFloat(float value)
{
	return Checked(std::cos(static_cast<double>(value)));
}

FUNCTION(GRAPH_PURE("Math.TanFloat"), CATEGORY("Math/Float"), TOOLTIP("Angles use radians."),
	GRAPH_PIN_DEFAULT("value", 0.0f))
inline float TanFloat(float value)
{
	return Checked(std::tan(static_cast<double>(value)));
}

FUNCTION(GRAPH_PURE("Math.AtanFloat"), CATEGORY("Math/Float"), TOOLTIP("Angles use radians."),
	GRAPH_PIN_DEFAULT("value", 0.0f))
inline float AtanFloat(float value)
{
	return Checked(std::atan(static_cast<double>(value)));
}

FUNCTION(GRAPH_PURE("Math.AsinFloat"), CATEGORY("Math/Float"), TOOLTIP("Angles use radians."),
	GRAPH_PIN_DEFAULT("value", 0.0f))
inline float AsinFloat(float value)
{
	if (value < -1.0f || value > 1.0f) throw std::domain_error("Asin input must be in [-1, 1].");
	return std::asin(value);
}

FUNCTION(GRAPH_PURE("Math.AcosFloat"), CATEGORY("Math/Float"), TOOLTIP("Angles use radians."),
	GRAPH_PIN_DEFAULT("value", 0.0f))
inline float AcosFloat(float value)
{
	if (value < -1.0f || value > 1.0f) throw std::domain_error("Acos input must be in [-1, 1].");
	return std::acos(value);
}

FUNCTION(GRAPH_PURE("Math.Atan2Float"), CATEGORY("Math/Float"), TOOLTIP("Angles use radians."),
	GRAPH_PIN_DEFAULT("y", 0.0f), GRAPH_PIN_DEFAULT("x", 1.0f))
inline float Atan2Float(float y, float x)
{
	if (x == 0.0f && y == 0.0f) throw std::domain_error("Atan2 direction has zero length.");
	return std::atan2(y, x);
}

FUNCTION(GRAPH_PURE("Math.RadiansFloat"), CATEGORY("Math/Float"),
	GRAPH_PIN_DEFAULT("degrees", 0.0f))
inline float RadiansFloat(float degrees)
{
	return Checked(static_cast<double>(degrees) * std::numbers::pi / 180.0);
}

FUNCTION(GRAPH_PURE("Math.DegreesFloat"), CATEGORY("Math/Float"),
	GRAPH_PIN_DEFAULT("radians", 0.0f))
inline float DegreesFloat(float radians)
{
	return Checked(static_cast<double>(radians) * 180.0 / std::numbers::pi);
}

FUNCTION(GRAPH_PURE("Math.PiFloat"), CATEGORY("Math/Float"))
inline float PiFloat()
{
	return std::numbers::pi_v<float>;
}

FUNCTION(GRAPH_PURE("Math.EqualFloat"), CATEGORY("Math/Float"),
	GRAPH_PIN_DEFAULT("a", 0.0f), GRAPH_PIN_DEFAULT("b", 0.0f))
inline bool EqualFloat(float a, float b)
{
	return a == b;
}

FUNCTION(GRAPH_PURE("Math.NotEqualFloat"), CATEGORY("Math/Float"),
	GRAPH_PIN_DEFAULT("a", 0.0f), GRAPH_PIN_DEFAULT("b", 0.0f))
inline bool NotEqualFloat(float a, float b)
{
	return a != b;
}

FUNCTION(GRAPH_PURE("Math.LessOrEqualFloat"), CATEGORY("Math/Float"),
	GRAPH_PIN_DEFAULT("a", 0.0f), GRAPH_PIN_DEFAULT("b", 0.0f))
inline bool LessOrEqualFloat(float a, float b)
{
	return a <= b;
}

FUNCTION(GRAPH_PURE("Math.GreaterOrEqualFloat"), CATEGORY("Math/Float"),
	GRAPH_PIN_DEFAULT("a", 0.0f), GRAPH_PIN_DEFAULT("b", 0.0f))
inline bool GreaterOrEqualFloat(float a, float b)
{
	return a >= b;
}

FUNCTION(GRAPH_PURE("Math.NearlyEqualFloat"), CATEGORY("Math/Float"),
	GRAPH_PIN_DEFAULT("a", 0.0f), GRAPH_PIN_DEFAULT("b", 0.0f), GRAPH_PIN_DEFAULT("tolerance", 0.0001f))
inline bool NearlyEqualFloat(float a, float b, float tolerance)
{
	if (tolerance < 0.0f) throw std::domain_error("Tolerance must be non-negative.");
	return std::abs(static_cast<double>(a) - b) <= tolerance;
}

FUNCTION(GRAPH_PURE("Math.AddInt"), CATEGORY("Math/Int"),
	GRAPH_PIN_DEFAULT("a", 0), GRAPH_PIN_DEFAULT("b", 0))
inline std::int64_t AddInt(std::int64_t a, std::int64_t b)
{
	const auto min = std::numeric_limits<std::int64_t>::min();
	const auto max = std::numeric_limits<std::int64_t>::max();
	if ((b > 0 && a > max - b) || (b < 0 && a < min - b)) throw std::overflow_error("Int addition overflow.");
	return a + b;
}

FUNCTION(GRAPH_PURE("Math.SubtractInt"), CATEGORY("Math/Int"),
	GRAPH_PIN_DEFAULT("a", 0), GRAPH_PIN_DEFAULT("b", 0))
inline std::int64_t SubtractInt(std::int64_t a, std::int64_t b)
{
	const auto min = std::numeric_limits<std::int64_t>::min();
	const auto max = std::numeric_limits<std::int64_t>::max();
	if ((b > 0 && a < min + b) || (b < 0 && a > max + b)) throw std::overflow_error("Int subtraction overflow.");
	return a - b;
}

FUNCTION(GRAPH_PURE("Math.MultiplyInt"), CATEGORY("Math/Int"),
	GRAPH_PIN_DEFAULT("a", 1), GRAPH_PIN_DEFAULT("b", 1))
inline std::int64_t MultiplyInt(std::int64_t a, std::int64_t b)
{
	const auto min = std::numeric_limits<std::int64_t>::min();
	const auto max = std::numeric_limits<std::int64_t>::max();
	if ((a > 0 && ((b > 0 && a > max / b) || (b < 0 && b < min / a))) ||
		(a < 0 && ((b > 0 && a < min / b) || (b < 0 && a < max / b))))
		throw std::overflow_error("Int multiplication overflow.");
	return a * b;
}

FUNCTION(GRAPH_PURE("Math.DivideInt"), CATEGORY("Math/Int"),
	GRAPH_PIN_DEFAULT("a", 0), GRAPH_PIN_DEFAULT("b", 1))
inline std::int64_t DivideInt(std::int64_t a, std::int64_t b)
{
	if (b == 0) throw std::domain_error("Int division by zero.");
	if (a == std::numeric_limits<std::int64_t>::min() && b == -1) throw std::overflow_error("Int division overflow.");
	return a / b;
}

FUNCTION(GRAPH_PURE("Math.RemainderInt"), CATEGORY("Math/Int"),
	GRAPH_PIN_DEFAULT("a", 0), GRAPH_PIN_DEFAULT("b", 1))
inline std::int64_t RemainderInt(std::int64_t a, std::int64_t b)
{
	if (b == 0) throw std::domain_error("Int remainder by zero.");
	if (a == std::numeric_limits<std::int64_t>::min() && b == -1) return 0;
	return a % b;
}

FUNCTION(GRAPH_PURE("Math.MinInt"), CATEGORY("Math/Int"),
	GRAPH_PIN_DEFAULT("a", 0), GRAPH_PIN_DEFAULT("b", 0))
inline std::int64_t MinInt(std::int64_t a, std::int64_t b)
{
	return std::min(a, b);
}

FUNCTION(GRAPH_PURE("Math.MaxInt"), CATEGORY("Math/Int"),
	GRAPH_PIN_DEFAULT("a", 0), GRAPH_PIN_DEFAULT("b", 0))
inline std::int64_t MaxInt(std::int64_t a, std::int64_t b)
{
	return std::max(a, b);
}

FUNCTION(GRAPH_PURE("Math.ClampInt"), CATEGORY("Math/Int"),
	GRAPH_PIN_DEFAULT("value", 0), GRAPH_PIN_DEFAULT("minimum", 0), GRAPH_PIN_DEFAULT("maximum", 1))
inline std::int64_t ClampInt(std::int64_t value, std::int64_t minimum, std::int64_t maximum)
{
	if (minimum > maximum) throw std::domain_error("Clamp minimum exceeds maximum.");
	return std::clamp(value, minimum, maximum);
}

FUNCTION(GRAPH_PURE("Math.IntToFloat"), CATEGORY("Math/Int"),
	GRAPH_PIN_DEFAULT("value", 0))
inline float IntToFloat(std::int64_t value)
{
	return static_cast<float>(value);
}

FUNCTION(GRAPH_PURE("Math.FloatToInt"), CATEGORY("Math/Int"),
	GRAPH_PIN_DEFAULT("value", 0.0f))
inline std::int64_t FloatToInt(float value)
{
	const double number = std::trunc(static_cast<double>(value));
	if (!std::isfinite(number) || number < -9223372036854775808.0 || number >= 9223372036854775808.0)
		throw std::overflow_error("Float is outside the Int range.");
	return static_cast<std::int64_t>(number);
}

FUNCTION(GRAPH_PURE("Math.NegateInt"), CATEGORY("Math/Int"),
	GRAPH_PIN_DEFAULT("value", 0))
inline std::int64_t NegateInt(std::int64_t value)
{
	if (value == std::numeric_limits<std::int64_t>::min()) throw std::overflow_error("Int negation overflow.");
	return -value;
}

FUNCTION(GRAPH_PURE("Math.AbsInt"), CATEGORY("Math/Int"),
	GRAPH_PIN_DEFAULT("value", 0))
inline std::int64_t AbsInt(std::int64_t value)
{
	return value < 0 ? NegateInt(value) : value;
}

FUNCTION(GRAPH_PURE("Math.EqualInt"), CATEGORY("Math/Int"),
	GRAPH_PIN_DEFAULT("a", 0), GRAPH_PIN_DEFAULT("b", 0))
inline bool EqualInt(std::int64_t a, std::int64_t b)
{
	return a == b;
}

FUNCTION(GRAPH_PURE("Math.NotEqualInt"), CATEGORY("Math/Int"),
	GRAPH_PIN_DEFAULT("a", 0), GRAPH_PIN_DEFAULT("b", 0))
inline bool NotEqualInt(std::int64_t a, std::int64_t b)
{
	return a != b;
}

FUNCTION(GRAPH_PURE("Math.GreaterInt"), CATEGORY("Math/Int"),
	GRAPH_PIN_DEFAULT("a", 0), GRAPH_PIN_DEFAULT("b", 0))
inline bool GreaterInt(std::int64_t a, std::int64_t b)
{
	return a > b;
}

FUNCTION(GRAPH_PURE("Math.LessInt"), CATEGORY("Math/Int"),
	GRAPH_PIN_DEFAULT("a", 0), GRAPH_PIN_DEFAULT("b", 0))
inline bool LessInt(std::int64_t a, std::int64_t b)
{
	return a < b;
}

FUNCTION(GRAPH_PURE("Math.GreaterOrEqualInt"), CATEGORY("Math/Int"),
	GRAPH_PIN_DEFAULT("a", 0), GRAPH_PIN_DEFAULT("b", 0))
inline bool GreaterOrEqualInt(std::int64_t a, std::int64_t b)
{
	return a >= b;
}

FUNCTION(GRAPH_PURE("Math.LessOrEqualInt"), CATEGORY("Math/Int"),
	GRAPH_PIN_DEFAULT("a", 0), GRAPH_PIN_DEFAULT("b", 0))
inline bool LessOrEqualInt(std::int64_t a, std::int64_t b)
{
	return a <= b;
}

FUNCTION(GRAPH_PURE("Math.SignFloat"), CATEGORY("Math/Float"),
	GRAPH_PIN_DEFAULT("value", 0.0f))
inline float SignFloat(float value)
{
	return value > 0.0f ? 1.0f : (value < 0.0f ? -1.0f : 0.0f);
}

FUNCTION(GRAPH_PURE("Math.FractFloat"), CATEGORY("Math/Float"),
	GRAPH_PIN_DEFAULT("value", 0.0f))
inline float FractFloat(float value)
{
	return value - std::floor(value);
}

FUNCTION(GRAPH_PURE("Math.StepFloat"), CATEGORY("Math/Float"),
	GRAPH_PIN_DEFAULT("edge", 0.0f), GRAPH_PIN_DEFAULT("value", 0.0f))
inline float StepFloat(float edge, float value)
{
	return value < edge ? 0.0f : 1.0f;
}

}

