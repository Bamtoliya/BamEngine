#include "GraphMessageLocalization.h"
#include <iostream>
#include <string>
#include <string_view>

int main()
{
    using Engine::LocalizationManager;
    if (!LocalizationManager::Create()) return 1;
    struct Scope { ~Scope() { LocalizationManager::Destroy(); } } scope;
    auto& localization = LocalizationManager::Get();
    localization.RegisterLocalizationData(L"Resources/Data/Localization/English.json");
    localization.RegisterLocalizationData(L"Resources/Data/Localization/Korean.json");
    if (Engine::IsFailure(localization.LoadData())) return 2;
    localization.SetCurrentLanguage(ELocalizationLanguage::English);
    if (Editor::LocalizeGraphMessage("Division by zero.") != "Division by zero.") return 3;
    if (Editor::LocalizeGraphMessage("Unknown node type: Custom.Node") != "Unknown node type: Custom.Node") return 4;
    localization.SetCurrentLanguage(ELocalizationLanguage::Korean);
    if (Editor::LocalizeGraphMessage("Division by zero.") != "0으로 나눌 수 없습니다.") return 5;
    const auto withDetails = Editor::LocalizeGraphMessage("Unknown node type: Custom.Node");
    if (withDetails != "알 수 없는 노드 유형: Custom.Node") return 6;
    if (Editor::LocalizeGraphMessage("Third-party error: custom details") != "Third-party error: custom details") return 7;
    if (!Editor::LocalizeGraphMessage("").empty()) return 8;
    if (std::string_view(localization.GetTextForDisplay("Metrics.CurrentFrame").c_str()) != "현재 프레임")
        return 9;

    const auto missingA = localization.GetTextForDisplay(std::string("__DisplayCheck.MissingA"));
    const auto missingB = localization.GetTextForDisplay(std::wstring(L"__DisplayCheck.MissingB"));
    if (std::string_view(missingA.c_str()) != "__DisplayCheck.MissingA" ||
        std::string_view(missingB.c_str()) != "__DisplayCheck.MissingB")
        return 10;

    if (localization.GetTextForDisplay("").c_str()[0] != '\0') return 11;
    localization.AddText(ELocalizationLanguage::English, "__DisplayCheck.EnglishOnly", "English fallback");
    if (std::string_view(localization.GetTextForDisplay("__DisplayCheck.EnglishOnly").c_str()) != "English fallback")
        return 12;

    if (Engine::IsFailure(localization.ReloadData())) return 13;
    if (std::string_view(localization.GetTextForDisplay("Metrics.CurrentFrame").c_str()) != "현재 프레임")
        return 14;
    if (std::string_view(missingA.c_str()) != "__DisplayCheck.MissingA") return 15;
    const auto previousLanguage = localization.GetCurrentLanguage();
    const ELocalizationLanguage invalidLanguages[] = {
        ELocalizationLanguage::Max, static_cast<ELocalizationLanguage>(255)
    };

    for (const auto language : invalidLanguages)
    {
        if (localization.SetCurrentLanguage(language) != Engine::EResult::InvalidArgument) return 16;
        if (localization.GetCurrentLanguage() != previousLanguage) return 17;
    }

    if (std::string_view(localization.GetTextForDisplay("Metrics.CurrentFrame").c_str()) != "현재 프레임")
        return 18;

    if (localization.SetCurrentLanguage(ELocalizationLanguage::English) != Engine::EResult::Success) return 19;
    if (std::string_view(localization.GetTextForDisplay("Metrics.CurrentFrame").c_str()) != "Current frame")
        return 20;

    if (localization.SetCurrentLanguage(previousLanguage) != Engine::EResult::Success) return 21;
    if (std::string_view(localization.GetTextForDisplay("Metrics.CurrentFrame").c_str()) != "현재 프레임")
        return 22;
    std::cout << "Graph diagnostic localization, language switching and identifier preservation checks passed.\n";
    return 0;
}
