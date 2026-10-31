#include "ConfigManager.h"

namespace
{
    namespace fs = std::filesystem;

    struct ConfigTemporaryDirectory
    {
        fs::path Directory;

        ConfigTemporaryDirectory() = default;
        ConfigTemporaryDirectory(const ConfigTemporaryDirectory&) = delete;
        ConfigTemporaryDirectory& operator=(const ConfigTemporaryDirectory&) = delete;

        ~ConfigTemporaryDirectory()
        {
            if (Directory.empty())
                return;

            std::error_code ignored;
            fs::remove(Directory / "settings.tmp", ignored);
            fs::remove(Directory, ignored);
        }
    };
}

namespace Engine
{
    ConfigSaveResult ConfigManager::SaveDocument(const archive::JsonArchive& document, const fs::path& path)
    {
        if (path.empty() || !path.has_filename() || path.filename() == "." || path.filename() == "..")
            return { { archive::ArchiveErrorCode::InvalidValue, "Path" }, {} };

        std::error_code error;
        const auto target = fs::absolute(path, error);
        if (error)
            return { {}, error };

        fs::create_directories(target.parent_path(), error);
        if (error)
            return { {}, error };

        ConfigTemporaryDirectory temporary;
        const auto stamp = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());

        for (unsigned attempt = 0; attempt < 64; ++attempt)
        {
            auto directory = target.parent_path() / (".bamconfig-save-" + stamp + "-" + std::to_string(attempt));

            if (fs::create_directory(directory, error))
            {
                temporary.Directory = std::move(directory);
                break;
            }

            if (error && error != std::errc::file_exists)
                return { {}, error };
        }

        if (temporary.Directory.empty())
            return { {}, std::make_error_code(std::errc::file_exists) };

        const auto stagedFile = temporary.Directory / "settings.tmp";
        const auto result = document.SaveToFile(stagedFile);
        if (!result)
            return { result, {} };

        fs::rename(stagedFile, target, error);
        if (error)
            return { {}, error };

        return {};
    }
}