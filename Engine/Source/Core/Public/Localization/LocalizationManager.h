#pragma once

#include "Base.h"

enum class ELocalizationLanguage : uint8
{
	English = 0,
	Korean,
	Max
};

BEGIN(Engine)
class ENGINE_API LocalizationManager : public Base
{
	DECLARE_SINGLETON(LocalizationManager)

#pragma region Constructor&Destructor
private:
	LocalizationManager() {}
	virtual ~LocalizationManager() {}
	EResult Initialize(void* arg = nullptr);
public:
	virtual void Free() {}
#pragma endregion

#pragma region Localization Management
public:
	ELocalizationLanguage GetCurrentLanguage() const { return m_CurrentLanguage; }
	EResult SetCurrentLanguage(ELocalizationLanguage language) { m_CurrentLanguage = language; return EResult::Success; }
#pragma endregion

#pragma region Data Management
public:
	EResult LoadData();

	EResult RegisterLocalizationData(const wstring& filePath);
	string GetText(const string& key) const;
	EResult AddText(ELocalizationLanguage lang, const string& key, const string& text);
#pragma endregion

#pragma region Variables
private:
	ELocalizationLanguage m_CurrentLanguage = ELocalizationLanguage::English;
	vector<unordered_map<string, string>> m_LocalizationData;
	vector<wstring> m_RegisteredFiles;
#pragma endregion

	
};
END