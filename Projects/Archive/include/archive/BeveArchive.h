#pragma once

#include <archive/GlazeArchiveBase.h>

namespace archive
{
    class BeveArchive final : public GlazeArchiveBase
    {
    public:
        BeveArchive() = default;

        [[nodiscard]] ArchiveResult Parse(std::string_view bytes);
        [[nodiscard]] ArchiveResult ToBeve(std::string& bytes) const;

    protected:
        ArchiveResult EncodeDocument(std::string& document) const override;
        ArchiveResult DecodeDocument(std::string_view document) override;
    };
}