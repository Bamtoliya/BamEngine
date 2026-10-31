#include "ConfigManager.h"

namespace Engine
{
    ConfigSaveResult ConfigManager::SaveDocument(
        const archive::JsonArchive& document, const std::filesystem::path& path)
    {
        namespace fs = std::filesystem;

        if (path.empty() || !path.has_filename() || path.filename() == "." || path.filename() == "..")
            return { { archive::ArchiveErrorCode::InvalidValue, "Path" }, {} };

        std::error_code error;
        const auto target = fs::absolute(path, error);
        if (error)
            return { {}, error };

        fs::create_directories(target.parent_path(), error);
        if (error)
            return { {}, error };

        std::string encoded;
        const auto encodedResult = document.Encode(encoded);
        if (!encodedResult)
            return { encodedResult, {} };

        const auto result = archive::IArchive::SaveEncodedToFile(target, encoded, error);
        if (error)
            return { {}, error };

        return { result, {} };
    }
}