#pragma once

#include "Base.h"
#include <functional>
#include <string_view>
#include <unordered_map>

enum class ELocalizationLanguage : uint8
{
	English = 0,
	Korean,
	Max
};

BEGIN(Engine)

class ENGINE_API LocalizedText final
{
	friend class LocalizationManager;

private:
	LocalizedText(const string* text, std::string_view key) : m_Text(text)
	{
		if (!text) m_Fallback.assign(key);
	}

public:
	const char* c_str() const { return m_Text ? m_Text->c_str() : m_Fallback.c_str(); }

private:
	const string* m_Text = nullptr;
	string m_Fallback;
};

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
	EResult SetCurrentLanguage(ELocalizationLanguage language)
	{
		if (language >= ELocalizationLanguage::Max)
			return EResult::InvalidArgument;

		m_CurrentLanguage = language;
		return EResult::Success;
	}
#pragma endregion

#pragma region Data Management
public:
	EResult LoadData();
	EResult ReloadData();

	EResult RegisterLocalizationData(const wstring& filePath);
private:
	const string* FindText(std::string_view key) const;
public:
	string GetText(std::string_view key) const;
	string GetText(const wstring& key) const;
	LocalizedText GetTextForDisplay(std::string_view key) const;
	LocalizedText GetTextForDisplay(const wstring& key) const;
	EResult AddText(ELocalizationLanguage lang, const string& key, const string& text);

#pragma endregion

#pragma region Variables
private:
	ELocalizationLanguage m_CurrentLanguage = ELocalizationLanguage::English;
	struct TextKeyHash
	{
		using is_transparent = void;

		std::size_t operator()(std::string_view key) const noexcept
		{
			return std::hash<std::string_view>{}(key);
		}
	};

	using TextMap = std::unordered_map<string, string, TextKeyHash, std::equal_to<>>;
	vector<TextMap> m_LocalizationData;
	vector<wstring> m_RegisteredFiles;
#pragma endregion

	
};
END