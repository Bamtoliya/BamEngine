#pragma once

#include <reflection/Annotations.h>

#include <array>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#define PACKAGE_NAME(value) META("DisplayName", value)

STRUCT(PACKAGE_NAME("Package Settings"))
struct PackageSettings
{
    PROPERTY(PACKAGE_NAME("Speed"))
        float Speed = 1.0f;
};

CLASS()
class PackageContainers
{
    REFLECT_BODY()

public:
    PROPERTY()
        std::array<float, 2> Samples{ 10.0f, 20.0f };

    PROPERTY()
        std::set<std::int32_t> Sorted{ 1, 2 };

    PROPERTY()
        std::unordered_set<std::wstring> Tags{ L"한글" };

    PROPERTY()
        std::unordered_map<std::wstring, std::uint64_t> Counts{ { L"플레이어", 3 } };

    PROPERTY()
        std::vector<PackageSettings> Items{ PackageSettings{} };

    const std::map<std::int32_t, std::wstring>& GetNumbers() const
    {
        return m_numbers;
    }

private:
    PROPERTY()
        std::map<std::int32_t, std::wstring> m_numbers{ { 1, L"첫 번째" } };
};