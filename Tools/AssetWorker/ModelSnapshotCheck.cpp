#include "ModelConverter.h"

#include <filesystem>
#include <iostream>

int wmain(int argc, wchar_t* argv[])
{
    if (argc != 2)
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

    return 0;
}
