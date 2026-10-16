#include <archive/ArchiveInterface.h>

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
        {
            return { ArchiveErrorCode::InvalidOperation, std::string{ key } };
        }
        return WriteArrayElementValues(key, size,
            [&](IArchive& temporary, std::string_view valueKey, std::size_t index)
            {
                auto result = writeElement(temporary, valueKey, index);
                if (result && temporary.GetObjectDepth() != 0)
                {
                    result = { ArchiveErrorCode::InvalidOperation, {} };
                }
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
        {
            return result;
        }

        if (std::cmp_greater(document.size(), (std::numeric_limits<std::streamsize>::max)()))
        {
            return { ArchiveErrorCode::FileWriteFailed, {} };
        }

        std::ofstream file{ path, std::ios::binary | std::ios::trunc };

        if (!file)
        {
            return { ArchiveErrorCode::FileOpenFailed, {} };
        }

        file.write(document.data(), static_cast<std::streamsize>(document.size()));
        file.close();

        if (!file)
        {
            return { ArchiveErrorCode::FileWriteFailed, {} };
        }

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
