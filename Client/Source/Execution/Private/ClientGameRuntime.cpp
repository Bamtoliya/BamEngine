#include "Engine_Includes.h"
#include "ClientGameRuntime.h"

#include <archive/JsonArchive.h>
#include <glm/gtc/matrix_transform.hpp>

#include "AsyncResourceLoader.h"
#include "ResourceStreamer.h"
#include "AudioManager.h"
#include "BuiltInPrimitiveMeshes.h"
#include "CameraComponent.h"
#include "ComponentEnable.h"
#include "ConfigManager.h"
#if defined(_WIN32)
#include "CrashHandler.h"
#endif
#include "FlagComponent.h"
#include "GraphBindingComponent.h"
#include "InputActionSettings.h"
#include "LocalizationManager.h"
#include "PakArchive.h"
#include "PixelPerfectCameraComponent.h"
#include "PlatformPath.h"
#include "StartupResourceLoader.h"
#include "Systems.h"

Client::ClientGameRuntime::~ClientGameRuntime()
{
    Free();
}

Engine::EResult Client::ClientGameRuntime::LoadProject(const std::filesystem::path& projectRoot)
{
    if (m_ProjectLoaded || m_Runtime || projectRoot.empty())
        return Engine::EResult::InvalidArgument;

    m_ProjectRoot = projectRoot;
    if (!Engine::ReflectionRegistry::Create())
        return Engine::EResult::Fail;

    const auto result = LoadProjectSettings();
    if (result != Engine::EResult::Success)
        return result;

    m_ProjectLoaded = true;
    return Engine::EResult::Success;
}

Engine::EResult Client::ClientGameRuntime::Initialize(const Engine::RendererDesc& rendererDesc,
    const std::function<void()>& loadModules, const std::function<void(ClientStartupStage)>& onStageChanged)
{
    if (!m_ProjectLoaded || m_Runtime || !rendererDesc.rhiDesc)
        return Engine::EResult::InvalidArgument;

    Engine::RUNTIMEDESC runtimeDesc = {};
    runtimeDesc.RendererDesc = rendererDesc;
    runtimeDesc.ProjectRoot = m_ProjectRoot;
    runtimeDesc.AssetRoot = std::filesystem::u8path(m_ProjectSettings.AssetDirectory);
    runtimeDesc.EngineResourceRoot = std::filesystem::u8path(m_ProjectSettings.EngineResourceDirectory);

    if (onStageChanged)
        onStageChanged(ClientStartupStage::Graphics);
    m_Runtime = Engine::Runtime::Create(&runtimeDesc);
    if (!m_Runtime)
        return Engine::EResult::Fail;

    if (!Engine::AsyncResourceLoader::IsCreated())
        Engine::AsyncResourceLoader::Create();

    TryMountPackages();
    TryLoadAudioMixerSettings();

    if (onStageChanged)
        onStageChanged(ClientStartupStage::Resources);

    auto& localization = Engine::LocalizationManager::Get();
    bool registeredCatalog = false;
    for (const auto* uri : { L"Engine://Data/Localization/English.json", L"Engine://Data/Localization/Korean.json" })
    {
        const auto path = Engine::PlatformPath::ResolveUri(uri);
        std::error_code error;
        if (std::filesystem::is_regular_file(path, error) && IsSuccess(localization.RegisterLocalizationData(path.wstring())))
            registeredCatalog = true;
    }
    if (registeredCatalog) localization.LoadData();
    if (!IsSuccess(localization.ReloadProjectData()))
        BAM_LOG(Warn, "Client", "Startup", "Project localization catalogs could not be loaded.");

    Engine::RegisterBuiltInPrimitiveMeshes(Engine::ResourceManager::Get());
    if (!Engine::StartupResourceLoader::LoadPipeline(m_ProjectSettings.StartupManifestPath))
        return Engine::EResult::Fail;

    Engine::InputActionSettings inputSettings;
    const auto inputPath = m_ProjectRoot / "Config" / "InputActions.json";
    const auto& registry = Engine::ReflectionRegistry::Get().GetRegistry();
    const auto inputLoad = Engine::ConfigManager::Load(inputSettings, inputPath, registry);
    if (inputLoad.Status == Engine::EConfigLoadStatus::Failed ||
        (inputLoad.Status == Engine::EConfigLoadStatus::Loaded &&
            !Engine::InputManager::Get().SetInputActionSettings(inputSettings)))
    {
        BAM_LOG(Error, "Client", "Startup", "Could not load or apply Config/InputActions.json");
        return Engine::EResult::Fail;
    }

    if (IsFailure(m_RenderPipeline.Initialize(m_ProjectSettings.RenderPipelineProfilePath,
        rendererDesc.rhiDesc->width, rendererDesc.rhiDesc->height)))
        return Engine::EResult::Fail;
    Engine::ResourceStreamer::Get().SetBudgetMB(static_cast<Engine::uint32>((std::max)(m_ProjectSettings.ResourceStreamingBudgetMB, 0)));
    m_RenderPipeline.SetRaytracedShadowMode(Engine::ToRaytracedShadowMode(m_ProjectSettings.RaytracedShadowMode));
    m_RenderPipeline.SetRaytracedReflectionMode(Engine::ToRaytracedShadowMode(m_ProjectSettings.RaytracedReflectionMode));
    m_RenderPipeline.SetTemporalAAMode(Engine::ToRaytracedShadowMode(m_ProjectSettings.TemporalAAMode));
    if (IsFailure(InitializeSystems()))
        return Engine::EResult::Fail;
    if (loadModules)
        loadModules();
    if (onStageChanged)
        onStageChanged(ClientStartupStage::Scene);
    if (IsFailure(LoadStartupScene()) || !ValidateStartupGraphs())
        return Engine::EResult::Fail;

    BAM_LOG(Info, "Client", "Application", "Client initialization finished successfully");
    return Engine::EResult::Success;
}

