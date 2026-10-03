#pragma once

#include <reflection/Annotations.h>

namespace diagnostic
{
    STRUCT()
        struct Probe
    {
        FUNCTION()
            void Ready()
        {
        }

        FUNCTION()
            void Reference(float& value)
        {
            value += 1.0f;
        }

        FUNCTION()
            void Safe() noexcept
        {
        }

        FUNCTION()
            float& ReferenceResult()
        {
            return m_value;
        }

    private:
        FUNCTION()
            void Hidden()
        {
            m_value += 1.0f;
        }

        float m_value = 0.0f;
    };

    STRUCT()
        struct PropertyProbe
    {
        PROPERTY()
            float Value = 2.0f;

        PROPERTY()
            const int Fixed = 7;

        PROPERTY()
            unsigned int Bits : 3 = 0;

        PROPERTY()
            float& Alias = Value;

        PROPERTY()
            volatile int Signal = 0;

        PROPERTY()
            int Samples[2] = { 3, 4 };
    };
}