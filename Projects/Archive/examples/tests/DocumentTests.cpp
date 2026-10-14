#include "TestSupport.h"

#include <archive/BeveArchive.h>
#include <archive/JsonArchive.h>

#include <cstdint>
#include <string>

namespace archive_tests
{
    namespace
    {
        int CheckDocumentOperations(archive::IArchive& writer, archive::IArchive& reader)
        {
            const std::string sourceText{ "A\0B", 3 };

            if (!writer.Write("Count", std::int32_t{ 25 }) || !writer.Write("Text", sourceText))
            {
                return Fail("Common document writing failed.");
            }

            std::string document;

            if (!writer.Encode(document) || !reader.Decode(document))
            {
                return Fail("Common document encoding or decoding failed.");
            }

            std::int32_t count{};
            std::string text;

            if (!reader.Read("Count", count) || !reader.Read("Text", text) ||
                count != 25 || text != sourceText)
            {
                return Fail("Common document round-trip values differ.");
            }

            std::string before;

            if (!reader.Encode(before))
            {
                return Fail("Cannot capture the common document.");
            }

            const auto decodeFailure = reader.Decode({});
            std::string after;

            if (decodeFailure.Code != archive::ArchiveErrorCode::InvalidDocument || !reader.Encode(after) || after != before)
            {
                return Fail("Failed common decoding changed the document or returned the wrong error.");
            }

            if (!writer.BeginWriteObject("Nested"))
            {
                return Fail("Cannot open the common document object.");
            }

            std::string unchanged = "keep";
            const auto encodeFailure = writer.Encode(unchanged);
            const auto openDecodeFailure = writer.Decode(document);
            const auto depth = writer.GetObjectDepth();
            const auto endResult = writer.EndObject();

            if (encodeFailure.Code != archive::ArchiveErrorCode::InvalidOperation ||
                openDecodeFailure.Code != archive::ArchiveErrorCode::InvalidOperation ||
                unchanged != "keep" || depth != 1 || !endResult)
            {
                return Fail("Common document operations changed an open object.");
            }

            return 0;
        }
    }

    int CheckCommonDocuments()
    {
        archive::JsonArchive jsonWriter;
        archive::JsonArchive jsonReader;

        const auto jsonResult = CheckDocumentOperations(jsonWriter, jsonReader);

        if (jsonResult != 0)
        {
            return jsonResult;
        }

        archive::BeveArchive beveWriter;
        archive::BeveArchive beveReader;
        return CheckDocumentOperations(beveWriter, beveReader);
    }
}