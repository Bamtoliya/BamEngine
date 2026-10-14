#include "TestSupport.h"
#include "../ExampleMetadata.h"

#include <reflection/Reflection.h>

#include <iostream>
#include <string>
#include <string_view>
#include <variant>

namespace
{
    constexpr std::string_view ExpandedMetadata = REFLECTION_DETAIL_STRINGIZE(DISPLAY_NAME("Speed"), EDITABLE, RANGE(0.0, 100.0));

    static_assert(ExpandedMetadata.find("DisplayName") != std::string_view::npos);
    static_assert(ExpandedMetadata.find("Editable") != std::string_view::npos);
    static_assert(ExpandedMetadata.find("RangeMin") != std::string_view::npos);
    static_assert(ExpandedMetadata.find("RangeMax") != std::string_view::npos);
    static_assert(ExpandedMetadata.find("DISPLAY_NAME(") == std::string_view::npos);
    static_assert(ExpandedMetadata.find("RANGE(") == std::string_view::npos);
}

namespace reflection_tests
{
    int CheckMetadata()
    {
        reflection::Metadata metadata;

        if (!metadata.Add("DisplayName", std::string{ "Speed" }))
        {
            return 1;
        }

        if (!metadata.Add("Editable", true))
        {
            return 2;
        }

        if (!metadata.Add("RangeMin", 0.0))
        {
            return 3;
        }

        // 중복 키는 거부되어야 합니다.
        if (metadata.Add("DisplayName", std::string{ "Other" }))
        {
            return 4;
        }

        const reflection::MetadataValue* value = metadata.Find("DisplayName");

        if (value == nullptr)
        {
            return 5;
        }

        const std::string* displayName = std::get_if<std::string>(value);

        if (displayName == nullptr || *displayName != "Speed")
        {
            return 6;
        }

        // 존재하는 키라도 요청한 값의 자료형이 다르면 실패해야 합니다.
        if (std::get_if<bool>(value) != nullptr)
        {
            return 7;
        }

        if (metadata.Find("Missing") != nullptr)
        {
            return 8;
        }

        std::cout << "DisplayName: " << *displayName << '\n';
        std::cout << "Metadata checks passed.\n";
        std::cout << "Expanded metadata: " << ExpandedMetadata << '\n';
        return 0;
    }
}