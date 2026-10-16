#define SDL_MAIN_HANDLED
#include <SDL3/SDL_main.h>

#include "Engine_Includes.h"
#include "Runtime.h"

BEGIN(Client)

class Application final
{
public:
    Application() = default;
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    ~Application()
    {
        if (m_Runtime)
            Runtime::Destroy();

        if (m_Window)
            SDL_DestroyWindow(m_Window);

        if (m_SDLInitialized)
            SDL_Quit();

        if (m_LoggerInitialized)
            Logger::Shutdown();
    }

    EResult Initialize()
    {
        if (!Logger::Initialize(L"Logs/Client.log"))
            return EResult::Fail;

        m_LoggerInitialized = true;

        if (!SDL_Init(SDL_INIT_VIDEO))
        {
            BAM_LOG(Error, "Client", "SDL_Init failed: {}", SDL_GetError());
            return EResult::Fail;
        }

        m_SDLInitialized = true;
        m_Window = SDL_CreateWindow("BamEngine Client", 1280, 720,
            SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);

        if (!m_Window)
        {
            BAM_LOG(Error, "Client", "SDL_CreateWindow failed: {}", SDL_GetError());
            return EResult::Fail;
        }

        int width = 0;
        int height = 0;
        if (!SDL_GetWindowSizeInPixels(m_Window, &width, &height) || width <= 0 || height <= 0)
            return EResult::Fail;

        const SDL_PropertiesID properties = SDL_GetWindowProperties(m_Window);
        if (!properties)
            return EResult::Fail;

        HWND hwnd = static_cast<HWND>(
            SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));

        if (!hwnd)
            return EResult::Fail;

        DirectX12RHIDesc rhiDesc = {};
        rhiDesc.windowHandle = hwnd;
        rhiDesc.width = static_cast<uint32>(width);
        rhiDesc.height = static_cast<uint32>(height);
        rhiDesc.isVSync = true;

        RUNTIMEDESC runtimeDesc = {};
        runtimeDesc.RendererDesc.rhiType = ERHIType::DirectX12;
        runtimeDesc.RendererDesc.rhiDesc = &rhiDesc;

        m_Runtime = Runtime::Create(&runtimeDesc);
        if (!m_Runtime)
            return EResult::Fail;

        RenderPassManager::Get().RegisterRenderPass(
            L"Client Background", {}, L"",
            ERenderPassLoadOperation::RPLO_Clear, ERenderPassStoreOperation::RPSO_Store,
            ERenderPassLoadOperation::RPLO_Load, ERenderPassStoreOperation::RPSO_Store,
            vec4(0.08f, 0.12f, 0.18f, 1.0f));

        BAM_LOG(Info, "Client", "Initialization finished");
        return EResult::Success;
    }

    EResult Run()
    {
        if (!m_Window || !m_Runtime)
            return EResult::InvalidArgument;

        auto& timeManager = TimeManager::Get();
        RHI* rhi = Renderer::Get().GetRHI();

        while (true)
        {
            bool shouldQuit = false;
            SDL_Event event = {};

            while (SDL_PollEvent(&event))
            {
                InputManager::Get().ProcessEvent(event);

                if (event.type == SDL_EVENT_QUIT)
                    shouldQuit = true;

                if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                    event.window.windowID == SDL_GetWindowID(m_Window))
                {
                    shouldQuit = true;
                }
            }

            if (shouldQuit)
                break;

            timeManager.Update();
            const f32 dt = timeManager.GetDeltaTime();

            if (SDL_GetWindowFlags(m_Window) & SDL_WINDOW_MINIMIZED)
                continue;

            int width = 0;
            int height = 0;
            if (!SDL_GetWindowSizeInPixels(m_Window, &width, &height))
                return EResult::Fail;

            if (width <= 0 || height <= 0)
                continue;

            const uint32 pixelWidth = static_cast<uint32>(width);
            const uint32 pixelHeight = static_cast<uint32>(height);

            if (pixelWidth != rhi->GetSwapChainWidth() || pixelHeight != rhi->GetSwapChainHeight())
            {
                const EResult result = rhi->Resize(pixelWidth, pixelHeight);
                if (result != EResult::Success)
                    return result;
            }

            while (timeManager.IsFixedUpdateReady())
                m_Runtime->FixedUpdate(timeManager.GetFixedStep());

            m_Runtime->Update(dt);
            m_Runtime->LateUpdate(dt);

            const EResult result = m_Runtime->Render(dt);
            if (result != EResult::Success)
                return result;
        }

        return EResult::Success;
    }

private:
    SDL_Window* m_Window = nullptr;
    Runtime* m_Runtime = nullptr;
    bool m_SDLInitialized = false;
    bool m_LoggerInitialized = false;
};

END

int main()
{
    SDL_SetMainReady();

    Client::Application application;
    EResult result = application.Initialize();

    if (result == EResult::Success)
        result = application.Run();

    if (result != EResult::Success)
    {
        fmt::print(stderr, "Client failed: {}\n", ResultToString(result));
        return 1;
    }

    return 0;
}