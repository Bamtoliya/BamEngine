#include <archive/ArchiveInterface.h>
#include <chrono>
#include <system_error>

namespace archive
{
    namespace
    {
        ArchiveResult ArrayElementResult(std::string_view key, std::size_t index,
            std::string_view valueKey, ArchiveResult result)
        {
            if (!result)
            {
                const auto path = std::string{ key } + "[" + std::to_string(index) + "]";
                if (result.Field == valueKey || (result.Field.starts_with(valueKey) &&
                    result.Field.size() > valueKey.size() &&
                    (result.Field[valueKey.size()] == '.' || result.Field[valueKey.size()] == '[')))
                {
                    result.Field = path + result.Field.substr(valueKey.size());
                }
                else
                {
                    result.Field = path + (result.Field.empty() ? "" : "." + result.Field);
                }
            }
            return result;
        }
    }

    ArchiveResult IArchive::WriteArrayElements(std::string_view key, std::size_t size,
        const ArrayElementCallback& writeElement)
    {
        if (!writeElement)
            return { ArchiveErrorCode::InvalidOperation, std::string{ key } };

        return WriteArrayElementValues(key, size,
            [&](IArchive& temporary, std::string_view valueKey, std::size_t index)
            {
                temporary.m_WriteContext = m_WriteContext;

                auto result = writeElement(temporary, valueKey, index);
                if (result && temporary.GetObjectDepth() != 0)
                    result = { ArchiveErrorCode::InvalidOperation, {} };

                return ArrayElementResult(key, index, valueKey, result);
            });
    }

    ArchiveResult IArchive::ReadArrayElements(std::string_view key, const ArrayElementCallback& readElement) const
    {
        if (!readElement)
        {
            return { ArchiveErrorCode::InvalidOperation, std::string{ key } };
        }
        return ReadArrayElementValues(key,
            [&](IArchive& temporary, std::string_view valueKey, std::size_t index)
            {
                auto result = readElement(temporary, valueKey, index);
                if (result && temporary.GetObjectDepth() != 0)
                {
                    result = { ArchiveErrorCode::InvalidOperation, {} };
                }
                return ArrayElementResult(key, index, valueKey, result);
            });
    }

    ArchiveResult IArchive::Encode(std::string& document) const
    {
        if (GetObjectDepth() != 0)
        {
            return { ArchiveErrorCode::InvalidOperation, {} };
        }

        std::string candidate;
        const auto result = EncodeDocument(candidate);

        if (!result)
        {
            return result;
        }

        document = std::move(candidate);
        return {};
    }

    ArchiveResult IArchive::Decode(std::string_view document)
    {
        if (GetObjectDepth() != 0)
        {
            return { ArchiveErrorCode::InvalidOperation, {} };
        }

        return DecodeDocument(document);
    }

    ArchiveResult IArchive::SaveToFile(const std::filesystem::path& path) const
    {
        std::string document;
        const auto result = Encode(document);
        if (!result)
            return result;

        return SaveEncodedToFile(path, document);
    }

    ArchiveResult IArchive::SaveEncodedToFile(const std::filesystem::path& path, std::string_view document)
    {
        std::error_code ignored;
        return SaveEncodedToFile(path, document, ignored);
    }

    ArchiveResult IArchive::SaveEncodedToFile(
        const std::filesystem::path& path, std::string_view document, std::error_code& fileError)
    {
        namespace fs = std::filesystem;
        fileError.clear();

        if (std::cmp_greater(document.size(), (std::numeric_limits<std::streamsize>::max)()))
            return { ArchiveErrorCode::FileWriteFailed, {} };

        if (path.empty() || !path.has_filename() || path.filename() == "." || path.filename() == "..")
            return { ArchiveErrorCode::FileOpenFailed, {} };

        const auto target = fs::absolute(path, fileError);
        if (fileError)
            return { ArchiveErrorCode::FileOpenFailed, {} };

        struct TemporarySave
        {
            fs::path Directory;
            fs::path File;

            ~TemporarySave()
            {
                std::error_code ignored;
                if (!File.empty())
                    fs::remove(File, ignored);
                if (!Directory.empty())
                    fs::remove(Directory, ignored);
            }
        } temporary;

        const auto stamp = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());

        for (unsigned attempt = 0; attempt < 64; ++attempt)
        {
            auto directory = target.parent_path() / (".archive-save-" + stamp + "-" + std::to_string(attempt));

            if (fs::create_directory(directory, fileError))
            {
                temporary.Directory = std::move(directory);
                temporary.File = temporary.Directory / "document.tmp";
                break;
            }

            if (fileError && fileError != std::errc::file_exists)
                return { ArchiveErrorCode::FileOpenFailed, {} };
        }

        if (temporary.Directory.empty())
        {
            fileError = std::make_error_code(std::errc::file_exists);
            return { ArchiveErrorCode::FileOpenFailed, {} };
        }

        std::ofstream file{ temporary.File, std::ios::binary | std::ios::trunc };
        if (!file)
            return { ArchiveErrorCode::FileOpenFailed, {} };

        if (!document.empty())
            file.write(document.data(), static_cast<std::streamsize>(document.size()));

        file.close();
        if (!file)
            return { ArchiveErrorCode::FileWriteFailed, {} };

        fs::rename(temporary.File, target, fileError);
        if (fileError)
            return { ArchiveErrorCode::FileWriteFailed, {} };

        return {};
    }

    ArchiveResult IArchive::LoadFromFile(const std::filesystem::path& path)
    {
        if (GetObjectDepth() != 0)
        {
            return { ArchiveErrorCode::InvalidOperation, {} };
        }

        std::ifstream file{ path, std::ios::binary };

        if (!file)
        {
            return { ArchiveErrorCode::FileOpenFailed, {} };
        }

        std::string document;
        std::array<char, 4096> buffer{};

        while (file.read(buffer.data(), static_cast<std::streamsize>(buffer.size())))
        {
            document.append(buffer.data(), buffer.size());
        }

        if (file.bad() || !file.eof())
        {
            return { ArchiveErrorCode::FileReadFailed, {} };
        }

        document.append(buffer.data(), static_cast<std::size_t>(file.gcount()));
        return Decode(document);
    }
}
