#include "TestSupport.h"

#include <archive/JsonArchive.h>

#include <iostream>

namespace archive_tests
{
    int CheckJsonFailures()
    {
        archive::JsonArchive reader;

        if (!reader.Parse(R"({"Speed":25})"))
        {
            return Fail("Cannot prepare JSON parsing failure checks.");
        }

        const auto result = reader.Parse(R"({"Speed":1} trailing)");

        if (result.Code != archive::ArchiveErrorCode::InvalidDocument)
        {
            return Fail("Invalid JSON check failed.");
        }

        float speed = 0.0f;

        if (!reader.Read("Speed", speed) || speed != 25.0f)
        {
            return Fail("Failed parsing changed the document.");
        }

        std::cout << "JSON failure checks passed.\n";
        return 0;
    }
    int CheckJsonStringEscaping()
    {
        const std::wstring embeddedWide{ L"A\0한글", 4 };
        archive::JsonArchive writer;

        if (!writer.Write("Embedded", embeddedWide))
        {
            return Fail("Cannot prepare JSON control character escaping.");
        }

        std::string json;

        if (!writer.ToJson(json) || json.find('\0') != std::string::npos ||
            json.find(R"(A\u0000한글)") == std::string::npos)
        {
            return Fail("JSON control character escaping failed.");
        }

        return 0;
    }
}