Engine::EResult Client::ClientGameRuntime::RunFrame(Engine::uint32 width, Engine::uint32 height, glm::vec2 logicalSize,
    bool inputFocused, bool pointerInWindow)
{
    if (!m_Runtime)
        return Engine::EResult::InvalidArgument;

    if (Engine::AsyncResourceLoader::IsCreated())
    {
        Engine::AsyncResourceLoader::Get().StepMainThread(2.0);
    }

    auto* rhi = Engine::Renderer::Get().GetRHI();
    if (!rhi)
        return Engine::EResult::Fail;

    auto& timeManager = Engine::TimeManager::Get();
    timeManager.Update();
    const Engine::f32 dt = timeManager.GetDeltaTime();
    auto& input = Engine::InputManager::Get();
    input.Update(dt);
    if (width == 0 || height == 0)
    {
        m_RuntimeUIInput.Clear();
        input.EvaluateActionMap("Gameplay", false);
        return Engine::EResult::Success;
    }

    if (width != rhi->GetSwapChainWidth() || height != rhi->GetSwapChainHeight())
    {
        const auto result = rhi->Resize(width, height);
        if (result != Engine::EResult::Success)
            return result;
        if (const auto targetResult = m_RenderPipeline.Resize(width, height); IsFailure(targetResult))
        {
            BAM_LOG(Error, "Client", "Rendering", "Could not resize render profile targets to {}x{}", width, height);
            return targetResult;
        }
    }

    Engine::UIPointerInput pointer;
    pointer.focused = inputFocused && logicalSize.x > 0.0f && logicalSize.y > 0.0f;
    const auto insideOutput = [](const glm::vec2& position)
    {
        return position.x >= 0.0f && position.y >= 0.0f && position.x < 1.0f && position.y < 1.0f;
    };
    if (pointer.focused)
    {
        pointer.position = input.GetMousePosition() / logicalSize;
        pointer.insideViewport = pointerInWindow && insideOutput(pointer.position);
        if (const auto down = input.GetMouseButtonDownPosition(Engine::EMouseButton::Left))
        {
            pointer.downPosition = *down / logicalSize;
            pointer.downInsideViewport = insideOutput(*pointer.downPosition);
        }
    }
    pointer.held = input.IsMouseButtonPressed(Engine::EMouseButton::Left);
    pointer.down = input.IsMouseButtonDown(Engine::EMouseButton::Left);
    pointer.up = input.IsMouseButtonUp(Engine::EMouseButton::Left);
    pointer.wheelDelta = input.GetMouseScrollDeltaY();
    bool wheelConsumed = false;
    const bool pointerBlockedByUI = m_RuntimeUIInput.Update(Engine::SceneManager::Get().GetActiveScenes(), pointer,
        &wheelConsumed);
    Engine::InputActionMapMouseInput mouseInput;
    if (pointerBlockedByUI)
        mouseInput.AllowedButtons &= ~(Engine::uint32{ 1 } << static_cast<Engine::uint32>(Engine::EMouseButton::Left));
    mouseInput.MotionDelta = input.GetMouseDelta();
    mouseInput.ScrollDeltaY = wheelConsumed ? 0.0f : pointer.wheelDelta;
    input.EvaluateActionMap("Gameplay", inputFocused, mouseInput);

    while (timeManager.IsFixedUpdateReady())
        m_Runtime->FixedUpdate(timeManager.GetFixedStep());

    m_Runtime->Update(dt, false);
    m_Runtime->LateUpdate(dt);
        SubmitRenderPipeline(dt, width, height);

    return m_Runtime->Render(dt);
}

