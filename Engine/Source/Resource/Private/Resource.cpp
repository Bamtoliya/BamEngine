#pragma once

#include "Resource.h"
#include "SerializationHelper.h"

void Resource::Serialize(Archive& ar)
{
    tagResourceBinaryHeader header;
    header.ResourceType = m_ResourceType;
    header.Version = m_Version;

    if (ar.PushScope("AssetHeader"))
    {
        SerializationHelper::SerializeStaticType(ar, header);
        ar.PopScope();
    }

    SerializationHelper::SerializeReflectionProperties(ar, &GetTypeInfo(), this);
}

void Resource::Deserialize(Archive& ar)
{
    const wstring fallbackKey = m_Key;
    const wstring fallbackPath = m_Path;
    const EResourceType expectedType = m_ResourceType;

    tagResourceBinaryHeader header;

    if (!ar.PushScope("AssetHeader"))
    {
        fmt::print(stderr, "Invalid asset file: Missing AssetHeader.\n");
        return;
    }

    SerializationHelper::SerializeStaticType(ar, header);
    ar.PopScope();

    if (header.MagicNumber != ENGINE_ASSET_MAGIC)
    {
        fmt::print(stderr, "Invalid asset file: Magic number mismatch.\n");
        return;
    }

    if (expectedType != EResourceType::Unknown &&
        header.ResourceType != EResourceType::Unknown &&
        header.ResourceType != expectedType)
    {
        fmt::print(stderr, "Invalid asset file: Resource type mismatch.\n");
        return;
    }

    m_ResourceType = header.ResourceType;
    m_Version = header.Version;

    SerializationHelper::SerializeReflectionProperties(ar, &GetTypeInfo(), this);

    if (m_Path.empty())
    {
        m_Path = fallbackPath;
    }

    if (m_Key.empty())
    {
        m_Key = fallbackKey.empty() ? m_Path : fallbackKey;
    }

    m_Path = NormalizePath(m_Path);
    m_Key = NormalizePath(m_Key.empty() ? m_Path : m_Key);
}