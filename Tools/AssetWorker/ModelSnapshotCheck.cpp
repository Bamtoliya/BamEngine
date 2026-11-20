#include "ModelConverter.h"

#include <filesystem>
#include <iostream>
#include <string>

int wmain(int argc, wchar_t* argv[])
{
    if (argc != 2 && argc != 3)
    {
        std::cerr << "Usage: AssetWorkerSnapshotCheck <snapshot-path>\n";
        return 2;
    }

    const auto result = Tools::ModelConversion::ReadModelSnapshot(std::filesystem::path(argv[1]));
    if (!result)
    {
        std::cerr << "Model snapshot could not be consumed: " << result.Message << '\n';
        return 1;
    }

    if (argc == 3)
    {
        const std::wstring mode = argv[2];
        bool different = false;
        bool sawVertex = false;
        bool expectedFirstUv = false;
        bool nonzeroUv = false;
        for (const auto& mesh : result.Model->Meshes)
        {
            for (const auto& vertex : mesh.Vertices)
            {
                sawVertex = true;
                different |= vertex.TexCoord != vertex.TexCoord1;
                nonzeroUv |= vertex.TexCoord[0] != 0.0f || vertex.TexCoord[1] != 0.0f;
                if (vertex.Position[0] == 0.0f && vertex.Position[1] == 0.0f)
                    expectedFirstUv |= vertex.TexCoord[0] == 0.0f && vertex.TexCoord1[0] == 0.25f;
            }
        }
        if (!sawVertex || (mode == L"--expect-fallback" && (different || !nonzeroUv)) ||
            (mode == L"--expect-distinct" && (!different || !expectedFirstUv)) ||
            (mode != L"--expect-fallback" && mode != L"--expect-distinct"))
        {
            std::cerr << "Unexpected UV1 snapshot data.\n";
            return 1;
        }
    }

    return 0;
}
