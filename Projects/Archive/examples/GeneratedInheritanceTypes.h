#pragma once

#include <reflection/Annotations.h>

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace generated_inheritance
{
    CLASS()
        class Identity
    {
        REFLECT_BODY()
    public:
        PROPERTY()
        std::string Label = "Base";

        PROPERTY(META("Optional", true))
        std::int32_t Retries = 3;

        PROPERTY(META("Transient", true))
        std::int32_t Cached = 11;

        PROPERTY()
        std::vector<std::int32_t> Marks{ 1, 2 };

        std::int32_t GetId() const { return m_id; }
        void SetId(std::int32_t value) { m_id = value; }
        bool operator==(const Identity&) const = default;
    private:
        PROPERTY()
        std::int32_t m_id = 7;
    };

    STRUCT()
        struct Coordinates
    {
        PROPERTY()
        float X = 2.0f;
        bool operator==(const Coordinates&) const = default;
    };

    STRUCT()
        struct Derived : Identity, Coordinates
    {
        PROPERTY()
        std::int32_t Health = 100;
        bool operator==(const Derived&) const = default;
    };

    STRUCT()
        struct MultiLevel : Derived
    {
        PROPERTY()
        std::int32_t Level = 1;
        bool operator==(const MultiLevel&) const = default;
    };

    STRUCT()
        struct Left : virtual Identity
    {
        PROPERTY()
        std::int32_t LeftValue = 5;
        bool operator==(const Left&) const = default;
    };

    STRUCT()
        struct Right : virtual Identity
    {
        PROPERTY()
        std::int32_t RightValue = 9;
        bool operator==(const Right&) const = default;
    };

    STRUCT()
        struct Diamond : Left, Right
    {
        PROPERTY()
        bool Enabled = true;
        bool operator==(const Diamond&) const = default;
    };

    CLASS()
        class PrivateDerived : private Identity
    {
        REFLECT_BODY()
    public:
        PROPERTY()
        std::int32_t Secret = 17;
        Identity& GetIdentity() { return *this; }
        const Identity& GetIdentity() const { return *this; }
        bool operator==(const PrivateDerived&) const = default;
    };

    CLASS()
        class ProtectedDerived : protected Identity
    {
        REFLECT_BODY()
    public:
        PROPERTY()
        std::int32_t Secret = 19;
        Identity& GetIdentity() { return *this; }
        const Identity& GetIdentity() const { return *this; }
        bool operator==(const ProtectedDerived&) const = default;
    };

    STRUCT()
        struct Collision : Identity
    {
        PROPERTY()
        std::string Label = "Hidden";
        bool operator==(const Collision&) const = default;
    };

    STRUCT()
        struct NonVirtualLeft : Identity { bool operator==(const NonVirtualLeft&) const = default; };

    STRUCT()
        struct NonVirtualRight : Identity { bool operator==(const NonVirtualRight&) const = default; };

    STRUCT()
        struct Repeated : NonVirtualLeft, NonVirtualRight { bool operator==(const Repeated&) const = default; };

    struct ExternalBase
    {
        std::int32_t ExternalValue = 23;
        bool operator==(const ExternalBase&) const = default;
    };

    STRUCT()
        struct MissingBase : ExternalBase
    {
        PROPERTY()
        std::int32_t Value = 29;
        bool operator==(const MissingBase&) const = default;
    };

    STRUCT()
        struct Containers
    {
        PROPERTY()
        std::vector<Derived> Items{ Derived{} };

        PROPERTY()
        std::array<Diamond, 1> Fixed{};

        PROPERTY()
        std::map<std::string, MultiLevel> ByName{ { "Player", MultiLevel{} } };

        bool operator==(const Containers&) const = default;
    };
}
