#include "tests/TestSupport.h"

#include <iostream>

int main()
{
    using Check = int (*)();

    const Check checks[]
    {
        archive_tests::CheckCommonDocuments,
        archive_tests::CheckBeveArchive,
        archive_tests::CheckCommonObjectMaps,
        archive_tests::CheckCommonScalarMaps,
        archive_tests::CheckCommonSets,
        archive_tests::CheckCommonScalarReads,
        archive_tests::CheckScalarRoundTrip,
        archive_tests::CheckJsonFailures,
        archive_tests::CheckScalarFile,

        archive_tests::CheckObjectNavigation,
        archive_tests::CheckNestedObjectFile,
        archive_tests::CheckDynamicArrays,
        archive_tests::CheckDynamicArrayFile,
        archive_tests::CheckObjectArrays,
        archive_tests::CheckObjectArrayFile,
        archive_tests::CheckSingleObjects,
        archive_tests::CheckSingleObjectFile,

        archive_tests::CheckEnumValues,
        archive_tests::CheckEnumFile,
        archive_tests::CheckBitFlags,
        archive_tests::CheckBitFlagFile,
        archive_tests::CheckUnicodeValues,
        archive_tests::CheckJsonStringEscaping,
        archive_tests::CheckUnicodeFile,
        archive_tests::CheckTagSets,
        archive_tests::CheckTagSetFile,
        archive_tests::CheckFixedArrays,
        archive_tests::CheckFixedArrayFile
    };

    for (const Check check : checks)
    {
        if (check() != 0)
        {
            return 1;
        }
    }

    std::cout << "All archive checks passed.\n";
    return 0;
}