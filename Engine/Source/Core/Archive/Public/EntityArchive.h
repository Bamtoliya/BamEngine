#pragma once

#include "Scene.h"
#include "Reflection/CoreComponentReflection.h"

#include <reflection_archive/ReflectionArchiveAdapter.h>

#include <string>
#include <string_view>

namespace Engine
{
    [[nodiscard]] inline archive::ArchiveResult WriteEntityComponents(
        archive::GlazeArchiveBase& ar, std::string_view key, const Scene& scene, const Entity& entity,
        const archive_reflection::PropertyCodecs* codecs = nullptr)
    {
        const auto* registry = GetCoreComponentReflectionRegistry();

        if (registry == nullptr)
        {
            return { archive::ArchiveErrorCode::InvalidOperation, std::string{ key } };
        }

        return ar.WriteObject(key, entity,
            [&scene, registry, codecs, key](archive::GlazeArchiveBase& temporary, const Entity& source)
            -> archive::ArchiveResult
            {
                archive::ArchiveResult result;

                const auto visited = scene.ForEachReflectedComponent(source,
                    [&](const reflection::TypeInfo& type, const reflection::ObjectView& object)
                    {
                        bool enabled = true;
                        result = archive_reflection::IsSerializationEnabled(type.Metadata, type.QualifiedName, enabled);

                        if (!result)
                        {
                            return false;
                        }

                        if (!enabled)
                        {
                            return true;
                        }

                        result = archive_reflection::WriteObject(
                            temporary, type.QualifiedName, type, object, registry, codecs);

                        if (!result && result.Field != type.QualifiedName)
                        {
                            const auto suffix = result.Field.empty() ? std::string{} : "." + result.Field;
                            result.Field = type.QualifiedName + suffix;
                        }

                        return static_cast<bool>(result);
                    });

                if (!result)
                {
                    return result;
                }

                if (visited != ComponentVisitResult::Completed)
                {
                    return { archive::ArchiveErrorCode::InvalidOperation, std::string{ key } };
                }

                return {};
            });
    }
}