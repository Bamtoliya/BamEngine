#include <archive/BeveArchive.h>

#include <glaze/beve.hpp>

#include <utility>

namespace archive
{
    ArchiveResult BeveArchive::Parse(std::string_view bytes)
    {
        return Decode(bytes);
    }

    ArchiveResult BeveArchive::ToBeve(std::string& bytes) const
    {
        return Encode(bytes);
    }

    ArchiveResult BeveArchive::EncodeDocument(std::string& document) const
    {
        if (glz::write_beve(m_Root, document))
        {
            return { ArchiveErrorCode::SerializationFailed, {} };
        }

        return {};
    }

    ArchiveResult BeveArchive::DecodeDocument(std::string_view document)
    {
        Value candidate;
        const auto error = glz::read_beve(candidate, document);

        if (error || error.count != document.size() || !candidate.is_object())
        {
            return { ArchiveErrorCode::InvalidDocument, {} };
        }

        m_Root = std::move(candidate);
        return {};
    }
}