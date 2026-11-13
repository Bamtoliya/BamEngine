#include "ClientBuildJob.h"
#include "ClientPackage.h"
#include "Functions.h"

#include <SDL3/SDL_process.h>
#include <SDL3/SDL_thread.h>
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

    bool RunCMake(const char** args, const std::string& workingDirectory, std::string& output)
    {
        const auto properties = SDL_CreateProperties();
        if (!properties)
        {
            output += SDL_GetError();
            return false;
        }

        const bool configured = SDL_SetPointerProperty(properties, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, args)
            && SDL_SetStringProperty(properties, SDL_PROP_PROCESS_CREATE_WORKING_DIRECTORY_STRING,
                workingDirectory.c_str())
            && SDL_SetNumberProperty(properties, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_APP)
            && SDL_SetBooleanProperty(properties, SDL_PROP_PROCESS_CREATE_STDERR_TO_STDOUT_BOOLEAN, true);
        std::unique_ptr<SDL_Process, decltype(&SDL_DestroyProcess)> process(
            configured ? SDL_CreateProcessWithProperties(properties) : nullptr, &SDL_DestroyProcess);
        SDL_DestroyProperties(properties);

        if (!process)
        {
            output += SDL_GetError();
            return false;
        }

        std::size_t size = 0;
        int exitCode = -1;
        std::unique_ptr<void, decltype(&SDL_free)> data(SDL_ReadProcess(process.get(), &size, &exitCode), &SDL_free);
        if (!data)
        {
            output += SDL_GetError();
            return false;
        }

        AppendBuildOutput({ static_cast<const char*>(data.get()), size }, output);
        return exitCode == 0;
    }
}

namespace Editor
{
    void ClientBuildJob::Start(const std::filesystem::path& projectRoot, std::optional<ClientPackageRequest> package)
    {
        if (m_Status == Status::Running)
            return;

        m_Output.clear();
        m_ExportedDirectory.clear();
        m_IsExport = package.has_value();
        try
        {
            m_Future = std::async(std::launch::async, &ClientBuildJob::Build, projectRoot, std::move(package));
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
        if (!m_Future.valid() || m_Future.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
            return;

        try
        {
            auto result = m_Future.get();
            m_Status = result.Success ? Status::Succeeded : Status::Failed;
            m_Output = std::move(result.Output);
            m_ExportedDirectory = std::move(result.ExportedDirectory);
        }
        catch (const std::exception& error)
        {
            m_Status = Status::Failed;
            m_Output = error.what();
        }
    }

    ClientBuildJob::Result ClientBuildJob::Build(const std::filesystem::path& projectRoot,
        const std::optional<ClientPackageRequest>& package)
    {
        // SDL does not clean up TLS for the std::async worker.
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
                    result.Output = "Export destination changed after selection. Start the export again.";
                    return result;
                }
            }
            catch (const std::exception& error)
            {
                result.Output = error.what();
                return result;
            }
        }

        result.Output = "[CMake configure]\n";
        if (!RunCMake(configureArgs, workingDirectory, result.Output))
            return result;

        const char* buildArgs[] = { cmake.c_str(), "--build", buildDirectory, "--config", "Release",
            "--target", "Client", "GameModule", nullptr };
        result.Output += "\n[CMake build]\n";
        if (!RunCMake(buildArgs, workingDirectory, result.Output))
            return result;

        const auto binDirectory = projectRoot / buildDirectory / "bin" / "Release";
        for (const wchar_t* fileName : { L"Client.exe", L"Engine.dll", L"GameModule.dll" })
        {
            const auto filePath = binDirectory / fileName;
            std::error_code error;
            if (!std::filesystem::is_regular_file(filePath, error))
            {
                result.Output += "\nExpected build output is unavailable: ";
                result.Output += Engine::WStrToStr(filePath.wstring());
                return result;
            }
        }

        result.Output += "\nBuild output: ";
        result.Output += Engine::WStrToStr(binDirectory.wstring());
        if (package)
        {
            result.Output += "\n\n[Package export]\n";
            try
            {
                ExportClientPackage(projectRoot, binDirectory, *package, result.Output);
                result.ExportedDirectory = package->Destination.Directory;
            }
            catch (const std::exception& error)
            {
                result.Output += error.what();
                return result;
            }
        }

        result.Success = true;
        return result;
    }
}
