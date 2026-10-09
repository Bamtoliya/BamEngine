#pragma once

#include "ReflectionEditState.h"
#include <glm/glm.hpp>
#include <span>

namespace reflection
{
    class ObjectView;
    class Registry;
    class Metadata;
    struct TypeInfo;
    struct PropertyInfo;
}

namespace Editor
{
    class ReflectionPropertyDrawer
    {
    public:
        static bool DrawObject(const reflection::TypeInfo& type, const reflection::ObjectView& object, const reflection::Registry* registry = nullptr, ReflectionObjectEditSettings* editSettings = nullptr);
        static bool DrawPropertyTable(const reflection::TypeInfo& type, const reflection::ObjectView& object, const reflection::Registry* registry = nullptr, ReflectionObjectEditSettings* editSettings = nullptr);
        static bool DrawValue(const reflection::PropertyInfo& property, const reflection::ObjectView& object, const reflection::Registry* registry = nullptr, ReflectionObjectEditSettings* editSettings = nullptr);

        // 호출자가 객체와 프로퍼티를 구분하는 ImGui ID를 설정해야 합니다.
        static bool DrawVector3(const reflection::Metadata& metadata, glm::vec3& value, ReflectionVectorEditSettings* editSettings = nullptr);
        static bool DrawVector4(const reflection::Metadata& metadata, glm::vec4& value, ReflectionVectorEditSettings* editSettings = nullptr);
    private:
        static bool DrawVectorComponents(const reflection::Metadata& metadata, std::span<float> value, ReflectionVectorEditSettings* editSettings);
    };
}