void Client::ClientGameRuntime::Free()
{
    if (Engine::AsyncResourceLoader::IsCreated())
    {
        Engine::AsyncResourceLoader::Destroy();
    }
    Engine::PakMountManager::Get().UnmountAll();

    if (!m_Runtime && !Engine::ReflectionRegistry::IsCreated())
        return;

    m_RuntimeUIInput.Clear();
    m_RenderPipeline.Free();
    if (m_Runtime)
    {
        Engine::Runtime::Destroy();
        m_Runtime = nullptr;
    }
    else if (Engine::ReflectionRegistry::IsCreated())
    {
        Engine::ReflectionRegistry::Destroy();
    }

    m_ProjectSettings = {};
    m_ProjectRoot.clear();
    m_ProjectLoaded = false;
}

Engine::EResult Client::ClientGameRuntime::LoadProjectSettings()
{
    const auto projectJsonPath = m_ProjectRoot / "Config" / "Project.json";
    const auto& registry = Engine::ReflectionRegistry::Get().GetRegistry();
    const auto loadResult = Engine::ConfigManager::Load(m_ProjectSettings, projectJsonPath, registry);
    if (loadResult.Status == Engine::EConfigLoadStatus::NotFound)
    {
        std::error_code fileError;
        const auto packageMarker = m_ProjectRoot / L".bam-client-package";
        if (std::filesystem::exists(packageMarker, fileError))
        {
            BAM_LOG(Error, "Client", "Config", "Project settings are required for a packaged Client: {}",
                projectJsonPath.string());
            return Engine::EResult::FileNotFound;
        }
        if (fileError)
        {
            BAM_LOG(Error, "Client", "Config", "Could not inspect package marker {}: {}",
                packageMarker.string(), fileError.message());
            return Engine::EResult::Fail;
        }
        BAM_LOG(Warn, "Client", "Config", "Project settings file not found: {}. Using defaults.",
            projectJsonPath.string());
        return Engine::EResult::Success;
    }
    if (loadResult.Status == Engine::EConfigLoadStatus::Failed)
    {
        BAM_LOG(Error, "Client", "Config", "Failed to load project settings from {}: code={}, field={}, fileError={}",
            projectJsonPath.string(), static_cast<int>(loadResult.Error.Code), loadResult.Error.Field,
            loadResult.FileError.message());
        return Engine::EResult::Fail;
    }
    if (m_ProjectSettings.SchemaVersion == 0)
    {
        BAM_LOG(Error, "Client", "Config", "Invalid ProjectSettings SchemaVersion: 0 in {}",
            projectJsonPath.string());
        return Engine::EResult::InvalidArgument;
    }
    if (m_ProjectSettings.SchemaVersion > Engine::ProjectSettings::CurrentSchemaVersion)
    {
        BAM_LOG(Error, "Client", "Config", "Unsupported ProjectSettings SchemaVersion: {} (Max: {}) in {}",
            m_ProjectSettings.SchemaVersion, Engine::ProjectSettings::CurrentSchemaVersion, projectJsonPath.string());
        return Engine::EResult::InvalidArgument;
    }

    BAM_LOG(Info, "Client", "Config", "Loaded project settings (SchemaVersion={}) from {}",
        m_ProjectSettings.SchemaVersion, projectJsonPath.string());
    return Engine::EResult::Success;
}

