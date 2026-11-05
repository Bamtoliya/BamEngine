#include "MainMenuBarLayout.h"
#include "LocalizationManager.h"
#include "Editor_Macros.h"
#include <imgui.h>
#include <iostream>
#include <optional>

using Engine::LocalizationManager;

namespace
{
    struct ItemRect
    {
        ImVec2 Min{}, Max{};
        void Capture() { Min = ImGui::GetItemRectMin(); Max = ImGui::GetItemRectMax(); }
        ImVec2 Center() const { return ImVec2((Min.x + Max.x) * 0.5f, (Min.y + Max.y) * 0.5f); }
    };

    struct MenuInput
    {
        Editor::MainMenuBarLayout Layout;
        ItemRect Help, Language, English, Korean;
        bool HelpOpen = false, LanguageOpen = false;
        int LegacyRejectedClicks = 0;
        std::optional<ELocalizationLanguage> PendingLanguage;

        explicit MenuInput(float scale)
        {
            ImGui::CreateContext();
            auto& io = ImGui::GetIO();
            io.IniFilename = nullptr;
            io.LogFilename = nullptr;
            io.DisplaySize = ImVec2(1800.0f, 1000.0f);
            io.DeltaTime = 1.0f / 60.0f;
            io.FontGlobalScale = scale;
            unsigned char* pixels;
            int width, height;
            io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        }
        ~MenuInput() { ImGui::DestroyContext(); }

        void Frame()
        {
            if (PendingLanguage)
            {
                LocalizationManager::Get().SetCurrentLanguage(*PendingLanguage);
                PendingLanguage.reset();
            }
            HelpOpen = LanguageOpen = false;
            Layout = {};
            ImGui::NewFrame();
            if (ImGui::BeginMainMenuBar())
            {
                ImGui::TextUnformatted("BamEngine");
                ImGui::SameLine(0.0f, 15.0f);
                for (const char* key : { "EditorMenu.File", "EditorMenu.Edit", "Viewport.Scene", "EditorMenu.Window" })
                    if (ImGui::BeginMenu(LOCAL_LABEL(key).c_str())) ImGui::EndMenu();
                HelpOpen = ImGui::BeginMenu(LOCAL_LABEL("EditorMenu.Help").c_str());
                Help.Capture();
                if (HelpOpen)
                {
                    LanguageOpen = ImGui::BeginMenu(LOCAL_LABEL("Settings.Language").c_str());
                    Language.Capture();
                    if (LanguageOpen)
                    {
                        if (ImGui::MenuItem(LOCAL_LABEL("Settings.LanguageEnglish").c_str()))
                            PendingLanguage = ELocalizationLanguage::English;
                        English.Capture();
                        if (ImGui::MenuItem(LOCAL_LABEL("Settings.LanguageKorean").c_str()))
                            PendingLanguage = ELocalizationLanguage::Korean;
                        Korean.Capture();
                        ImGui::EndMenu();
                    }
                    ImGui::EndMenu();
                }
                const ImVec2 origin = ImGui::GetMainViewport()->Pos;
                Layout.Top = ImGui::GetWindowPos().y - origin.y;
                Layout.Bottom = Layout.Top + ImGui::GetWindowHeight();
                Layout.MenusEndX = ImGui::GetCursorScreenPos().x - origin.x;
                ImGui::SameLine(ImGui::GetWindowWidth() - 135.0f);
                Layout.WindowButtonsBeginX = ImGui::GetCursorScreenPos().x - origin.x;
                ImGui::Button("-");
                ImGui::SameLine();
                ImGui::Button("[]");
                ImGui::SameLine();
                ImGui::Button("X");
                ImGui::EndMainMenuBar();
            }
            ImGui::Render();
        }

        void Move(ImVec2 point)
        {
            ImGui::GetIO().AddMousePosEvent(point.x, point.y);
            Frame();
            Frame();
        }

        bool Click(bool applyNativeHitTest = false)
        {
            const auto mouse = ImGui::GetIO().MousePos; // Synthetic platform input for the test backend.
            if (applyNativeHitTest && Layout.IsDragArea(mouse.x, mouse.y)) return false;
            ImGui::GetIO().AddMouseButtonEvent(0, true);
            Frame();
            ImGui::GetIO().AddMouseButtonEvent(0, false);
            Frame();
            return true;
        }

        bool ChangeLanguage(ELocalizationLanguage language)
        {
            Move(ImVec2(900, 500));
            Move(Help.Center());
            if (!HelpOpen && !Click(true)) return false;
            Frame();
            if (!HelpOpen) return false;
            Move(Language.Center());
            Frame();
            if (!LanguageOpen) return false;
            Move((language == ELocalizationLanguage::English ? English : Korean).Center());
            Click();
            Frame();
            Frame();
            if (LocalizationManager::Get().GetCurrentLanguage() != language) return false;

            Move(ImVec2(900, 500));
            Move(Help.Center());
            const auto mouse = ImGui::GetIO().MousePos;
            if (Layout.IsDragArea(Help.Max.x - 0.5f, Help.Center().y)) return false;
            if (mouse.x > 350.0f && mouse.x < 1650.0f && mouse.y < 36.0f) ++LegacyRejectedClicks;
            if (!Click(true) || !HelpOpen) return false;
            if (!Layout.IsDragArea((Layout.MenusEndX + Layout.WindowButtonsBeginX) * 0.5f, Help.Center().y))
                return false;
            Click();
            return true;
        }
    };
}

int main()
{
    if (!LocalizationManager::Create()) return 1;
    struct Scope { ~Scope() { LocalizationManager::Destroy(); } } scope;
    auto& localization = LocalizationManager::Get();
    localization.RegisterLocalizationData(L"Resources/Data/Localization/English.json");
    localization.RegisterLocalizationData(L"Resources/Data/Localization/Korean.json");
    if (Engine::IsFailure(localization.LoadData())) return 2;
    const Editor::MainMenuBarLayout empty;
    if (empty.IsDragArea(0.0f, 0.0f)) return 3;
    const Editor::MainMenuBarLayout crowded{ 0.0f, 30.0f, 600.0f, 500.0f };
    if (crowded.IsDragArea(550.0f, 15.0f)) return 4;
    int legacyRejectedClicks = 0;
    for (float scale : { 0.75f, 1.0f, 1.5f, 2.0f })
    {
        MenuInput menu(scale);
        localization.SetCurrentLanguage(ELocalizationLanguage::Korean);
        menu.Frame();
        menu.Frame();
        if (!menu.ChangeLanguage(ELocalizationLanguage::English) || !menu.ChangeLanguage(ELocalizationLanguage::Korean))
        {
            std::cerr << "Help did not open on the first click after a language change; scale=" << scale << '\n';
            return 5;
        }
        if (menu.Layout.IsDragArea(menu.Layout.WindowButtonsBeginX + 1.0f, menu.Help.Center().y)) return 6;
        if (menu.Layout.IsDragArea(menu.Layout.MenusEndX + 1.0f, menu.Layout.Bottom + 1.0f)) return 7;
        legacyRejectedClicks += menu.LegacyRejectedClicks;
    }
    if (legacyRejectedClicks == 0)
    {
        std::cerr << "Test did not cover Help extending beyond the legacy 350px boundary.\n";
        return 8;
    }
    std::cout << "Language switches and first Help clicks passed at four scales; legacy boundary intercepted "
        << legacyRejectedClicks << " of those clicks.\n";
    return 0;
}
