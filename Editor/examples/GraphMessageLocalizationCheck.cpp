#include "GraphMessageLocalization.h"
#include <iostream>

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
    std::cout << "Graph diagnostic localization, language switching and identifier preservation checks passed.\n";
    return 0;
}
