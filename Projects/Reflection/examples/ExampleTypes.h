#pragma once

#include <reflection/Annotations.h>

#include "ExampleMetadata.h"

namespace example
{
    STRUCT(DISPLAY_NAME("Movement Configuration"))
    struct MovementSettings
    {
        PROPERTY(
            DISPLAY_NAME("Speed"),
            CATEGORY("Movement"),
            EDITABLE,
            RANGE(0.0, 100.0),
            HELP_TEXT("Initial help")
        )
        float Speed = 10.0f;
    };

    CLASS(DISPLAY_NAME("Player"))
    class Player
    {
        REFLECT_BODY()
    public:
        FUNCTION(CATEGORY("Movement"))
        void Move(float distance)
        {
            m_position += distance;
        }

        FUNCTION(CATEGORY("Movement"))
        void Move(int distance)
        {
            Move(static_cast<float>(distance));
        }

        FUNCTION()
        float GetPosition() const
        {
            return m_position;
        }

        FUNCTION()
        void CopyPosition(float* output) const
        {
            if (output != nullptr)
            {
                *output = m_position;
            }
        }

        FUNCTION()
        float Advance(float distance)
        {
            Move(distance);
            return m_position;
        }

        FUNCTION(CATEGORY("Utility"))
            static float Scale(float value)
        {
            return value * 2.0f;
        }

        FUNCTION(CATEGORY("Utility"))
            static int Scale(int value)
        {
            return value * 3;
        }

        FUNCTION()
            static void Store(float* output, float value)
        {
            if (output != nullptr)
            {
                *output = value;
            }
        }

    private:
        PROPERTY(DISPLAY_NAME("Position"))
        float m_position = 0.0f;
    };

    ENUM(DISPLAY_NAME("Movement Mode"))
        enum class MovementMode
    {
        Walk,
        Run
    };

    ENUM()
    enum class SignedBoundary : long long
    {
        Minimum = (-9223372036854775807LL - 1LL),
        Negative = -1,
        Alias = Negative
    };

    ENUM()
    enum class UnsignedBoundary : unsigned long long
    {
        Maximum = 18446744073709551615ULL
    };

    FUNCTION(CATEGORY("Math"))
        inline float Add(float left, float right)
    {
        return left + right;
    }

    FUNCTION(CATEGORY("Math"))
        inline int Add(int left, int right)
    {
        return left + right;
    }

    FUNCTION()
        inline void WriteValue(float* output, float value)
    {
        if (output != nullptr)
        {
            *output = value;
        }
    }

    namespace other
    {
        FUNCTION()
            inline float Add(float left, float right)
        {
            return left + right + 10.0f;
        }
    }
}