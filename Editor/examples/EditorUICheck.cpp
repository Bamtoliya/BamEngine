#undef DBG_NEW
#undef new
#include <imgui.h>
#include <imgui_internal.h>
#ifndef DBG_NEW
#define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )
#define new DBG_NEW
#endif

#include "EditorUI.h"
#include "InputManager.h"
#include "IconsFontAwesome7.h"
#include "LocalizationManager.h"
#include <SDL3/SDL.h>
#include <iostream>
#include <cmath>
#include <cassert>

namespace
{
    // ── ImGui 헤드리스 테스트 환경 래퍼 ──
    struct HeadlessImGuiContext
    {
        ImGuiContext* Context = nullptr;

        explicit HeadlessImGuiContext(float fontScale = 1.0f, ImVec2 displaySize = ImVec2(1280.0f, 720.0f))
        {
            Context = ImGui::CreateContext();
            auto& io = ImGui::GetIO();
            io.IniFilename = nullptr;
            io.LogFilename = nullptr;
            io.DisplaySize = displaySize;
            io.DeltaTime = 1.0f / 60.0f;
            io.FontGlobalScale = fontScale;

            unsigned char* pixels = nullptr;
            int width = 0, height = 0;
            io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        }

        ~HeadlessImGuiContext()
        {
            if (Context)
            {
                ImGui::DestroyContext(Context);
                Context = nullptr;
            }
        }
    };

    void PressKey(Engine::InputManager& input, SDL_Scancode key)
    {
        SDL_Event event{};
        event.type = SDL_EVENT_KEY_DOWN;
        event.key.scancode = key;
        input.ProcessEvent(event);
        input.Update(1.0f / 60.0f);
    }

    void ReleaseKey(Engine::InputManager& input, SDL_Scancode key)
    {
        SDL_Event event{};
        event.type = SDL_EVENT_KEY_UP;
        event.key.scancode = key;
        input.ProcessEvent(event);
        input.Update(1.0f / 60.0f);
    }

