#pragma once

#include "Engine_API.h"
#include "ReflectionMacro.h"
#include <vector>
#include <cstdint>
#include <cstddef>

namespace Engine
{
	ENUM(DISPLAY_NAME("ENUM_CURVE_INTERP_MODE"))
		enum class ECurveInterpMode : uint8
	{
		Constant = 0,   // 스텝: 다음 키 전까지 현재 값 유지
		Linear,         // 선형: 두 키 사이를 직선으로 보간
		CubicBezier     // 3차 에르미트/베지어: In/Out 탄젠트 곡선 보간
	};

	STRUCT(DISPLAY_NAME("STRUCT_CURVE_KEY"))
		struct ENGINE_API CurveKey
	{
		REFLECT_BODY()

			PROPERTY(EDITABLE, DISPLAY_NAME("PROP_TIME"))
			float time = 0.0f;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_VALUE"))
			float value = 0.0f;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_IN_TANGENT"))
			float inTangent = 0.0f;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_OUT_TANGENT"))
			float outTangent = 0.0f;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_INTERP_MODE"))
			ECurveInterpMode interpMode = ECurveInterpMode::CubicBezier;

		CurveKey() = default;
		CurveKey(float t, float v, float inT = 0.0f, float outT = 0.0f, ECurveInterpMode mode = ECurveInterpMode::CubicBezier)
			: time(t), value(v), inTangent(inT), outTangent(outT), interpMode(mode) {
		}
	};

	STRUCT(DISPLAY_NAME("STRUCT_FLOAT_CURVE"))
		struct ENGINE_API FloatCurve
	{
		REFLECT_BODY()

			PROPERTY(EDITABLE, DISPLAY_NAME("PROP_KEYS"))
			std::vector<CurveKey> keys;

		// 주어진 시간(정규화 0~1 또는 절대 시간)에서의 보간 값 평가
		float Evaluate(float time, float defaultValue = 0.0f) const;

		// 키프레임 추가 또는 기존 시간의 키 갱신 (시간 순 자동 정렬)
		int AddKey(float time, float value, float inTangent = 0.0f, float outTangent = 0.0f, ECurveInterpMode interp = ECurveInterpMode::CubicBezier);

		// 특정 인덱스의 키프레임 제거
		bool RemoveKey(std::size_t index);

		// 시간 순서로 키프레임 정렬
		void SortKeys();

		// 키프레임 전체 삭제
		void Clear();

		bool IsEmpty() const { return keys.empty(); }
		std::size_t GetKeyCount() const { return keys.size(); }

		// ── 주요 이펙트 연출 프리셋 팩토리 ──
		static FloatCurve CreateConstant(float value);
		static FloatCurve CreateLinear(float startValue = 0.0f, float endValue = 1.0f);
		static FloatCurve CreateEaseIn(float startValue = 0.0f, float endValue = 1.0f);
		static FloatCurve CreateEaseOut(float startValue = 0.0f, float endValue = 1.0f);
		static FloatCurve CreateEaseInOut(float startValue = 0.0f, float endValue = 1.0f);
		static FloatCurve CreateSpike(float peakValue = 1.0f, float peakTime = 0.1f);
	};
}