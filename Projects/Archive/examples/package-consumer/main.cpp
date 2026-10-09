#include <archive/JsonArchive.h>

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

int main()
{
    const std::wstring title = L"설치 패키지 \U0001F600";
    const std::vector<std::int32_t> values{ 1, -2, 300 };
    archive::JsonArchive writer;

    if (!writer.Write("Title", title) || !writer.WriteArray("Values", values))
    {
        std::cerr << "Installed archive writing failed.\n";
        return 1;
    }

    std::string document;
    archive::JsonArchive reader;
    std::wstring restoredTitle = L"Before";
    std::vector<std::int32_t> restoredValues{ 9 };

    if (!writer.ToJson(document) || !reader.Parse(document) ||
        !reader.Read("Title", restoredTitle) || !reader.ReadArray("Values", restoredValues) ||
        restoredTitle != title || restoredValues != values)
    {
        std::cerr << "Installed archive round-trip failed.\n";
        return 1;
    }

    std::cout << "Installed Archive package checks passed.\n";
    return 0;
}