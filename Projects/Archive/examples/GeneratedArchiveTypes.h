#pragma once

#include <reflection/Annotations.h>

#include <cstdint>
#include <limits>
#include <string>
#include <set>
#include <unordered_set>
#include <map>
#include <unordered_map>
#include <vector>
#include <array>

#define ARCHIVE_TRANSIENT META("Serialize", false)
#define ARCHIVE_OPTIONAL META("Optional", true)
#define ARCHIVE_LABEL(value) META("DisplayName", value)

namespace generated_archive
{
    ENUM()
        enum class State : std::uint8_t
    {
        Idle = 0,
        Active = 1
    };

    ENUM()
        enum class SignedMode : std::int8_t
    {
        Minimum = -128,
        Idle = 0,
        Maximum = 127
    };

    ENUM()
        enum class WideFlags : std::uint64_t
    {
        None = 0,
        High = std::uint64_t{ 1 } << 63,
        All = (std::numeric_limits<std::uint64_t>::max)()
    };

    ENUM()
        enum class BinaryState : bool
    {
        Off = false,
        On = true
    };

    STRUCT()
        struct Settings
    {
        PROPERTY()
            std::wstring Title = L"이동 설정 \U0001F600";

        PROPERTY()
            float Speed = 25.0f;

        PROPERTY(ARCHIVE_TRANSIENT)
            float CachedSpeed = std::numeric_limits<float>::infinity();

        PROPERTY(ARCHIVE_OPTIONAL)
            std::int32_t Retries = 3;

        bool operator==(const Settings&) const = default;
    };

    CLASS()
        class Profile
    {
        REFLECT_BODY()

    public:
        PROPERTY()
            std::wstring Name = L"플레이어";

        PROPERTY()
            Settings Options;

        PROPERTY()
            std::unordered_set<std::wstring> Tags{ L"이동", L"테스트 \U0001F600" };

        PROPERTY()
            State Current = State::Active;

        std::int32_t GetScore() const
        {
            return m_score;
        }

        void SetScore(std::int32_t value)
        {
            m_score = value;
        }

    private:
        PROPERTY()
            std::int32_t m_score = 7;
    };

    STRUCT()
        struct EnumContainerProfile
    {
        PROPERTY()
            std::vector<SignedMode> Modes{ SignedMode::Minimum, SignedMode::Maximum };

        PROPERTY()
            std::array<State, 2> Fixed{ State::Idle, State::Active };

        PROPERTY()
            std::array<State, 0> Empty{};

        PROPERTY()
            std::set<State> Ordered{ State::Idle, State::Active };

        PROPERTY()
            std::unordered_set<WideFlags> Bits{ WideFlags::High, WideFlags::All };

        PROPERTY()
            std::vector<BinaryState> Binary{ BinaryState::Off, BinaryState::On };

        PROPERTY()
            std::map<SignedMode, State> Pairs{ { SignedMode::Minimum, State::Active } };

        PROPERTY()
            std::unordered_map<State, SignedMode> Reverse{ { State::Active, SignedMode::Maximum } };

        PROPERTY()
            std::map<WideFlags, WideFlags> WidePairs{ { WideFlags::All, WideFlags::High } };

        PROPERTY()
            std::map<std::wstring, State> ByName{ { L"상태 \U0001F600", State::Active } };

        PROPERTY()
            std::map<State, std::wstring> Labels{ { State::Active, L"이동" } };

        PROPERTY()
            std::map<State, Settings> Objects{ { State::Active, Settings{} } };
    };

    CLASS(ARCHIVE_LABEL("Container Profile"))
        class ContainerProfile
    {
        REFLECT_BODY()

    public:
        PROPERTY()
            std::map<std::int64_t, std::wstring> Numbers{ { -1, L"음수" }, { 10, L"한글" } };

        PROPERTY()
            std::unordered_map<std::wstring, std::uint64_t> Counts{ { L"플레이어", 3 } };

        PROPERTY()
            std::set<std::int32_t> Sorted{ 1, 2, 3 };

        const std::map<std::int32_t, Settings>& GetSettings() const
        {
            return m_settings;
        }

        const std::unordered_map<std::wstring, Settings>& GetNamedSettings() const
        {
            return m_namedSettings;
        }

    private:
        PROPERTY(ARCHIVE_LABEL("Settings by ID"))
            std::map<std::int32_t, Settings> m_settings{ { 1, Settings{} } };

        PROPERTY()
            std::unordered_map<std::wstring, Settings> m_namedSettings{ { L"플레이어", Settings{} } };
    };

    CLASS()
        class SequenceProfile
    {
        REFLECT_BODY()

    public:
        const std::vector<Settings>& GetSteps() const
        {
            return m_steps;
        }

    private:
        PROPERTY()
            std::vector<Settings> m_steps{ Settings{} };
    };

    CLASS()
        class FixedProfile
    {
        REFLECT_BODY()

    public:
        PROPERTY()
            std::array<bool, 2> Flags{ false, false };

        PROPERTY()
            std::array<State, 2> States{ State::Idle, State::Idle };

        PROPERTY()
            std::array<std::int32_t, 0> Empty{};

        PROPERTY()
            std::array<Settings, 2> Objects{};

        const std::array<float, 3>& GetSamples() const
        {
            return m_samples;
        }

    private:
        PROPERTY()
            std::array<float, 3> m_samples{ 10.0f, 20.0f, 30.0f };
    };

    STRUCT()
        struct ObjectArrayProfile
    {
        PROPERTY()
            std::array<Settings, 2> Values{};

        PROPERTY()
            std::array<Settings, 0> Empty{};
    };

    STRUCT()
        struct NestedContainerProfile
    {
        PROPERTY()
            std::vector<std::vector<std::int32_t>> Matrix{ { 1, 2 }, {}, { 3 } };

        PROPERTY()
            std::array<std::vector<float>, 2> FixedRows{ std::vector<float>{ 1, 2 }, std::vector<float>{} };

        PROPERTY()
            std::map<std::string, std::vector<Settings>> Objects{ { "Steps", { Settings{} } } };

        PROPERTY()
            std::vector<std::array<State, 2>> EnumGroups{ { State::Idle, State::Active } };

        PROPERTY()
            std::vector<std::map<State, std::vector<Settings>>> MapGroups{ { { State::Active, { Settings{} } } } };

        PROPERTY()
            std::vector<std::set<std::string>> Tags{ { "A", "B" }, {} };

        PROPERTY()
            std::vector<std::vector<bool>> Masks{ { true, false }, {} };

        PROPERTY()
            std::vector<std::vector<std::vector<std::int32_t>>> Deep{ { { 1, 2 }, {} }, {} };

        PROPERTY()
            std::array<std::vector<Settings>, 0> Empty{};

        bool operator==(const NestedContainerProfile&) const = default;
    };

    STRUCT()
        struct ReadOnlyArrayProfile
    {
        PROPERTY()
            const std::array<float, 2> Values{ 7.0f, 9.0f };
    };
}
