#pragma once

#include <reflection/Annotations.h>
#include "AppMetadata.h"
#include <string>

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
        Running
    };

    FUNCTION()
        inline float DoubleValue(float value)
    {
        return value * 2.0f;
    }
}