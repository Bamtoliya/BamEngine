#pragma once

#include <reflection_entt/Metadata.h>

#include <clang/Basic/SourceLocation.h>
#include <clang/Lex/Token.h>

#include <span>
#include <string>
#include <vector>

namespace clang
{
    class ASTContext;
}

namespace reflection_entt_codegen
{
    struct MetadataParseResult
    {
        std::vector<reflection_entt::MetadataEntry> Entries;
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