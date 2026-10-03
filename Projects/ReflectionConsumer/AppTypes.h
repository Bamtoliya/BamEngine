#pragma once

#include <reflection/Annotations.h>
#include "AppMetadata.h"
#include <string>
#include <vector>

namespace consumer
{
    STRUCT(APP_NAME("Application Settings"))
        struct Settings
    {
        PROPERTY(APP_NAME("Speed"), APP_UNIT("m/s"))
            float Speed = 1.0f;

        PROPERTY(APP_NAME("Title"))
            std::string Title = "Default";
    };

    CLASS(APP_NAME("Meter"))
        class Meter
    {
        REFLECT_BODY()

    public:
        FUNCTION()
            void Add(float amount)
        {
            m_value += amount;
        }

        FUNCTION()
            float GetValue() const
        {
            return m_value;
        }

    private:
        PROPERTY(APP_NAME("Measured Value"))
            float m_value = 0.0f;
    };

    ENUM(APP_NAME("Application State"))
        enum class State
    {
        Stopped,
        Running,
        Active = Running
    };

    STRUCT(APP_NAME("Snapshot"))
    struct Snapshot
    {
        PROPERTY(APP_NAME("Configuration"))
        Settings Configuration;

        PROPERTY(APP_NAME("Frozen Configuration"))
        const Settings FrozenConfiguration {};

        PROPERTY(APP_NAME("Current State"))
        State Current = State::Stopped;

        PROPERTY(APP_NAME("Target"))
        Settings* Target = nullptr;
    };

    FUNCTION()
    inline float DoubleValue(float value)
    {
        return value * 2.0f;
    }

    STRUCT(APP_NAME("Collection"))
        struct Collection
    {
        PROPERTY()
        std::vector<float> Samples{ 1.0f, 2.0f };

        PROPERTY()
        std::vector<Settings> Items{ Settings{} };

        PROPERTY()
        const std::vector<int> Frozen{ 3, 4 };

        PROPERTY()
        std::vector<int> Empty;

        PROPERTY()
        std::vector<bool> Flags{ true };

        PROPERTY()
        const std::vector<Settings> FrozenItems{ Settings{} };

        PROPERTY()
        std::vector<State> States{ State::Stopped, State::Running };

        PROPERTY()
        const std::vector<State> FrozenStates{ State::Stopped };
    };

    CLASS(APP_NAME("Private Collection"))
        class PrivateCollection
    {
        REFLECT_BODY()

    public:
        const std::vector<float>& GetValues() const
        {
            return m_values;
        }

        const std::vector<Settings>& GetItems() const
        {
            return m_items;
        }

    private:
        PROPERTY(APP_NAME("Values"))
            std::vector<float> m_values{ 1.0f, 2.0f };

        PROPERTY(APP_NAME("Items"))
            std::vector<Settings> m_items{ Settings{} };
    };
}