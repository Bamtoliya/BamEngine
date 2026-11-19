#pragma once

#include "PlatformPath.h"
#include "ResourceManager.h"
#include <archive/JsonArchive.h>
#include <memory>

namespace Editor
{
    template<class Effect>
    auto MakeEffectDocument()
    {
        const auto release = [](Effect* effect) {
            effect->Free();
            delete static_cast<Engine::Resource*>(effect);
        };
        return std::unique_ptr<Effect, decltype(release)>(Effect::Create(), release);
    }

    template<class Effect>
    bool LoadEffectDocument(Effect& effect, const std::filesystem::path& path)
    {
        const auto resolved = Engine::PlatformPath::ResolveProjectRelative(path);
        return effect.LoadFromFile(resolved.wstring()) || effect.Reload(resolved) == Engine::EResult::Success;
    }

    inline bool IsSameEffectDocument(const std::filesystem::path& lhs, const std::filesystem::path& rhs)
    {
        const auto first = Engine::PlatformPath::ResolveProjectRelative(lhs);
        const auto second = Engine::PlatformPath::ResolveProjectRelative(rhs);
        std::error_code error;
        return Engine::PlatformPath::IsSamePath(first, second) || std::filesystem::equivalent(first, second, error);
    }

    template<class Effect, class Apply>
    Engine::ResourceHandle<Effect> SaveEffectDocumentCopy(Effect& source, const std::filesystem::path& sourcePath,
        const std::filesystem::path& destinationPath, Apply apply)
    {
        auto& manager = Engine::ResourceManager::Get();
        const auto key = manager.ResolveFileKey(destinationPath);
        if (key.empty()) return {};

        const auto existing = manager.FindHandleByFilePath(destinationPath);
        auto* destination = dynamic_cast<Effect*>(manager.GetResource(existing));
        if ((existing.IsValid() && !destination) || destination == &source) return {};

        auto savedSource = MakeEffectDocument<Effect>();
        auto copy = MakeEffectDocument<Effect>();
        if (!savedSource || !copy || !LoadEffectDocument(*savedSource, sourcePath)) return {};

        archive::JsonArchive archive;
        if (!source.WriteArchive(archive) || !copy->ReadArchive(archive)) return {};
        copy->SetKey(key);
        copy->SetPath(key);
        const auto resolved = Engine::PlatformPath::ResolveProjectRelative(destinationPath);
        if (!copy->SaveToFile(resolved.wstring())) return {};

        Engine::ResourceHandle<Effect> handle;
        if (destination)
        {
            apply(*destination, *copy);
            handle = Engine::ResourceHandle<Effect>(existing);
        }
        else
            handle = manager.AddResource<Effect>(key, copy.release());

        if (handle.Get()) apply(source, *savedSource);
        return handle;
    }
}