    // ── 1. DialogButtonBar 계약 검증 ──
    void TestDialogButtonBar()
    {
        HeadlessImGuiContext env;
        auto& input = Engine::InputManager::Get();

        const EditorUI::DialogAction actions[] = {
            { "EditorCommon.Cancel", true, false },
            { "EditorCommon.Discard", true, true },
            { "EditorCommon.Save", true, false }
        };
        constexpr int actionCount = IM_ARRAYSIZE(actions);

        // 1-1. 넓은 폭(1줄)에서의 높이 측정 일치 검증
        ImGui::NewFrame();
        ImGui::SetNextWindowSize(ImVec2(800.0f, 400.0f));
        ImGui::Begin("TestDialogWide", nullptr, ImGuiWindowFlags_NoDecoration);

        const float predictedHeightWide = EditorUI::GetDialogButtonBarHeight(actions, actionCount);
        const float cursorBeforeWide = ImGui::GetCursorPosY();
        int selectedWide = EditorUI::DialogButtonBar("WideActions", actions, actionCount);
        const float cursorAfterWide = ImGui::GetCursorPosY();
        // ImGui는 다음 위젯을 위해 마지막 버튼 뒤에 ItemSpacing.y(4px)를 가산하므로 이를 제외한 순수 바운딩 높이와 비교
        const float actualHeightWide = (cursorAfterWide - cursorBeforeWide) - ImGui::GetStyle().ItemSpacing.y;

        if (std::abs(predictedHeightWide - actualHeightWide) > 1.5f)
        {
            std::cerr << "[FAIL] DialogButtonBar wide height mismatch. Predicted: "
                << predictedHeightWide << ", Actual: " << actualHeightWide << std::endl;
            std::exit(1);
        }
        ImGui::End();
        ImGui::Render();

        // 1-2. 좁은 폭(세로 줄바꿈)에서의 높이 측정 일치 검증
        ImGui::NewFrame();
        ImGui::SetNextWindowSize(ImVec2(150.0f, 600.0f));
        ImGui::Begin("TestDialogNarrow", nullptr, ImGuiWindowFlags_NoDecoration);

        const float predictedHeightNarrow = EditorUI::GetDialogButtonBarHeight(actions, actionCount);
        const float cursorBeforeNarrow = ImGui::GetCursorPosY();
        int selectedNarrow = EditorUI::DialogButtonBar("NarrowActions", actions, actionCount);
        const float cursorAfterNarrow = ImGui::GetCursorPosY();
        // 세로 줄바꿈에서도 마지막 버튼 뒤의 ItemSpacing.y(4px)를 제외한 순수 높이와 비교
        const float actualHeightNarrow = (cursorAfterNarrow - cursorBeforeNarrow) - ImGui::GetStyle().ItemSpacing.y;

        if (std::abs(predictedHeightNarrow - actualHeightNarrow) > 1.5f)
        {
            std::cerr << "[FAIL] DialogButtonBar narrow height mismatch. Predicted: "
                << predictedHeightNarrow << ", Actual: " << actualHeightNarrow << std::endl;
            std::exit(1);
        }
        ImGui::End();
        ImGui::Render();

        // 1-3. CancelIndex = -1 일 때 Escape 키 무반응 검증 (IsWindowAppearing 해제를 위해 1프레임 선회)
        ImGui::NewFrame();
        ImGui::SetNextWindowSize(ImVec2(500.0f, 300.0f));
        ImGui::Begin("TestDialogEscapeNegative", nullptr, ImGuiWindowFlags_NoDecoration);
        ImGui::End();
        ImGui::Render();

        ImGui::NewFrame();
        ImGui::SetNextWindowSize(ImVec2(500.0f, 300.0f));
        ImGui::Begin("TestDialogEscapeNegative", nullptr, ImGuiWindowFlags_NoDecoration);
        ImGui::SetWindowFocus();

        PressKey(input, SDL_SCANCODE_ESCAPE);

        EditorUI::DialogButtonBarOptions noEscapeOpt{ .CancelIndex = -1 };
        int selectedNoEscape = EditorUI::DialogButtonBar("NoEscapeActions", actions, actionCount, noEscapeOpt);
        if (selectedNoEscape != -1)
        {
            std::cerr << "[FAIL] DialogButtonBar triggered selection on Escape when CancelIndex = -1. Result: "
                << selectedNoEscape << std::endl;
            std::exit(1);
        }
        ReleaseKey(input, SDL_SCANCODE_ESCAPE);
        ImGui::End();
        ImGui::Render();

        // 1-4. CancelIndex = 0 일 때 Escape 키 취소 선택 검증
        ImGui::NewFrame();
        ImGui::SetNextWindowSize(ImVec2(500.0f, 300.0f));
        ImGui::Begin("TestDialogEscapeCancel", nullptr, ImGuiWindowFlags_NoDecoration);
        ImGui::End();
        ImGui::Render();

        ImGui::NewFrame();
        ImGui::SetNextWindowSize(ImVec2(500.0f, 300.0f));
        ImGui::Begin("TestDialogEscapeCancel", nullptr, ImGuiWindowFlags_NoDecoration);
        ImGui::SetWindowFocus();

        PressKey(input, SDL_SCANCODE_ESCAPE);

        EditorUI::DialogButtonBarOptions escapeOpt{ .CancelIndex = 0 };
        int selectedEscape = EditorUI::DialogButtonBar("EscapeActions", actions, actionCount, escapeOpt);
        if (selectedEscape != 0)
        {
            std::cerr << "[FAIL] DialogButtonBar failed to select CancelIndex on Escape. Result: "
                << selectedEscape << std::endl;
            std::exit(1);
        }
        ReleaseKey(input, SDL_SCANCODE_ESCAPE);
        ImGui::End();
        ImGui::Render();

        std::cout << "[PASS] DialogButtonBar contracts verified." << std::endl;
    }

