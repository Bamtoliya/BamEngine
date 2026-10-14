#include "tests/TestSupport.h"

#include <iostream>

int main()
{
    const int metadataResult = reflection_tests::CheckMetadata();

    if (metadataResult != 0)
    {
        return metadataResult;
    }

    struct Check
    {
        bool (*Run)();
        const char* FailureMessage;
        int ExitCode;
    };

    const Check checks[]
    {
        { reflection_tests::ValidateRegistry, "Registry checks failed.", 9 },
        { reflection_tests::ValidateEnumRegistry, "Enum registry checks failed.", 10 },
        { reflection_tests::ValidateGeneratedEnums, "Generated enum checks failed.", 11 },
        { reflection_tests::ValidateGeneratedFunctions, "Generated function checks failed.", 12 },
        { reflection_tests::ValidateStaticFunctions, "Static invocation checks failed.", 13 },
        { reflection_tests::ValidateFreeFunctions, "Free function checks failed.", 14 },
        { reflection_tests::ValidateMultipleHeaders, "Multiple header checks failed.", 15 },
        { reflection_tests::ValidateInvocationDiagnostics, "Invocation diagnostics checks failed.", 16 },
        { reflection_tests::ValidateRegistrationDiagnostics, "Registration diagnostics checks failed.", 17 },
        { reflection_tests::ValidateModuleDiagnostics, "Module diagnostics checks failed.", 18 },
        { reflection_tests::ValidateAtomicRegistration, "Atomic registration checks failed.", 19 },
        { reflection_tests::ValidatePropertyDiagnostics, "Property access diagnostics checks failed.", 20 }
    };

    for (const auto& check : checks)
    {
        if (!check.Run())
        {
            std::cerr << check.FailureMessage << '\n';
            return check.ExitCode;
        }
    }

    std::cout << "All reflection checks passed.\n";
    return 0;
}