Engine::EResult Client::ClientGameRuntime::InitializeSystems()
{
    auto& systemManager = Engine::SystemManager::Get();
    if (!systemManager.AddSystem<Engine::TransformSystem>()) return Engine::EResult::Fail;
    if (!systemManager.AddSystem<Engine::SpringArmSystem>()) return Engine::EResult::Fail;
    if (!systemManager.AddSystem<Engine::CameraSystem>()) return Engine::EResult::Fail;
    if (!m_RenderPipeline.UsesDeferredProfile() &&
        !systemManager.AddSystem<Engine::LightSystem>())
        return Engine::EResult::Fail;
    if (!systemManager.AddSystem<Engine::MeshRenderSystem>()) return Engine::EResult::Fail;
    if (!systemManager.AddSystem<Engine::SpriteRenderSystem>()) return Engine::EResult::Fail;
    if (!systemManager.AddSystem<Engine::SpriteAnimationSystem>()) return Engine::EResult::Fail;
    if (!systemManager.AddSystem<Engine::TilemapRenderSystem>()) return Engine::EResult::Fail;
    if (!systemManager.AddSystem<Engine::AnimationSystem>()) return Engine::EResult::Fail;
    if (!systemManager.AddSystem<Engine::ParticleSystem>()) return Engine::EResult::Fail;
    if (!systemManager.AddSystem<Engine::ActionTimelineSystem>()) return Engine::EResult::Fail;
    if (!systemManager.AddSystem<Engine::CompositeVFXSystem>()) return Engine::EResult::Fail;
    if (!systemManager.AddSystem<Engine::ThirdPersonControllerSystem>()) return Engine::EResult::Fail;
    if (!systemManager.AddSystem<Engine::KinematicCharacterSystem>()) return Engine::EResult::Fail;
#if !defined(__EMSCRIPTEN__)
    if (!systemManager.AddSystem<Engine::NetworkSystem>()) return Engine::EResult::Fail;
#endif
    if (!systemManager.AddSystem<Engine::UIRenderSystem>()) return Engine::EResult::Fail;
    if (!systemManager.AddSystem<Engine::AudioSystem>()) return Engine::EResult::Fail;
    if (!systemManager.AddSystem<Engine::TerrainSystem>()) return Engine::EResult::Fail;
    if (!systemManager.AddSystem<Engine::PhysicsSystem>()) return Engine::EResult::Fail;
    if (!systemManager.AddSystem<Engine::GraphSystem>()) return Engine::EResult::Fail;
    return Engine::EResult::Success;
}

Engine::EResult Client::ClientGameRuntime::LoadStartupScene()
{
    if (m_ProjectSettings.StartupScenePath.empty())
    {
        BAM_LOG(Error, "Client", "Startup", "StartupScenePath is empty.");
        return Engine::EResult::InvalidArgument;
    }

    archive::JsonArchive ar;
    Engine::EResult result = Engine::EResult::Fail;

    std::string sceneUri = m_ProjectSettings.StartupScenePath;
    if (!sceneUri.starts_with("Project://") && !sceneUri.starts_with("Engine://"))
    {
        sceneUri = "Project://" + sceneUri;
    }

    std::vector<uint8_t> sceneBytes;
    if (Engine::PakMountManager::Get().HasEntry(sceneUri) &&
        Engine::PakMountManager::Get().TryReadBytes(sceneUri, sceneBytes) == Engine::EResult::Success)
    {
        std::string_view jsonView(reinterpret_cast<const char*>(sceneBytes.data()), sceneBytes.size());
        auto decodeRes = ar.Decode(jsonView);
        if (decodeRes)
        {
            Engine::Scene* candidate = nullptr;
            auto createRes = Engine::Scene::CreateFromArchive(ar, candidate);
            if (createRes && candidate)
            {
                result = Engine::SceneManager::Get().OpenScene(candidate);
                Safe_Release(candidate);
            }
        }
    }
    else
    {
        const std::filesystem::path scenePath =
            Engine::PlatformPath::GetProjectAssetRoot() / std::filesystem::u8path(m_ProjectSettings.StartupScenePath);
        if (!std::filesystem::exists(scenePath))
        {
            BAM_LOG(Error, "Client", "Startup", "Configured startup scene file does not exist: {}",
                scenePath.string());
            return Engine::EResult::FileNotFound;
        }

        result = Engine::SceneManager::Get().LoadScene(ar, scenePath.wstring());
    }

    if (result != Engine::EResult::Success)
    {
        BAM_LOG(Error, "Client", "Startup", "Failed to load startup scene: {}", Engine::ResultToString(result));
        return result;
    }
#if defined(_WIN32)
    Engine::CrashHandler::SetActiveScenePath(m_ProjectSettings.StartupScenePath);
#endif
    BAM_LOG(Info, "Client", "Startup", "Loaded startup scene: {}", m_ProjectSettings.StartupScenePath);
    return Engine::EResult::Success;
}

