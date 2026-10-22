#pragma once

#include "ReflectionEditState.h"
#include <glm/glm.hpp>
#include <functional>
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
        static bool IsVisible(const reflection::TypeInfo& type);
        static bool DrawObject(const reflection::TypeInfo& type, const reflection::ObjectView& object, const reflection::Registry* registry = nullptr, ReflectionObjectEditSettings* editSettings = nullptr, std::function<void()> onHeaderContextMenu = nullptr);
        static bool DrawPropertyTable(const reflection::TypeInfo& type, const reflection::ObjectView& object, const reflection::Registry* registry = nullptr, ReflectionObjectEditSettings* editSettings = nullptr);
        static bool DrawValue(const reflection::PropertyInfo& property, const reflection::ObjectView& object, const reflection::Registry* registry = nullptr, ReflectionObjectEditSettings* editSettings = nullptr);

        // 호출자가 객체와 프로퍼티를 구분하는 ImGui ID를 설정해야 합니다.
        static bool DrawVector3(const reflection::Metadata& metadata, glm::vec3& value, ReflectionVectorEditSettings* editSettings = nullptr);
        static bool DrawVector4(const reflection::Metadata& metadata, glm::vec4& value, ReflectionVectorEditSettings* editSettings = nullptr);

        static bool DrawCommonBoolProperties(const reflection::TypeInfo& type, std::span<const reflection::ObjectView> objects, const reflection::Registry* registry = nullptr, std::span<const ReflectionObjectEditSettings* const> editSettings = {});
        static bool DrawCommonFloatProperties(const reflection::TypeInfo& type, std::span<const reflection::ObjectView> objects, const reflection::Registry* registry = nullptr, std::span<const ReflectionObjectEditSettings* const> editSettings = {});
    private:
        static bool DrawVectorComponents(const reflection::Metadata& metadata, std::span<float> value, ReflectionVectorEditSettings* editSettings);
    };
}