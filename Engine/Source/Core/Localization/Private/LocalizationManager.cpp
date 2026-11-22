#pragma once

#include "LocalizationManager.h"
#include "PlatformPath.h"

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
		BAM_LOG(Error, "Engine", "Localization", "No localization files registered.");
		return EResult::Fail;
	}
	for (const wstring& filePath : m_RegisteredFiles)
	{
		ifstream file(filePath);
		if (!file.is_open())
		{
			BAM_LOG(Error, "Engine", "Localization", "Failed to open localization file: {}", std::filesystem::path(filePath).string());
			continue;
		}

		stringstream buffer;
		buffer << file.rdbuf();
		string content = buffer.str();

		unordered_map<string, unordered_map<string, string>> rawData;
		auto result = glz::read_json(rawData, content);
		if (result)
		{
			BAM_LOG(Error, "Engine", "Localization", "Error parsing localization file: {} - {}",
				std::filesystem::path(filePath).string(), static_cast<int>(result.ec));
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
				BAM_LOG(Warn, "Engine", "Localization", "Unknown language key in localization file: {}", langKey);
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

EResult LocalizationManager::ReloadData()
{
	for (auto& dataMap : m_LocalizationData)
	{
		dataMap.clear();
	}
	return LoadData();
}


EResult LocalizationManager::RegisterLocalizationData(const wstring& filePath)
{
	const auto resolved = PlatformPath::ResolveUri(filePath);
	if (!filesystem::exists(resolved)) return EResult::FileNotFound;
	const auto norm = PlatformPath::Normalize(resolved).wstring();
	if (std::find(m_RegisteredFiles.begin(), m_RegisteredFiles.end(), norm) != m_RegisteredFiles.end())
		return EResult::Success;
	m_RegisteredFiles.push_back(norm);
	return EResult::Success;
}

const string* LocalizationManager::FindText(std::string_view key) const
{
	if (key.empty()) return nullptr;

	const auto find = [&](const std::vector<TextMap>& catalog) -> const string*
	{
		if (catalog.empty()) return nullptr;
		const auto& current = catalog[static_cast<std::size_t>(m_CurrentLanguage)];
		const auto translated = current.find(key);
		if (translated != current.end()) return &translated->second;
		if (m_CurrentLanguage == ELocalizationLanguage::English) return nullptr;

		const auto& fallback = catalog[static_cast<std::size_t>(ELocalizationLanguage::English)];
		const auto english = fallback.find(key);
		return english != fallback.end() ? &english->second : nullptr;
	};
	// Project files provide game strings without replacing Engine or Editor labels.
	if (const auto* text = find(m_LocalizationData)) return text;
	return find(m_ProjectLocalizationData);
}

string LocalizationManager::GetText(std::string_view key) const
{
	return GetText(key, key);
}

string LocalizationManager::GetText(std::string_view key, std::string_view fallback) const
{
	if (const string* text = FindText(key))
		return *text;

	return string(fallback);
}

string LocalizationManager::GetText(const wstring& key) const
{
	return GetText(WStrToStr(key));
}

LocalizedText LocalizationManager::GetTextForDisplay(std::string_view key) const
{
	return LocalizedText(FindText(key), key);
}

LocalizedText LocalizationManager::GetTextForDisplay(const wstring& key) const
{
	return GetTextForDisplay(WStrToStr(key));
}

EResult LocalizationManager::AddText(ELocalizationLanguage lang, const string& key, const string& text)
{
	if (lang >= ELocalizationLanguage::Max) return EResult::InvalidArgument;
	int index = static_cast<int>(lang);
	m_LocalizationData[index][key] = text;
	return EResult::Success;
}
#pragma endregion