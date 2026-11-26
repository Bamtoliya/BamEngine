#pragma once

#include "ReflectionRegistry.h"
#include <archive/JsonArchive.h>
#include <reflection_archive/ReflectionArchiveAdapter.h>

#include <filesystem>
#include <string_view>
#include <system_error>

BEGIN(Engine)

// ReflectionRegistry + archive::JsonArchive 기반 범용 프리셋 파일 저장/복원 헬퍼.
// PIE 실행 중에도 머티리얼/컴포넌트/설정 프리셋을 즉시 JSON 파일로 저장하거나 복원할 수 있다.
template<typename T>
[[nodiscard]] inline EResult SavePresetToFile(const T& preset, const std::filesystem::path& filePath,
    std::string_view rootKey = "Preset")
{
    if (filePath.empty() || !ReflectionRegistry::IsCreated())
        return EResult::InvalidArgument;

    const auto& refl = ReflectionRegistry::Get();
    const auto& registry = refl.GetRegistry();
    const auto* type = registry.FindType<T>();
    if (!type)
        return EResult::Fail;

    archive::JsonArchive writer;
    const auto writeResult = archive_reflection::WriteObject(
        writer, rootKey, *type, reflection::ObjectView::From(preset), &registry, &refl.GetPropertyCodecs());
    if (!writeResult)
        return EResult::Fail;

    if (filePath.has_parent_path())
    {
        std::error_code dirError;
        std::filesystem::create_directories(filePath.parent_path(), dirError);
        if (dirError)
            return EResult::Fail;
    }

    const auto saveResult = writer.SaveToFile(filePath);
    return saveResult ? EResult::Success : EResult::Fail;
}

template<typename T>
[[nodiscard]] inline EResult LoadPresetFromFile(T& outPreset, const std::filesystem::path& filePath,
    std::string_view rootKey = "Preset")
{
    if (filePath.empty() || !ReflectionRegistry::IsCreated())
        return EResult::InvalidArgument;

    std::error_code existsError;
    if (!std::filesystem::exists(filePath, existsError) || existsError)
        return EResult::FileNotFound;

    const auto& refl = ReflectionRegistry::Get();
    const auto& registry = refl.GetRegistry();
    const auto* type = registry.FindType<T>();
    if (!type)
        return EResult::Fail;

    archive::JsonArchive reader;
    const auto loadResult = reader.LoadFromFile(filePath);
    if (!loadResult)
        return EResult::Fail;

    const auto readResult = archive_reflection::ReadObject(
        reader, rootKey, *type, outPreset, &registry, &refl.GetPropertyCodecs());
    return readResult ? EResult::Success : EResult::Fail;
}

END