    // ── 2. DrawSearchBar 계약 검증 ──
    void TestDrawSearchBar()
    {
        HeadlessImGuiContext env;
        auto& input = Engine::InputManager::Get();

        char buffer[128] = "Sample Query";

        // 2-1. FocusOnDraw = true 및 Escape 입력 시 초기화 계약 검증
        ImGui::NewFrame();
        ImGui::SetNextWindowSize(ImVec2(400.0f, 300.0f));
        ImGui::Begin("TestSearchBar", nullptr, ImGuiWindowFlags_NoDecoration);
        ImGui::SetWindowFocus();

        EditorUI::SearchBarOptions options{ .Width = 250.0f, .FocusOnDraw = true };
        EditorUI::DrawSearchBar("SearchBar1", buffer, sizeof(buffer), "EditorCommon.Search", options);

        ImGui::PushID("SearchBar1");
        const ImGuiID inputId = ImGui::GetID("##SearchInput");
        ImGui::PopID();

        ImGui::End();
        ImGui::Render();

        // 다음 프레임에서 ActiveID 설정 및 Escape 키 주입
        ImGui::NewFrame();
        ImGui::SetNextWindowSize(ImVec2(400.0f, 300.0f));
        ImGui::Begin("TestSearchBar", nullptr, ImGuiWindowFlags_NoDecoration);
        ImGui::SetWindowFocus();

        ImGui::SetActiveID(inputId, ImGui::GetCurrentWindow());
        PressKey(input, SDL_SCANCODE_ESCAPE);

        auto res2 = EditorUI::DrawSearchBar("SearchBar1", buffer, sizeof(buffer), "EditorCommon.Search", options);

        if (!res2.Cleared || buffer[0] != '\0')
        {
            std::cerr << "[FAIL] DrawSearchBar failed to clear buffer on Escape. Cleared: "
                << res2.Cleared << ", Buffer: '" << buffer << "'" << std::endl;
            std::exit(1);
        }

        ReleaseKey(input, SDL_SCANCODE_ESCAPE);
        ImGui::End();
        ImGui::Render();

        // 2-2. 빈 버퍼일 때 상태 불변 확인
        ImGui::NewFrame();
        ImGui::SetNextWindowSize(ImVec2(400.0f, 300.0f));
        ImGui::Begin("TestSearchBarEmpty", nullptr, ImGuiWindowFlags_NoDecoration);

        buffer[0] = '\0';
        auto resEmpty = EditorUI::DrawSearchBar("SearchBarEmpty", buffer, sizeof(buffer));
        if (resEmpty.Changed || resEmpty.Cleared)
        {
            std::cerr << "[FAIL] DrawSearchBar changed state on empty buffer." << std::endl;
            std::exit(1);
        }

        ImGui::End();
        ImGui::Render();

        std::cout << "[PASS] DrawSearchBar contracts verified." << std::endl;
    }

    // ── 3. IconButton / SmallIconButton 배율 계약 검증 ──
    void TestIconButtonScaling()
    {
        const float testScales[] = { 1.0f, 1.5f, 2.0f };
        for (float scale : testScales)
        {
            HeadlessImGuiContext env(scale);

            ImGui::NewFrame();
            ImGui::Begin("TestIconButtonScaling", nullptr, ImGuiWindowFlags_NoDecoration);

            const char* testIcon = ICON_FA_FLOPPY_DISK;
            const ImVec2 textSize = ImGui::CalcTextSize(testIcon);

            // 일반 IconButton
            const ImVec2 standardSize = EditorUI::GetIconButtonSize(testIcon, false);
            if (standardSize.x < textSize.x || standardSize.y < textSize.y)
            {
                std::cerr << "[FAIL] GetIconButtonSize standard size (" << standardSize.x << ", " << standardSize.y
                    << ") smaller than glyph (" << textSize.x << ", " << textSize.y << ") at scale " << scale << std::endl;
                std::exit(1);
            }

            // Compact SmallIconButton
            const ImVec2 compactSize = EditorUI::GetIconButtonSize(testIcon, true);
            if (compactSize.x < textSize.x || compactSize.y < textSize.y)
            {
                std::cerr << "[FAIL] GetIconButtonSize compact size (" << compactSize.x << ", " << compactSize.y
                    << ") smaller than glyph (" << textSize.x << ", " << textSize.y << ") at scale " << scale << std::endl;
                std::exit(1);
            }

            ImGui::End();
            ImGui::Render();
        }

        std::cout << "[PASS] IconButton scaling contracts verified across 1.0x, 1.5x, 2.0x." << std::endl;
    }

