#pragma once

#include "Resource.h"
#include "SerializationHelper.h"

#pragma region Save & Load
void Resource::Serialize(Archive& ar) 
{
	tagResourceBinaryHeader header;
	header.ResourceType = m_ResourceType;
	header.Version = m_Version;
	ar.Process<tagResourceBinaryHeader>("AssetHeader", header);
	SerializationHelper::SerializeReflectionProperties(ar, &GetTypeInfo(), this);
}

void Resource::Deserialize(Archive& ar)
{
	tagResourceBinaryHeader header;
	ar.Process<tagResourceBinaryHeader>("AssetHeader", header);
	m_ResourceType = header.ResourceType;
	m_Version = header.Version;
	if (header.MagicNumber != ENGINE_ASSET_MAGIC)
	{
		fmt::print(stderr, "Invalid asset file: Magic number mismatch.\n");
		return;
	}
	SerializationHelper::SerializeReflectionProperties(ar, &GetTypeInfo(), this);
}

#pragma endregion
