#pragma once

#include "AssetRef.h"
#include "ResourceManager.h"
#include "Archives.h"

void AssetRefBase::Serialize(Archive& ar)
{
	if (ar.IsWriting())
	{
		wstring tag = m_Ptr ? m_Ptr->GetKey() : L"";
		ar.Process("AssetKey", tag);
	}
	else
	{
		wstring key;
		ar.Process("AssetKey", key);
		if (!key.empty())
		{
			m_Ptr = static_cast<Resource*>(ResourceManager::Get().LoadFile(key));
			if (!m_Ptr)
			{
				ENGINE_LOG_ERROR("Failed to load asset: {}", WStrToStr(key));
			}
			else
			{
				ENGINE_LOG_INFO("Successfully loaded asset: {}", WStrToStr(key));
			}
		}
	}
}