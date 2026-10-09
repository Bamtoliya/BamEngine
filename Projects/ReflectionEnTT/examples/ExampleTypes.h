#pragma once

#include <reflection_entt/Annotations.h>
#include "ExampleMetadata.h"
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace example
{
    STRUCT(NAME("Movement Settings"))
    struct MovementSettings
    {
        PROPERTY(NAME("Speed"), CATEGORY("Movement"), EDITABLE, RANGE(0.0, 100.0), TOOLTIP("Speed, \"fast\""))
        float Speed = 1.0f;

        PROPERTY(NAME("Title"), EDITABLE)
        std::wstring Title = L"Entity";

        PROPERTY(READONLY)
        const int Revision = 2;

        PROPERTY(EDITABLE)
        std::vector<float> Samples{1.0f, 2.0f};

        PROPERTY(EDITABLE)
        std::map<std::string, float> Weights{{"Walk", 1.0f}};

        PROPERTY(EDITABLE)
        std::set<std::string> Tags{"Moving"};
    };

    CLASS(NAME("Base"))
    class Base
    {
        REFLECT_BODY()

    public:
        virtual ~Base() = default;

        PROPERTY(NAME("Root"))
        int Root = 7;
    };

    CLASS(NAME("Player"))
    class Player : public Base
    {
        REFLECT_BODY()

    public:
        FUNCTION(CATEGORY("Movement"))
        void Move(float distance) { m_position += distance; }

        FUNCTION(CATEGORY("Movement"))
        void Move(float distance, float factor) { m_position += distance * factor; }

        FUNCTION()
        float GetPosition() const noexcept { return m_position; }

        FUNCTION()
        std::string Label() const { return m_label; }

        FUNCTION()
        const std::string& LabelRef() const { return m_label; }

        FUNCTION()
        void SetLabel(const std::string& label) { m_label = label; }

        FUNCTION()
        static float Twice(float value) { return value * 2.0f; }

    private:
        PROPERTY(NAME("Position"), EDITABLE)
        float m_position = 0.0f;

        PROPERTY()
        std::string m_label = "Player";

        FUNCTION()
        void Reset() { m_position = 0.0f; }
    };

    ENUM(NAME("Movement Mode"))
    enum class MovementMode : std::uint64_t
    {
        Walk = 0,
        Run = 1,
        Active = Run,
        Maximum = UINT64_MAX
    };

    STRUCT()
    struct Snapshot
    {
        PROPERTY()
        MovementMode Mode = MovementMode::Walk;

        PROPERTY()
        MovementSettings* Target = nullptr;
    };

    FUNCTION(NAME("Double"))
    inline float DoubleValue(float value) { return value * 2.0f; }
}
