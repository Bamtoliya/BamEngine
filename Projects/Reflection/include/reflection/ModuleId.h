// Projects/Reflection/include/reflection/ModuleId.h
#pragma once
#include <cstdint>

namespace reflection
{
    using ModuleId = std::uint32_t;
    inline constexpr ModuleId DefaultModuleId = 0; // 엔진 코어 또는 미지정
}