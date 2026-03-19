#pragma once

#include "AssetRef.h"
#include "ResourceManager.h"
#include "Archives.h"

void AssetRefBase::Serialize(Archive& ar)
{
	if (ar.IsWriting())
	{
		wstring tag = m_Ptr ? m_Ptr->GetTag() : L"";
		ar.Process("AssetTag", tag);
		wstring path = m_Ptr ? m_Ptr->GetPath() : L"";
		ar.Process("AssetPath", path);
	}
	else
	{
		wstring tag;
		ar.Process("AssetTag", tag);
		wstring path;
		ar.Process("AssetPath", path);
		if (!tag.empty() && !path.empty())
		{
			m_Ptr = static_cast<Resource*>(ResourceManager::Get().LoadFile(path));
			if (!m_Ptr)
			{
				ENGINE_LOG_ERROR("Failed to load asset: {} at path: {}", WStrToStr(tag), WStrToStr(path));
			}
			else
			{
				ENGINE_LOG_INFO("Successfully loaded asset: {} at path: {}", WStrToStr(tag), WStrToStr(path));
			}
		}
	}
}