#include "ClientBuildJob.h"
#include "ClientPackage.h"
#include "Functions.h"
#include "PlatformPath.h"
#include "ProjectDescriptor.h"

#include <SDL3/SDL_process.h>
#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_thread.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <chrono>
#include <exception>
#include <memory>
#include <utility>

namespace
{
    void AppendBuildOutput(std::string_view bytes, std::string& output)
    {
        if (bytes.starts_with("\xEF\xBB\xBF"))
            bytes.remove_prefix(3);

        while (!bytes.empty())
        {
            const auto newline = bytes.find('\n');
            const auto line = bytes.substr(0, newline);

            if (!line.empty() && std::in_range<int>(line.size()))
            {
                const int size = static_cast<int>(line.size());
                if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, line.data(), size, nullptr, 0) > 0)
                {
                    output.append(line);
                }
                else
                {
                    const int length = MultiByteToWideChar(CP_OEMCP, 0, line.data(), size, nullptr, 0);
                    std::wstring decoded(length, L'\0');
                    if (length > 0 && MultiByteToWideChar(CP_OEMCP, 0, line.data(), size, decoded.data(), length) > 0)
                        output += Engine::Unicode::WideToUtf8(decoded);
                    else
                        output += "\xEF\xBF\xBD";
                }
            }

            if (newline == std::string_view::npos)
                break;

            output.push_back('\n');
            bytes.remove_prefix(newline + 1);
        }
    }

    bool RunCMake(const char** args, const std::string& workingDirectory,
        const Editor::ClientBuildJob::OutputCallback& onOutput)
    {
        const auto properties = SDL_CreateProperties();
        if (!properties)
        {
            if (onOutput) onOutput(SDL_GetError());
            return false;
        }

        const bool configured = SDL_SetPointerProperty(properties, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, args)
            && SDL_SetStringProperty(properties, SDL_PROP_PROCESS_CREATE_WORKING_DIRECTORY_STRING, workingDirectory.c_str())
            && SDL_SetNumberProperty(properties, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_APP)
            && SDL_SetBooleanProperty(properties, SDL_PROP_PROCESS_CREATE_STDERR_TO_STDOUT_BOOLEAN, true);
        std::unique_ptr<SDL_Process, decltype(&SDL_DestroyProcess)> process(
            configured ? SDL_CreateProcessWithProperties(properties) : nullptr, &SDL_DestroyProcess);
        SDL_DestroyProperties(properties);

        if (!process)
        {
            if (onOutput) onOutput(SDL_GetError());
            return false;
        }

        SDL_IOStream* stream = SDL_GetProcessOutput(process.get());
        char buffer[4096];
        int exitCode = -1;
        bool exited = false;
        std::string rawResidual;

        while (!exited)
        {
            if (stream)
            {
                while (true)
                {
                    const size_t bytesRead = SDL_ReadIO(stream, buffer, sizeof(buffer));
                    if (bytesRead == 0)
                        break;

                    rawResidual.append(buffer, bytesRead);
                    const auto lastNewline = rawResidual.rfind('\n');
                    if (lastNewline != std::string::npos)
                    {
                        std::string decoded;
                        AppendBuildOutput(std::string_view(rawResidual.data(), lastNewline + 1), decoded);
                        if (!decoded.empty() && onOutput)
                            onOutput(decoded);
                        rawResidual.erase(0, lastNewline + 1);
                    }
                }
            }

            exited = SDL_WaitProcess(process.get(), false, &exitCode);
            if (!exited)
                SDL_Delay(10);
        }

        if (stream)
        {
            while (true)
            {
                const size_t bytesRead = SDL_ReadIO(stream, buffer, sizeof(buffer));
                if (bytesRead == 0)
                    break;
                rawResidual.append(buffer, bytesRead);
            }
        }

        if (!rawResidual.empty())
        {
            std::string decoded;
            AppendBuildOutput(rawResidual, decoded);
            if (!decoded.empty() && onOutput)
                onOutput(decoded);
            rawResidual.clear();
        }

        return exitCode == 0;
    }
}

namespace Editor
{
    ClientBuildJob::~ClientBuildJob()
    {
        if (m_WorkerThread.joinable())
            m_WorkerThread.join();
    }

    void ClientBuildJob::AppendOutput(std::string_view text)
    {
        std::lock_guard lock(m_OutputMutex);
        m_LiveOutput.append(text);
    }

