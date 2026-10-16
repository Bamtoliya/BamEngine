#pragma once

#include <iostream>

namespace archive_tests
{
    inline int Fail(const char* message)
    {
        std::cerr << message << '\n';
        return 1;    
    }

    //Scalar
    int CheckCommonScalarReads();
    int CheckScalarRoundTrip();
    int CheckEnumValues();
    int CheckBitFlags();

    // Array
    int CheckDynamicArrays();
    int CheckFixedArrays();
    int CheckArrayElements();

    //Map
    int CheckCommonScalarMaps();
    int CheckCommonObjectMaps();

    //Set
    int CheckCommonSets();
    int CheckTagSets();

    // Unicode
    int CheckUnicodeValues();

    //Object
    int CheckObjectNavigation();
    int CheckObjectArrays();
    int CheckSingleObjects();
    int CheckObjectCommit();

    //ScalarFile
    int CheckScalarFile();
    int CheckEnumFile();
    int CheckBitFlagFile();

	//ArrayFile
    int CheckDynamicArrayFile();
    int CheckFixedArrayFile();
	//SetFile
    int CheckTagSetFile();

	//UnicodeFile
    int CheckUnicodeFile();

    //ObjectFile
    int CheckNestedObjectFile();
    int CheckObjectArrayFile();
    int CheckSingleObjectFile();

    //Json
    int CheckJsonFailures();
    int CheckJsonStringEscaping();

    //Beve
    int CheckBeveArchive();

    int CheckCommonDocuments();
}
