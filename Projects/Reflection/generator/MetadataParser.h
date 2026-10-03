#pragma once

#include <reflection/Metadata.h>

#include <clang/Basic/SourceLocation.h>
#include <clang/Lex/Token.h>

#include <span>
#include <string>
#include <vector>

namespace clang
{
    class ASTContext;
}

namespace reflection_codegen
{
    struct MetadataParseResult
    {
        std::vector<reflection::MetadataEntry> Entries;
        std::string Error;
        clang::SourceLocation ErrorLocation;

        bool Succeeded() const
        {
            return Error.empty();
        }
    };

    MetadataParseResult ParseMetadata(
        std::span<const clang::Token> tokens,
        clang::ASTContext& context
    );
}