void Client::ClientGameRuntime::TryMountPackages()
{
    std::error_code ec;
    std::filesystem::path manifestPath;

    for (const auto& base : {
             m_ProjectRoot,
             Engine::PlatformPath::GetProjectRoot(),
             Engine::PlatformPath::GetExecutableDirectory(),
             std::filesystem::current_path()
         })
    {
        if (base.empty())
            continue;

        const auto candidate = base / "Config" / "PackageManifest.json";
        if (std::filesystem::exists(candidate, ec) && std::filesystem::is_regular_file(candidate, ec))
        {
            manifestPath = candidate;
            break;
        }
    }

    if (!manifestPath.empty())
    {
        std::string startupSceneUri;
        const auto mountRes = Engine::PakMountManager::Get().MountFromPackageManifest(manifestPath, &startupSceneUri);
        if (mountRes == Engine::EResult::Success)
        {
            BAM_LOG(Info, "Client", "Startup", "Successfully mounted packages from manifest: {}", manifestPath.string());
            if (!startupSceneUri.empty() && m_ProjectSettings.StartupScenePath.empty())
            {
                m_ProjectSettings.StartupScenePath = startupSceneUri;
            }
        }
        else
        {
            BAM_LOG(Warn, "Client", "Startup", "Failed to mount packages from manifest: {} ({})",
                manifestPath.string(), Engine::ResultToString(mountRes));
        }
    }
    else
    {
        const auto checkAndMount = [](const std::filesystem::path& pakPath)
        {
            std::error_code err;
            if (std::filesystem::exists(pakPath, err) && std::filesystem::is_regular_file(pakPath, err))
            {
                if (Engine::PakMountManager::Get().MountPak(pakPath) == Engine::EResult::Success)
                {
                    BAM_LOG(Info, "Client", "Startup", "Mounted standalone pak: {}", pakPath.string());
                }
            }
        };

        for (const auto& base : {
                 m_ProjectRoot,
                 Engine::PlatformPath::GetProjectRoot(),
                 Engine::PlatformPath::GetExecutableDirectory(),
                 std::filesystem::current_path()
             })
        {
            if (base.empty())
                continue;

            checkAndMount(base / "Content" / "Engine.bampak");
            checkAndMount(base / "Content" / "Project.bampak");
            if (Engine::PakMountManager::Get().GetMountedPakCount() > 0)
                break;
        }
    }
}

void Client::ClientGameRuntime::TryLoadAudioMixerSettings()
{
    if (!Engine::AudioManager::IsCreated())
        return;

    std::error_code ec;
    std::filesystem::path mixerConfigPath;

    for (const auto& base : {
             m_ProjectRoot,
             Engine::PlatformPath::GetProjectRoot(),
             Engine::PlatformPath::GetExecutableDirectory(),
             std::filesystem::current_path()
         })
    {
        if (base.empty())
            continue;

        const auto candidate = base / "Config" / "AudioMixer.json";
        if (std::filesystem::exists(candidate, ec) && std::filesystem::is_regular_file(candidate, ec))
        {
            mixerConfigPath = candidate;
            break;
        }
    }

    if (!mixerConfigPath.empty())
    {
        if (Engine::AudioManager::Get().GetMixer().LoadSettings(mixerConfigPath))
        {
            BAM_LOG(Info, "Client", "Audio", "Loaded audio mixer settings from {}", mixerConfigPath.string());
        }
        else
        {
            BAM_LOG(Warn, "Client", "Audio", "Failed to load audio mixer settings from {}", mixerConfigPath.string());
        }
    }
}

