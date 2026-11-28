#pragma once

#include "LocalizationManager.h"
#include "PlatformPath.h"
#include "PakArchive.h"

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
		std::vector<uint8_t> bytes;
		if (PakMountManager::Get().ReadBytesWithFallback(filePath, bytes) != EResult::Success)
		{
			BAM_LOG(Error, "Engine", "Localization", "Failed to open localization file: {}", std::filesystem::path(filePath).string());
			continue;
		}

		string content(bytes.begin(), bytes.end());

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
	m_ReverseMapDirty = true;
	return EResult::Success;
}

EResult LocalizationManager::ReloadData()
{
	for (auto& dataMap : m_LocalizationData)
	{
		dataMap.clear();
	}
	m_ReverseMapDirty = true;
	return LoadData();
}


EResult LocalizationManager::RegisterLocalizationData(const wstring& filePath)
{
	const auto resolved = PlatformPath::ResolveUri(filePath);
	if (!PakMountManager::Get().ExistsOrOnDisk(resolved)) return EResult::FileNotFound;
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
	return GetText(Engine::Unicode::WideToUtf8(key));
}

LocalizedText LocalizationManager::GetTextForDisplay(std::string_view key) const
{
	return LocalizedText(FindText(key), key);
}

LocalizedText LocalizationManager::GetTextForDisplay(const wstring& key) const
{
	return GetTextForDisplay(Engine::Unicode::WideToUtf8(key));
}

EResult LocalizationManager::AddText(ELocalizationLanguage lang, const string& key, const string& text)
{
	if (lang >= ELocalizationLanguage::Max) return EResult::InvalidArgument;
	int index = static_cast<int>(lang);
	m_LocalizationData[index][key] = text;
	m_ReverseMapDirty = true;
	return EResult::Success;
}

void LocalizationManager::RebuildReverseMaps() const
{
	m_KoreanToEnglish.clear();
	m_EnglishToKorean.clear();
	m_KorToEngTemplates.clear();
	m_EngToKorTemplates.clear();

	const auto buildFromCatalog = [this](const std::vector<TextMap>& catalog)
	{
		if (catalog.size() <= static_cast<std::size_t>(ELocalizationLanguage::Korean))
			return;

		const auto& engMap = catalog[static_cast<std::size_t>(ELocalizationLanguage::English)];
		const auto& korMap = catalog[static_cast<std::size_t>(ELocalizationLanguage::Korean)];

		for (const auto& [key, korText] : korMap)
		{
			if (korText.empty()) continue;
			auto it = engMap.find(key);
			if (it == engMap.end() || it->second.empty()) continue;

			const std::string& engText = it->second;

			const auto engPos = engText.find("{}");
			const auto korPos = korText.find("{}");
			if (engPos != std::string::npos && korPos != std::string::npos &&
				engText.find("{}", engPos + 2) == std::string::npos &&
				korText.find("{}", korPos + 2) == std::string::npos)
			{
				TemplatePattern ke;
				ke.SourcePrefix = korText.substr(0, korPos);
				ke.SourceSuffix = korText.substr(korPos + 2);
				ke.TargetPrefix = engText.substr(0, engPos);
				ke.TargetSuffix = engText.substr(engPos + 2);
				m_KorToEngTemplates.push_back(std::move(ke));

				TemplatePattern ek;
				ek.SourcePrefix = engText.substr(0, engPos);
				ek.SourceSuffix = engText.substr(engPos + 2);
				ek.TargetPrefix = korText.substr(0, korPos);
				ek.TargetSuffix = korText.substr(korPos + 2);
				m_EngToKorTemplates.push_back(std::move(ek));
			}
			else if (engPos == std::string::npos && korPos == std::string::npos)
			{
				m_KoreanToEnglish.emplace(korText, engText);
				m_EnglishToKorean.emplace(engText, korText);
			}
		}
	};

	buildFromCatalog(m_LocalizationData);
	buildFromCatalog(m_ProjectLocalizationData);
	m_ReverseMapDirty = false;
}

std::string LocalizationManager::TranslateLogMessage(std::string_view message, ELocalizationLanguage targetLang) const
{
	if (message.empty() || targetLang >= ELocalizationLanguage::Max)
		return std::string(message);

	if (m_ReverseMapDirty)
		RebuildReverseMaps();

	const bool toEnglish = (targetLang == ELocalizationLanguage::English);
	const auto& exactMap = toEnglish ? m_KoreanToEnglish : m_EnglishToKorean;
	const auto& templates = toEnglish ? m_KorToEngTemplates : m_EngToKorTemplates;

	const auto translatePhrase = [&](std::string_view phrase) -> std::string
	{
		const std::string s(phrase);
		auto it = exactMap.find(s);
		if (it != exactMap.end())
			return it->second;

		for (const auto& tmpl : templates)
		{
			if (s.size() >= tmpl.SourcePrefix.size() + tmpl.SourceSuffix.size() &&
				s.starts_with(tmpl.SourcePrefix) &&
				s.ends_with(tmpl.SourceSuffix))
			{
				const auto argLen = s.size() - tmpl.SourcePrefix.size() - tmpl.SourceSuffix.size();
				const auto arg = s.substr(tmpl.SourcePrefix.size(), argLen);
				return tmpl.TargetPrefix + arg + tmpl.TargetSuffix;
			}
		}
		return s;
	};

	// 1. Direct exact or template match
	std::string direct = translatePhrase(message);
	if (direct != message)
		return direct;

	// 2. "Title: Message" compound match (e.g., from NotificationManager)
	const auto colonPos = message.find(": ");
	if (colonPos != std::string_view::npos)
	{
		const auto prefix = message.substr(0, colonPos);
		const auto suffix = message.substr(colonPos + 2);
		std::string transPrefix = translatePhrase(prefix);
		std::string transSuffix = translatePhrase(suffix);
		if (transPrefix != prefix || transSuffix != suffix)
			return transPrefix + ": " + transSuffix;
	}

	return std::string(message);
}
#pragma endregion