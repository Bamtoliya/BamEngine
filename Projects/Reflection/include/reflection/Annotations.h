#pragma once

#define REFLECTION_DETAIL_STRINGIZE_IMPL(...) #__VA_ARGS__

#define REFLECTION_DETAIL_STRINGIZE(...) \
    REFLECTION_DETAIL_STRINGIZE_IMPL(__VA_ARGS__)

// 일반 C++ 컴파일에서는 제거됩니다.
// 생성기는 전처리 과정에서 이 매크로의 호출 위치와 인자를 수집합니다.
#define CLASS(...)
#define STRUCT(...)
#define ENUM(...)
#define FUNCTION(...)
#define PROPERTY(...)

namespace reflection::detail
{
    template<typename T>
    struct TypeRegistration;
}

#define REFLECT_BODY() \
    template<typename T> \
    friend struct ::reflection::detail::TypeRegistration;