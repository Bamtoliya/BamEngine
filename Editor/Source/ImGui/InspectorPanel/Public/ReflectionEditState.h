#pragma once

#include <array>
#include <string>
#include <unordered_map>

namespace Editor
{
    enum class TransformEditMode
    {
        Translation,
        Rotation,
        Scale
    };

    struct ReflectionVectorEditSettings
    {
        std::array<bool, 3> LockedAxes{};
        bool SnapEnabled = false;

        // 0이면 프로퍼티의 SnapStep 메타데이터를 사용합니다.
        float SnapStep = 0.0f;
    };

    struct ReflectionObjectEditSettings
    {
        std::unordered_map<std::string, ReflectionVectorEditSettings> Vectors;
    };

    // 에디터에서만 사용하는 EnTT 컴포넌트입니다.
    struct ReflectionEntityEditState
    {
        std::unordered_map<std::string, ReflectionObjectEditSettings> Components;
    };
}