void Client::ClientGameRuntime::SubmitRenderPipeline(Engine::f32 dt,
    Engine::uint32 width, Engine::uint32 height)
{
    auto* currentScene = Engine::SceneManager::Get().GetCurrentScene();
    if (!currentScene)
        return;

    auto& registry = currentScene->GetRegistry();
    auto cameraView = registry.view<Engine::CameraComponent, Engine::FlagComponent>();
    std::optional<CameraBuffer> selectedBuffer;
    bool pixelPerfect = false;
    const Engine::f32 aspectRatio = static_cast<Engine::f32>(width) / static_cast<Engine::f32>(height);

    for (auto entity : cameraView)
    {
        const auto& flag = cameraView.get<Engine::FlagComponent>(entity);
        if (!flag.IsActiveInHierarchy() || Engine::HasFlag(flag.effectiveFlags, Engine::EEntityFlag::Dead))
            continue;

        if (!Engine::IsComponentEnabled<Engine::CameraComponent>(registry, entity)) continue;
        auto& camera = cameraView.get<Engine::CameraComponent>(entity);
        const auto previousAspect = camera.AspectRatio;
        camera.AspectRatio = aspectRatio;
        if (!Engine::CameraSystem::UpdateCamera(registry, entity, width, height))
        {
            camera.AspectRatio = previousAspect;
            continue;
        }
        const auto buffer = camera.GetCameraBuffer();
        if (!buffer)
            continue;

        if (!selectedBuffer || camera.IsMainCamera)
        {
            selectedBuffer = buffer;
            const auto* pixelCamera = Engine::TryGetEnabledComponent<Engine::PixelPerfectCameraComponent>(registry, entity);
            pixelPerfect = camera.CameraType == Engine::ECameraType::Orthographic && pixelCamera &&
                pixelCamera->assetsPPU > 0.0f;
        }

        if (camera.IsMainCamera)
            break;
    }

    if (!selectedBuffer)
    {
        CameraBuffer fallback = {};
        const Engine::vec3 eye(0.0f, 20.0f, -50.0f);
        const Engine::vec3 at(0.0f, 0.0f, 0.0f);
        const Engine::vec3 up(0.0f, 1.0f, 0.0f);
        fallback.viewMatrix = glm::lookAt(eye, at, up);
        fallback.projMatrix = glm::perspective(glm::radians(60.0f), aspectRatio, 0.1f, 1000.0f);
        fallback.invViewMatrix = glm::inverse(fallback.viewMatrix);
        fallback.invProjMatrix = glm::inverse(fallback.projMatrix);
        fallback.viewProjMatrix = fallback.projMatrix * fallback.viewMatrix;
        fallback.invViewProjMatrix = glm::inverse(fallback.viewProjMatrix);
        fallback.cameraPosition = eye;
        selectedBuffer = fallback;
    }

    selectedBuffer->time = static_cast<Engine::f32>(Engine::TimeManager::Get().GetTotalTime());
    m_RenderPipeline.Submit(*selectedBuffer, *currentScene, dt, pixelPerfect);
    if (m_RenderPipeline.UsesDeferredProfile())
    {
        for (auto* scene : Engine::SceneManager::Get().GetActiveScenes())
            if (scene)
                Engine::LightSystem::UpdateLights(scene->GetRegistry());
    }

}

bool Client::ClientGameRuntime::ValidateStartupGraphs()
{
    auto* scene = Engine::SceneManager::Get().GetCurrentScene();
    auto* graphSystem = Engine::SystemManager::Get().GetSystem<Engine::GraphSystem>();
    if (!scene || !graphSystem)
    {
        BAM_LOG(Error, "Client", "Startup", "Startup scene or graph system is unavailable.");
        return false;
    }

    const auto& world = scene->GetRegistry();
    const auto bindings = world.view<Engine::GraphBindingComponent>();
    for (const auto entity : bindings)
    {
        const auto& binding = bindings.get<Engine::GraphBindingComponent>(entity);
        if (Engine::IsComponentEnabled(world, entity, binding) && !binding.graphFile.empty() && !graphSystem->GetOrLoadGraph(binding.graphFile))
            return false;
    }

    return true;
}
