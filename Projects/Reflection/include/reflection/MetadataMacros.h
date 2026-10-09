#pragma once

#define REFLECTION_METADATA_DETAIL_STRINGIZE_IMPL(...) #__VA_ARGS__
#define REFLECTION_METADATA_DETAIL_STRINGIZE(...) REFLECTION_METADATA_DETAIL_STRINGIZE_IMPL(__VA_ARGS__)

// 표시 정보
#define DISPLAY_NAME(value) META("DisplayName", value)
#define CATEGORY(value) META("Category", value)
#define TOOLTIP(value) META("Tooltip", value)
#define UNITS(value) META("Units", value)

// 편집 정책
#define EDITABLE META("Editable", true)
#define READONLY META("ReadOnly", true)
#define VISIBLE(value) META("Visible", value)
#define HIDDEN VISIBLE(false)

// 직렬화 정책
#define SERIALIZE META("Serialize", true)
#define NOSERIALIZE META("NoSerialize", true)
#define TRANSIENT META("Transient", true)
#define ALLOW_MISSING META("Optional", true)

// 값에 대한 부가 정보
#define DEFAULT(value) META("Default", REFLECTION_METADATA_DETAIL_STRINGIZE(value))
#define STEP(value) META("Step", value)
// 숫자 입력의 초기화 값과 스냅 간격입니다.
#define RESET_VALUE(value) META("ResetValue", value)
#define SNAP_STEP(value) META("SnapStep", value)
#define COLOR_PROPERTY META("Color", "")
#define BITMASK META("Bitmask", true)

// RANGE(), RANGE(minimum, maximum), RANGE(minimum, maximum, step)를 지원합니다.
// 기존 소비자용 문자열과 새 소비자용 숫자 메타데이터를 함께 생성합니다.
#define REFLECTION_METADATA_DETAIL_RANGE_VALUES(minimum, maximum, ...) \
    META("RangeMin", minimum), META("RangeMax", maximum) __VA_OPT__(, META("Step", __VA_ARGS__))

#define RANGE(...) \
    META("Range", REFLECTION_METADATA_DETAIL_STRINGIZE(__VA_ARGS__)) \
    __VA_OPT__(, REFLECTION_METADATA_DETAIL_RANGE_VALUES(__VA_ARGS__))

// 조건과 콜백 표현을 문자열 메타데이터로 보관합니다.
#define EDITCONDITION(...) META("EditCondition", REFLECTION_METADATA_DETAIL_STRINGIZE(__VA_ARGS__))
#define ONCHANGED(...) META("OnChanged", REFLECTION_METADATA_DETAIL_STRINGIZE(__VA_ARGS__))