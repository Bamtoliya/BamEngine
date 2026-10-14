#include "TestSupport.h"

#include <archive/JsonArchive.h>

#include <iostream>

namespace archive_tests
{
    int CheckUnicodeValues()
    {
        const std::wstring wideName = L"한글 \U0001F600";
        const std::wstring embeddedWide{ L"A\0한글", 4 };
        const std::vector<std::wstring> wideLabels{ L"검", L"방패", L"\U0001F600", L"" };

        archive::JsonArchive storage;
        archive::IArchive& ar = storage;

        if (!ar.Write("Name", wideName) || !ar.Write("Embedded", embeddedWide) ||
            !ar.WriteArray("Labels", wideLabels))
        {
            return Fail("Wide string writing failed.");
        }

        std::wstring restoredWideName;
        std::wstring restoredEmbeddedWide;
        std::vector<std::wstring> restoredWideLabels;
        std::string restoredUtf8Name;

        if (!ar.Read("Name", restoredWideName) || !ar.Read("Embedded", restoredEmbeddedWide) ||
            !ar.ReadArray("Labels", restoredWideLabels) || !ar.Read("Name", restoredUtf8Name))
        {
            return Fail("Wide string reading failed.");
        }

        if (restoredWideName != wideName || restoredEmbeddedWide != embeddedWide ||
            restoredWideLabels != wideLabels || restoredUtf8Name != std::string{ "한글 \U0001F600" })
        {
            return Fail("Restored wide string values differ.");
        }

        std::string beforeFailure;

        if (!storage.ToJson(beforeFailure))
        {
            return Fail("Cannot capture the wide string document.");
        }

        const std::wstring invalidWide(1, static_cast<wchar_t>(0xD800));
        const auto writeFailure = ar.Write("Name", invalidWide);

        if (writeFailure.Code != archive::ArchiveErrorCode::InvalidValue)
        {
            return Fail("Invalid wide string check failed.");
        }

        std::string afterFailure;

        if (!storage.ToJson(afterFailure) || afterFailure != beforeFailure)
        {
            return Fail("Failed wide string writing changed the document.");
        }

        const std::vector<std::string> invalidUtf8Inputs{
            std::string{ "\xC0\xAF", 2 }, std::string{ "\xE2\x82", 2 },
            std::string{ "\xED\xA0\x80", 3 }, std::string{ "\xF4\x90\x80\x80", 4 }
        };

        archive::JsonArchive invalidReader;
        std::wstring unchangedWide = L"보존";

        for (const auto& invalidUtf8 : invalidUtf8Inputs)
        {
            if (!invalidReader.Write("Text", invalidUtf8))
            {
                return Fail("Cannot prepare the invalid UTF-8 input.");
            }

            const auto result = invalidReader.Read("Text", unchangedWide);

            if (result.Code != archive::ArchiveErrorCode::InvalidValue || unchangedWide != L"보존")
            {
                return Fail("Failed UTF-8 conversion changed the destination.");
            }
        }

        std::cout << "JSON wide string checks passed.\n";
        std::cout << "JSON wide string failure checks passed.\n";
        return 0;
    }
}