    // ── 4. ToolbarLayout 폭 경계 계약 검증 ──
    void TestToolbarLayoutBounds()
    {
        HeadlessImGuiContext env;

        constexpr float windowWidth = 220.0f;
        ImGui::NewFrame();
        ImGui::SetNextWindowSize(ImVec2(windowWidth, 400.0f));
        ImGui::SetNextWindowPos(ImVec2(50.0f, 50.0f));
        ImGui::Begin("TestToolbarNarrow", nullptr, ImGuiWindowFlags_NoDecoration);

        const float windowRight = ImGui::GetWindowPos().x + windowWidth;

        EditorUI::ToolbarLayout toolbar;
        // 연속으로 여러 버튼 배치
        toolbar.Icon("Tool1", ICON_FA_FILE, "EditorCommon.New");
        toolbar.Icon("Tool2", ICON_FA_FOLDER_OPEN, "EditorCommon.Open");
        toolbar.Icon("Tool3", ICON_FA_FLOPPY_DISK, "EditorCommon.Save");
        toolbar.Button("ToolBtn1", ICON_FA_PLAY, "EditorCommon.Play");
        toolbar.Button("ToolBtn2", ICON_FA_STOP, "EditorCommon.Stop");
        toolbar.SetNextItemWidth(100.0f);
        toolbar.Text("Status");

        const float maxItemX = ImGui::GetItemRectMax().x;
        if (maxItemX > windowRight + 2.0f)
        {
            std::cerr << "[FAIL] ToolbarLayout item exceeded window right boundary. MaxItemX: "
                << maxItemX << ", WindowRight: " << windowRight << std::endl;
            std::exit(1);
        }

        ImGui::End();
        ImGui::Render();

        std::cout << "[PASS] ToolbarLayout narrow boundary wrapping verified." << std::endl;
    }
}

int main()
{
    if (!SDL_Init(SDL_INIT_EVENTS) || !Engine::InputManager::Create() || !Engine::LocalizationManager::Create())
    {
        std::cerr << "[FAIL] Failed to initialize SDL3, InputManager, or LocalizationManager." << std::endl;
        return 1;
    }

    struct ScopeGuard
    {
        ~ScopeGuard()
        {
            Engine::LocalizationManager::Destroy();
            Engine::InputManager::Destroy();
            SDL_Quit();
        }
    } guard;

    // 로캘 데이터 등록 및 로드
    auto& localization = Engine::LocalizationManager::Get();
    localization.RegisterLocalizationData(L"Engine://Data/Localization/English.json");
    localization.RegisterLocalizationData(L"Engine://Data/Localization/Korean.json");
    localization.RegisterLocalizationData(L"Editor://Data/Localization/English.json");
    localization.RegisterLocalizationData(L"Editor://Data/Localization/Korean.json");
    localization.LoadData();

    std::cout << "==================================================" << std::endl;
    std::cout << "         EditorUI Headless Contract Checks        " << std::endl;
    std::cout << "==================================================" << std::endl;

    TestDialogButtonBar();
    TestDrawSearchBar();
    TestIconButtonScaling();
    TestToolbarLayoutBounds();

    std::cout << "==================================================" << std::endl;
    std::cout << "    All EditorUI checks passed successfully!      " << std::endl;
    std::cout << "==================================================" << std::endl;

    return 0;
}