    void ClientBuildJob::Start(const std::filesystem::path& projectRoot, std::optional<ClientPackageRequest> package)
    {
        if (m_Status == Status::Running)
            return;

        if (m_WorkerThread.joinable())
            m_WorkerThread.join();

        {
            std::lock_guard lock(m_OutputMutex);
            m_LiveOutput.clear();
            m_Output.clear();
        }
        m_ExportedDirectory.clear();
        m_IsExport = package.has_value();
        m_IsFinished.store(false, std::memory_order_relaxed);

        try
        {
            auto input = package ? PrepareClientPackageInput(projectRoot, *package) : ClientPackageInput{};
            m_WorkerThread = std::thread([this, projectRoot, package = std::move(package), input = std::move(input)] {
                Result res;
                try
                {
                    res = Build(projectRoot, package, input, [this](std::string_view text) { AppendOutput(text); });
                }
                catch (const std::exception& error)
                {
                    res.Success = false;
                    res.ErrorMessage = error.what();
                }
                {
                    std::lock_guard lock(m_ResultMutex);
                    m_Result = std::move(res);
                }
                m_IsFinished.store(true, std::memory_order_release);
                });
            m_Status = Status::Running;
        }
        catch (const std::exception& error)
        {
            m_Status = Status::Failed;
            m_Output = error.what();
        }
    }

    void ClientBuildJob::SetExportPreparationError(std::string output)
    {
        if (m_Status == Status::Running)
            return;
        m_Status = Status::Failed;
        m_IsExport = true;
        m_ExportedDirectory.clear();
        m_Output = std::move(output);
    }

    void ClientBuildJob::Update()
    {
        if (m_Status != Status::Running)
            return;

        {
            std::lock_guard lock(m_OutputMutex);
            if (m_Output.size() != m_LiveOutput.size())
                m_Output = m_LiveOutput;
        }

        if (!m_IsFinished.load(std::memory_order_acquire))
            return;

        if (m_WorkerThread.joinable())
            m_WorkerThread.join();

        Result res;
        {
            std::lock_guard lock(m_ResultMutex);
            res = std::move(m_Result);
        }

        m_Status = res.Success ? Status::Succeeded : Status::Failed;
        m_ExportedDirectory = std::move(res.ExportedDirectory);
        if (!res.ErrorMessage.empty())
            AppendOutput(std::string("\n[Build Error] ") + res.ErrorMessage);

        {
            std::lock_guard lock(m_OutputMutex);
            m_Output = m_LiveOutput;
        }

        m_IsFinished.store(false, std::memory_order_relaxed);
    }

