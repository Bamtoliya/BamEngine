#pragma once

#include "LocalizationManager.h"

IMPLEMENT_SINGLETON(LocalizationManager)

#pragma region Constructor&Destructor
EResult LocalizationManager::Initialize(void* arg)
{
	m_LocalizationData.resize(static_cast<int>(ELocalizationLanguage::Max));
	m_CurrentLanguage = ELocalizationLanguage::Korean;
	return EResult::Success;
}
#pragma endregion


#pragma region Data Management
EResult LocalizationManager::LoadData()
{
	if (m_RegisteredFiles.empty())
	{
		std::cerr << "No localization files registered." << std::endl;
		return EResult::Fail;
	}
	for (const wstring& filePath : m_RegisteredFiles)
	{
		ifstream file(filePath);
		if (!file.is_open())
		{
			std::cerr << "Failed to open localization file: " << std::filesystem::path(filePath).string() << std::endl;
			continue;
		}

		stringstream buffer;
		buffer << file.rdbuf();
		string content = buffer.str();

		unordered_map<string, unordered_map<string, string>> rawData;
		auto result =glz::read_json(rawData, content);
		if (result)
		{
			string errorMessage = "Error parsing localization file: " + std::filesystem::path(filePath).string() + " - " + std::to_string(static_cast<int>(result.ec));
			std::cerr << errorMessage << std::endl;
		}

		for (const auto& [langKey, dataMap] : rawData)
		{
			ELocalizationLanguage lang = ELocalizationLanguage::Max;
			if (langKey == "English")
			{
				lang = ELocalizationLanguage::English;
			}
			else if (langKey == "Korean")
			{
				lang = ELocalizationLanguage::Korean;
			}
			else
			{
				std::cerr << "Unknown language key in localization file: " << langKey << std::endl;
				continue;
			}
			int index = static_cast<int>(lang);
			for (const auto& [key, text] : dataMap)
			{
				m_LocalizationData[index][key] = text;
			}
		}
	}
	return EResult::Success;
}


EResult LocalizationManager::RegisterLocalizationData(const wstring& filePath)
{
	if(!filesystem::exists(filePath)) return EResult::FileNotFound;
	m_RegisteredFiles.push_back(filePath);
	return EResult::Success;
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

EResult LocalizationManager::AddText(ELocalizationLanguage lang, const string& key, const string& text)
{
	if (lang >= ELocalizationLanguage::Max) return EResult::InvalidArgument;
	int index = static_cast<int>(lang);
	m_LocalizationData[index][key] = text;
	return EResult::Success;
}
#pragma endregion