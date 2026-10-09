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

        PROPERTY(ARCHIVE_TRANSIENT)
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
        struct ReadOnlyArrayProfile
    {
        PROPERTY()
            const std::array<float, 2> Values{ 7.0f, 9.0f };
    };
}