#pragma once

#include "Localization/LocalizationManager.h"

IMPLEMENT_SINGLETON(LocalizationManager)

#pragma region Constructor&Destructor
EResult LocalizationManager::Initialize(void* arg)
{
	m_LocalizationData.resize(static_cast<int>(ELocalizationLanguage::Max));
	return EResult::Success;
}
#pragma endregion


#pragma region Data Management
void LocalizationManager::LoadData()
{
	// 데이터 로드 로직 구현 (예: 파일에서 읽기)
}

string LocalizationManager::GetText(const string& key) const
{
	if (key.empty()) return "";
	int index = static_cast<int>(m_CurrentLanguage);
	auto it = m_LocalizationData[index].find(key);
	if (it != m_LocalizationData[index].end())
	{
		return it->second;
	}

	if (m_CurrentLanguage != ELocalizationLanguage::English)
	{
		index = static_cast<int>(ELocalizationLanguage::English);
		it = m_LocalizationData[index].find(key);
		if (it != m_LocalizationData[index].end())
		{
			return it->second;
		}
	}

	return key;
}

void LocalizationManager::AddText(ELocalizationLanguage lang, const string& key, const string& text)
{
	int index = static_cast<int>(lang);
	m_LocalizationData[index][key] = text;
}
#pragma endregion