    ClientBuildJob::Result ClientBuildJob::Build(const std::filesystem::path& projectRoot, const std::optional<ClientPackageRequest>& package, const ClientPackageInput& input, OutputCallback onOutput)
    {
        struct SdlTlsCleanup
        {
            ~SdlTlsCleanup() { SDL_CleanupTLS(); }
        };
        const SdlTlsCleanup tlsCleanup{};

        Result result;

        const auto engineRoot = Engine::PlatformPath::GetEngineRoot();

        if (engineRoot.empty())
        {
            result.ErrorMessage = "Engine root is unavailable.";
            return result;
        }

        const auto cmake = Engine::Unicode::WideToUtf8(BAM_EDITOR_CMAKE_COMMAND);

        const auto workingDirectory = Engine::Unicode::WideToUtf8(engineRoot.wstring());

        const auto engineBuildPath = engineRoot / "out" / "build" / "x64-client-release";

        const auto buildDirectory = Engine::Unicode::WideToUtf8(engineBuildPath.wstring());

        const char* configureArgs[] =
        {
            cmake.c_str(),
            "--preset", "x64-release",
            "-B", buildDirectory.c_str(),
            "-A", "x64",
            "-DBAMENGINE_WITH_EDITOR=OFF",
            "-DBAMENGINE_SHIPPING=OFF",
            nullptr
        };

        if (package)
        {
            try
            {
                const auto destination = InspectClientPackageDestination(projectRoot, *package);
                if (destination.Directory != package->Destination.Directory ||
                    destination.ExistingPackage != package->Destination.ExistingPackage)
                {
                    if (onOutput) onOutput("Export destination changed after selection. Start the export again.\n");
                    return result;
                }
            }
            catch (const std::exception& error)
            {
                if (onOutput) onOutput(std::string(error.what()) + "\n");
                return result;
            }
        }

        if (onOutput) onOutput("[CMake configure]\n");
        if (!RunCMake(configureArgs, workingDirectory, onOutput))
            return result;

        const char* buildArgs[] = { cmake.c_str(), "--build", buildDirectory.c_str(), "--config", "Release",
            "--target", "Client", "GameModule", "--parallel", nullptr };
        if (onOutput) onOutput("\n[CMake build]\n");
        if (!RunCMake(buildArgs, workingDirectory, onOutput))
            return result;

        const auto binDirectory = engineBuildPath / "bin" / "Release";
        for (const wchar_t* fileName : { L"Client.exe", L"Engine.dll", L"GameModule.dll" })
        {
            const auto filePath = binDirectory / fileName;
            std::error_code error;
            if (!std::filesystem::is_regular_file(filePath, error))
            {
                if (onOutput)
                {
                    onOutput("\nExpected build output is unavailable: ");
                    onOutput(Engine::Unicode::WideToUtf8(filePath.wstring()) + "\n");
                }
                return result;
            }
        }

        if (onOutput)
        {
            onOutput("\nBuild output: ");
            onOutput(Engine::Unicode::WideToUtf8(binDirectory.wstring()) + "\n");
        }

        const auto normalizedProjectRoot = Engine::PlatformPath::Normalize(projectRoot);
        const auto normalizedEngineRoot = Engine::PlatformPath::Normalize(engineRoot);

        if (normalizedProjectRoot != normalizedEngineRoot)
        {
            const auto projectFile = Engine::ProjectDescriptor::FindProjectFile(projectRoot);

            Engine::ProjectDescriptor descriptor;

            if (!projectFile.empty())
            {
                if (!Engine::ProjectDescriptor::LoadFromFile(projectFile, descriptor))
                {
                    result.ErrorMessage = "Could not load the project descriptor.";
                    return result;
                }
            }

            const auto modules = descriptor.ResolveModules(projectRoot,
                { Engine::EProjectModuleRole::Client, false });

            bool configured = false;

            for (const auto& module : modules)
            {
                std::string lowerType = module.Type;

                std::transform(lowerType.begin(), lowerType.end(), lowerType.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

                if (!lowerType.empty() && lowerType != "runtime")
                    continue;

                const auto sourceDir = Engine::Unicode::WideToUtf8(module.CMakeSourceRoot.wstring());
                const auto moduleBuildDir = Engine::Unicode::WideToUtf8(module.BuildDirectory.wstring());

                const auto engineRootArg = "-DBAMENGINE_ROOT=" + workingDirectory;
                const auto engineBuildArg = "-DBAMENGINE_BUILD_DIR=" + buildDirectory;

                if (!configured)
                {
                    const char* moduleConfigureArgs[] =
                    {
                        cmake.c_str(),
                        "-S", sourceDir.c_str(),
                        "-B", moduleBuildDir.c_str(),
                        "-G", "Visual Studio 18 2026",
                        "-A", "x64",
                        "-DCMAKE_BUILD_TYPE=Release",
                        "-DCMAKE_CONFIGURATION_TYPES=Release",
                        engineRootArg.c_str(),
                        engineBuildArg.c_str(),
                        nullptr
                    };

                    if (onOutput)
                        onOutput("\n[Project module configure]\n");

                    if (!RunCMake(moduleConfigureArgs, workingDirectory, onOutput))
                    {
                        result.ErrorMessage = "External project module configuration failed.";
                        return result;
                    }

                    configured = true;
                }

                const char* moduleBuildArgs[] =
                {
                    cmake.c_str(),
                    "--build", moduleBuildDir.c_str(),
                    "--config", "Release",
                    "--target", module.Name.c_str(),
                    "--parallel",
                    nullptr
                };

                if (onOutput)
                    onOutput("\n[Project module build] " + module.Name + "\n");

                if (!RunCMake(moduleBuildArgs, workingDirectory, onOutput))
                {
                    result.ErrorMessage = "Project module build failed: " + module.Name;
                    return result;
                }

                std::error_code ec;

                if (!std::filesystem::is_regular_file(module.BinaryPath, ec))
                {
                    result.ErrorMessage = "Expected project module is missing: " + module.Name;
                    return result;
                }
            }
        }

        if (package)
        {
            if (onOutput) onOutput("\n[Package export]\n");
            try
            {
                ExportClientPackage(projectRoot, binDirectory, *package, input, onOutput);
                result.ExportedDirectory = package->Destination.Directory;
            }
            catch (const std::exception& error)
            {
                if (onOutput) onOutput(std::string(error.what()) + "\n");
                return result;
            }
        }

        result.Success = true;
        return result;
    }
}
