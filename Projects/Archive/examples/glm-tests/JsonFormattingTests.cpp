#include "TestSupport.h"

#include <archive/JsonArchive.h>

#include <iostream>
#include <string>

namespace archive_glm_tests
{
    int CheckJsonFormatting()
    {
        const std::string formatInput =
            R"({"Four":[1,2,3,4],"Five":[1,2,3,4,5],"Mixed":[1,2,3,4,"tail"],)"
            R"("Label":"문자열 [1,2,3,4,5], \"quoted\" \\ path","Embedded":"A\u0000한글",)"
            R"("Nested":{"Five":[-1,-2,-3,-4,-5]},"Items":[{"Name":"첫 번째"},{"Name":"두 번째"}]})";

        archive::JsonArchive formatWriter;
        std::string formattedJson;

        if (!formatWriter.Parse(formatInput) || !formatWriter.ToJson(formattedJson))
        {
            return Fail("JSON formatting failed.");
        }

        const std::string expectedFiveLayout =
            "\"Five\": [\n"
            "        1, 2, 3, 4,\n"
            "        5\n"
            "    ]";

        if (formattedJson.find("\"Four\": [1, 2, 3, 4]") == std::string::npos ||
            formattedJson.find(expectedFiveLayout) == std::string::npos ||
            formattedJson.find("\"Mixed\": [1, 2, 3, 4, \"tail\"]") == std::string::npos)
        {
            return Fail("Numeric array layout checks failed.");
        }

        if (formattedJson.find('\0') != std::string::npos ||
            formattedJson.find(R"(A\u0000한글)") == std::string::npos)
        {
            return Fail("Formatting damaged control-character escaping.");
        }

        archive::JsonArchive formatReader;
        std::string reformattedJson;
        std::string originalLabel;
        std::string restoredLabel;
        std::string originalEmbedded;
        std::string restoredEmbedded;

        if (!formatReader.Parse(formattedJson) ||
            !formatReader.ToJson(reformattedJson) ||
            !formatWriter.Read("Label", originalLabel) ||
            !formatReader.Read("Label", restoredLabel) ||
            !formatWriter.Read("Embedded", originalEmbedded) ||
            !formatReader.Read("Embedded", restoredEmbedded))
        {
            return Fail("Formatted JSON reading failed.");
        }

        if (reformattedJson != formattedJson ||
            restoredLabel != originalLabel ||
            restoredEmbedded != originalEmbedded)
        {
            return Fail("Formatting changed the JSON values.");
        }

        std::cout << "JSON layout checks passed.\n";
        return 0;
    }
}
