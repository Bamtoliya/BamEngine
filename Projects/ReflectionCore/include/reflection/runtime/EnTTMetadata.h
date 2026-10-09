#pragma once

#include "reflection/core/Metadata.h"

#include <entt/meta/meta.hpp>

namespace legacy_reflection
{
    inline MetadataView GetMetadata(const entt::meta_data& property)
    {
        if (!property)
        {
            return {};
        }

        const MetadataView* metadata = property.custom();

        return metadata ? *metadata : MetadataView{};
    }
}