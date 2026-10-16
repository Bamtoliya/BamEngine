#pragma once

#include <reflection/Annotations.h>

#include <cstdint>
#include <array>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace consumer
{
    ENUM()
        enum class Mode : std::int8_t
    {
        Backward = -1,
        Idle = 0,
        Forward = 1
    };

    ENUM()
        enum class Permission : std::uint64_t
    {
        None = 0,
        High = std::uint64_t{ 1 } << 63,
        All = (std::numeric_limits<std::uint64_t>::max)()
    };

    CLASS()
        class EntryIdentity
    {
        REFLECT_BODY()
    public:
        std::uint64_t GetId() const { return m_id; }
        void SetId(std::uint64_t value) { m_id = value; }
        bool operator==(const EntryIdentity&) const = default;
    private:
        PROPERTY(META("Optional", true))
        std::uint64_t m_id = 11;
    };

    STRUCT()
        struct Entry : EntryIdentity
    {
        Entry() = default;
        Entry(std::wstring name, float speed, std::int32_t retries)
            : Name(std::move(name)), Speed(speed), Retries(retries) {}

        PROPERTY()
        std::wstring Name = L"Entry";

        PROPERTY()
        float Speed = 1.0f;

        PROPERTY(META("Optional", true))
        std::int32_t Retries = 3;

        bool operator==(const Entry&) const = default;
    };

    STRUCT()
        struct SettingsBase
    {
        PROPERTY(META("Optional", true))
        std::int32_t SchemaVersion = 3;
        bool operator==(const SettingsBase&) const = default;
    };

    CLASS()
        class Settings : public SettingsBase
    {
        REFLECT_BODY()

    public:
        PROPERTY()
        bool Enabled = true;

        PROPERTY()
        std::vector<Entry> Entries{ Entry{} };

        PROPERTY()
        std::map<std::int64_t, std::wstring> Labels{ { 1, L"Default" } };

        PROPERTY()
        std::unordered_map<std::wstring, std::uint64_t> Counts{ { L"Default", 1 } };

        PROPERTY()
        std::map<std::int32_t, Entry> EntriesById{ { 1, Entry{} } };

        PROPERTY()
        std::unordered_map<std::wstring, Entry> NamedEntries{ { L"Default", Entry{} } };

        PROPERTY()
        std::vector<Mode> Modes{ Mode::Idle };

        PROPERTY()
        std::array<Mode, 2> FixedModes{ Mode::Idle, Mode::Forward };

        PROPERTY()
        std::set<Mode> ModeSet{ Mode::Idle };

        PROPERTY()
        std::unordered_set<Permission> Flags{ Permission::High };

        PROPERTY()
        std::map<Mode, Permission> PermissionsByMode{ { Mode::Idle, Permission::None } };

        PROPERTY()
        std::unordered_map<Permission, Mode> ModesByPermission{ { Permission::None, Mode::Idle } };

        PROPERTY()
        std::map<Mode, Entry> EntriesByMode{ { Mode::Idle, Entry{} } };

        PROPERTY()
        std::array<Entry, 2> FixedEntries{};

        PROPERTY()
        std::array<Entry, 0> EmptyEntries{};

        PROPERTY()
        std::vector<std::vector<std::int32_t>> Matrix{ { 1, 2 }, {} };

        PROPERTY()
        std::map<std::wstring, std::vector<Entry>> EntryGroups{ { L"Default", { Entry{} } } };

        PROPERTY()
        std::array<std::vector<Mode>, 2> ModeGroups{};

        std::int32_t GetRevision() const { return m_revision; }
        void SetRevision(std::int32_t value) { m_revision = value; }

        bool operator==(const Settings&) const = default;

    private:
        PROPERTY()
        std::int32_t m_revision = 7;
    };
}
