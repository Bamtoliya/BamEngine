#include "ClientBuildJob.h"
#include "ClientPackage.h"
#include "Functions.h"

#include <SDL3/SDL_process.h>
#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_timer.h>
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
                        output += Engine::WStrToStr(decoded);
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
            m_WorkerThread = std::thread([this, projectRoot, package = std::move(package)]() mutable {
                Result res;
                try
                {
                    res = Build(projectRoot, package, [this](std::string_view text) { AppendOutput(text); });
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

    ClientBuildJob::Result ClientBuildJob::Build(const std::filesystem::path& projectRoot,
        const std::optional<ClientPackageRequest>& package, OutputCallback onOutput)
    {
        struct SdlTlsCleanup
        {
            ~SdlTlsCleanup() { SDL_CleanupTLS(); }
        };
        const SdlTlsCleanup tlsCleanup{};

        const auto cmake = Engine::WStrToStr(BAM_EDITOR_CMAKE_COMMAND);
        const auto workingDirectory = Engine::WStrToStr(projectRoot.wstring());
        constexpr auto buildDirectory = "out/build/x64-client-release";
        const char* configureArgs[] = { cmake.c_str(), "--preset", "x64-release", "-B", buildDirectory,
            "-A", "x64", "-DBAMENGINE_WITH_EDITOR=OFF", nullptr };

        Result result;
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

        const char* buildArgs[] = { cmake.c_str(), "--build", buildDirectory, "--config", "Release",
            "--target", "Client", "GameModule", "--parallel", nullptr };
        if (onOutput) onOutput("\n[CMake build]\n");
        if (!RunCMake(buildArgs, workingDirectory, onOutput))
            return result;

        const auto binDirectory = projectRoot / buildDirectory / "bin" / "Release";
        for (const wchar_t* fileName : { L"Client.exe", L"Engine.dll", L"GameModule.dll" })
        {
            const auto filePath = binDirectory / fileName;
            std::error_code error;
            if (!std::filesystem::is_regular_file(filePath, error))
            {
                if (onOutput)
                {
                    onOutput("\nExpected build output is unavailable: ");
                    onOutput(Engine::WStrToStr(filePath.wstring()) + "\n");
                }
                return result;
            }
        }

        if (onOutput)
        {
            onOutput("\nBuild output: ");
            onOutput(Engine::WStrToStr(binDirectory.wstring()) + "\n");
        }

        if (package)
        {
            if (onOutput) onOutput("\n[Package export]\n");
            try
            {
                ExportClientPackage(projectRoot, binDirectory, *package, onOutput);
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