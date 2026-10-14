#pragma once

namespace reflection_tests
{
	//Metadata
    int CheckMetadata();

    //Enum
    bool ValidateEnumRegistry();
    bool ValidateGeneratedEnums();

    // Function
    bool ValidateGeneratedFunctions();
    bool ValidateStaticFunctions();
    bool ValidateFreeFunctions();

    // Registry
    bool ValidateRegistry();
    bool ValidateRegistrationDiagnostics();
    bool ValidateAtomicRegistration();

    // Module
    bool ValidateMultipleHeaders();
    bool ValidateModuleDiagnostics();

    // Function diagnostics
    bool ValidateInvocationDiagnostics();

    // Property
    bool ValidatePropertyDiagnostics();
}