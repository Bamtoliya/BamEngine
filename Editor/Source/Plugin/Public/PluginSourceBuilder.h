#pragma once

#include <atomic>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#include <system_error>
#include <algorithm>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace Editor
{
    struct PluginBuildResult
    {
        bool Success = false;
        int ExitCode = -1;
        std::string PluginId;
        std::filesystem::path SourceDir;
        std::filesystem::path OutputBinDir;
        std::filesystem::path StagedDll;
        std::filesystem::path PublishedDll;
        std::string ErrorMessage;
        std::string Logs;
    };

    class PluginSourceBuilder final
    {
    public:
        PluginSourceBuilder() = default;
        ~PluginSourceBuilder()
        {
            StopAndJoin();
        }

        PluginSourceBuilder(const PluginSourceBuilder&) = delete;
        PluginSourceBuilder& operator=(const PluginSourceBuilder&) = delete;

        void Free()
        {
            StopAndJoin();
        }

        void StopAndJoin()
        {
            if (m_WorkerThread.joinable())
            {
                m_WorkerThread.join();
            }
        }

        bool IsBuilding() const
        {
            return m_IsBuilding.load(std::memory_order_acquire);
        }

        bool HasNewResult() const
        {
            return m_HasNewResult.load(std::memory_order_acquire);
        }

        void ClearNewResult()
        {
            m_HasNewResult.store(false, std::memory_order_release);
        }

        std::string GetLogs() const
        {
            const std::lock_guard lock(m_LogMutex);
            return m_Logs;
        }

        PluginBuildResult GetLastResult() const
        {
            const std::lock_guard lock(m_ResultMutex);
            return m_LastResult;
        }

        bool StartBuild(std::string_view pluginId,
            const std::filesystem::path& sourceDir,
            const std::filesystem::path& outputBinDir,
            const std::filesystem::path& editorExeDir = {},
            const std::filesystem::path& expectedDllName = {},
            const std::filesystem::path& cmakeExe = {})
        {
            if (IsBuilding())
                return false;

            StopAndJoin();

            {
                const std::lock_guard lock(m_LogMutex);
                m_Logs.clear();
            }
            {
                const std::lock_guard lock(m_ResultMutex);
                m_LastResult = PluginBuildResult{};
            }

            m_HasNewResult.store(false, std::memory_order_release);
            m_IsBuilding.store(true, std::memory_order_release);

            const std::string idStr(pluginId);
            const std::filesystem::path src = sourceDir;
            const std::filesystem::path bin = outputBinDir;
            const std::filesystem::path editorBuildDir = FindEditorBuildDir(editorExeDir);
            const std::filesystem::path expectedName = expectedDllName;
            const std::filesystem::path exe = cmakeExe.empty() ? FindDefaultCMakeExe() : cmakeExe;

            m_WorkerThread = std::thread([this, idStr, src, bin, editorBuildDir, expectedName, exe]()
            {
                ExecuteBuildPipeline(idStr, src, bin, editorBuildDir, expectedName, exe);
            });

            return true;
        }

    private:
        static std::filesystem::path FindDefaultCMakeExe()
        {
#if defined(BAM_EDITOR_CMAKE_COMMAND)
            std::error_code ec;
            std::filesystem::path configured(BAM_EDITOR_CMAKE_COMMAND);
            if (std::filesystem::is_regular_file(configured, ec))
                return configured;
#endif
            return L"cmake";
        }

        static std::filesystem::path FindEditorBuildDir(const std::filesystem::path& exeDir)
        {
            if (exeDir.empty())
                return {};
            std::error_code ec;
            for (const auto& candidate : { exeDir.parent_path().parent_path(), exeDir.parent_path() })
            {
                if (!candidate.empty() && std::filesystem::is_regular_file(candidate / "CMakeCache.txt", ec))
                    return candidate;
            }
            return {};
        }

        static std::string ReadCMakeCacheValue(const std::filesystem::path& cachePath, std::string_view keyPrefix)
        {
            std::ifstream cache(cachePath);
            for (std::string line; std::getline(cache, line);)
            {
                if (!line.starts_with(keyPrefix))
                    continue;
                const size_t equal = line.find('=');
                if (equal != std::string::npos)
                    return line.substr(equal + 1);
            }
            return {};
        }

        static std::wstring Utf8ToWide(std::string_view str)
        {
#if defined(_WIN32)
            if (str.empty())
                return {};
            const int len = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0);
            if (len <= 0)
                return {};
            std::wstring out(static_cast<std::size_t>(len), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), out.data(), len);
            return out;
#else
            return {};
#endif
        }

        static bool HasMsvcEnvironment()
        {
#if defined(_WIN32)
            return GetEnvironmentVariableW(L"INCLUDE", nullptr, 0) > 0;
#else
            return true;
#endif
        }

        static std::wstring GetCommandProcessorPath()
        {
#if defined(_WIN32)
            wchar_t buffer[MAX_PATH]{};
            const DWORD length = GetEnvironmentVariableW(L"ComSpec", buffer, MAX_PATH);
            return (length > 0 && length < MAX_PATH) ? std::wstring(buffer) : std::wstring(L"C:\\Windows\\System32\\cmd.exe");
#else
            return {};
#endif
        }

        static std::wstring FindDevEnvironmentCommand(const std::filesystem::path& buildDir)
        {
#if defined(_WIN32)
            if (buildDir.empty())
                return {};

            std::string compiler = ReadCMakeCacheValue(buildDir / "CMakeCache.txt", "CMAKE_CXX_COMPILER:");
            if (compiler.find_first_of("/\\") == std::string::npos)
            {
                std::error_code ec;
                const auto cmakeFiles = buildDir / "CMakeFiles";
                if (std::filesystem::is_directory(cmakeFiles, ec))
                {
                    for (const auto& entry : std::filesystem::directory_iterator(cmakeFiles, std::filesystem::directory_options::skip_permission_denied, ec))
                    {
                        if (!entry.is_directory(ec))
                            continue;
                        const auto compilerFile = entry.path() / "CMakeCXXCompiler.cmake";
                        if (!std::filesystem::is_regular_file(compilerFile, ec))
                            continue;
                        std::ifstream configuredCompiler(compilerFile);
                        constexpr std::string_view prefix = "set(CMAKE_CXX_COMPILER \"";
                        for (std::string line; std::getline(configuredCompiler, line);)
                        {
                            if (!line.starts_with(prefix))
                                continue;
                            const auto end = line.find('"', prefix.size());
                            if (end != std::string::npos)
                                compiler = line.substr(prefix.size(), end - prefix.size());
                            break;
                        }
                        if (!compiler.empty())
                            break;
                    }
                }
            }

            std::replace(compiler.begin(), compiler.end(), '\\', '/');
            const std::string marker = "/VC/Tools/MSVC/";
            const size_t markerPos = compiler.find(marker);
            const size_t versionBegin = markerPos == std::string::npos ? std::string::npos : markerPos + marker.size();
            const size_t versionEnd = versionBegin == std::string::npos ? std::string::npos : compiler.find('/', versionBegin);
            if (versionEnd == std::string::npos)
                return {};

            const std::string binMarker = "/bin/Host";
            const size_t hostBegin = compiler.find(binMarker, versionEnd);
            const size_t hostEnd = hostBegin == std::string::npos ? std::string::npos : compiler.find('/', hostBegin + binMarker.size());
            const size_t archEnd = hostEnd == std::string::npos ? std::string::npos : compiler.find('/', hostEnd + 1);
            if (archEnd == std::string::npos)
                return {};

            const auto devArch = [](std::string_view arch) -> std::wstring
            {
                if (arch == "x64") return L"amd64";
                if (arch == "x86") return L"x86";
                if (arch == "arm64") return L"arm64";
                return {};
            };
            const auto hostArch = devArch(std::string_view(compiler).substr(hostBegin + binMarker.size(), hostEnd - hostBegin - binMarker.size()));
            const auto targetArch = devArch(std::string_view(compiler).substr(hostEnd + 1, archEnd - hostEnd - 1));
            if (hostArch.empty() || targetArch.empty())
                return {};

            const auto devCmd = std::filesystem::path(Utf8ToWide(compiler.substr(0, markerPos))) / "Common7" / "Tools" / "VsDevCmd.bat";
            std::error_code ec;
            if (!std::filesystem::is_regular_file(devCmd, ec))
                return {};

            const std::string toolsVersion = compiler.substr(versionBegin, versionEnd - versionBegin);
            return L"call \"" + devCmd.wstring() + L"\" -arch=" + targetArch + L" -host_arch=" + hostArch +
                L" -vcvars_ver=" + Utf8ToWide(toolsVersion) + L" -no_logo";
#else
            return {};
#endif
        }

        void AppendLog(std::string_view text)
        {
            const std::lock_guard lock(m_LogMutex);
            m_Logs.append(text);
        }

        static std::filesystem::path FindVcpkgInstalled(const std::filesystem::path& sourceDir,
            const std::filesystem::path& editorBuildDir)
        {
            std::error_code ec;
            if (!editorBuildDir.empty())
            {
                const auto candidate = editorBuildDir / "vcpkg_installed";
                if (std::filesystem::is_directory(candidate, ec))
                    return std::filesystem::canonical(candidate, ec);
            }
            auto cur = sourceDir;
            for (int i = 0; i < 6 && !cur.empty(); ++i)
            {
                for (const auto& sub : { "out/build/x64-release/vcpkg_installed",
                                         "out/build/x64-debug/vcpkg_installed",
                                         "out/build/x64-ninja-debug/vcpkg_installed",
                                         "vcpkg_installed" })
                {
                    auto cand = cur / sub;
                    if (std::filesystem::is_directory(cand, ec))
                        return std::filesystem::canonical(cand, ec);
                }
                const auto parent = cur.parent_path();
                if (parent == cur) break;
                cur = parent;
            }
            return {};
        }

        bool RunProcess(const std::wstring& application, const std::wstring& cmd,
            const std::filesystem::path& workingDir, int& exitCode)
        {
#if defined(_WIN32)
            SECURITY_ATTRIBUTES sa{};
            sa.nLength = sizeof(SECURITY_ATTRIBUTES);
            sa.bInheritHandle = TRUE;

            HANDLE hReadPipe = nullptr, hWritePipe = nullptr;
            if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0))
            {
                AppendLog("Failed to create execution pipe.\n");
                return false;
            }
            SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

            STARTUPINFOW si{};
            si.cb = sizeof(STARTUPINFOW);
            si.hStdOutput = hWritePipe;
            si.hStdError = hWritePipe;
            si.dwFlags |= STARTF_USESTDHANDLES;

            PROCESS_INFORMATION pi{};
            std::vector<wchar_t> cmdBuffer(cmd.begin(), cmd.end());
            cmdBuffer.push_back(L'\0');

            std::wstring workDirStr = workingDir.wstring();
            const wchar_t* appPtr = application.empty() ? nullptr : application.c_str();
            const BOOL success = CreateProcessW(appPtr, cmdBuffer.data(), nullptr, nullptr, TRUE,
                CREATE_NO_WINDOW, nullptr, workDirStr.empty() ? nullptr : workDirStr.c_str(), &si, &pi);

            CloseHandle(hWritePipe);

            if (!success)
            {
                const DWORD err = GetLastError();
                AppendLog("Failed to launch process (Win32 error " + std::to_string(err) + ").\n");
                CloseHandle(hReadPipe);
                return false;
            }

            char buffer[1024];
            DWORD bytesRead = 0;
            while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0)
            {
                buffer[bytesRead] = '\0';
                AppendLog(std::string_view(buffer, bytesRead));
            }

            WaitForSingleObject(pi.hProcess, INFINITE);
            DWORD procExit = 1;
            GetExitCodeProcess(pi.hProcess, &procExit);

            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            CloseHandle(hReadPipe);

            exitCode = static_cast<int>(procExit);
            return true;
#else
            exitCode = -1;
            return false;
#endif
        }

        void ExecuteBuildPipeline(const std::string& pluginId,
            const std::filesystem::path& sourceDir,
            const std::filesystem::path& outputBinDir,
            const std::filesystem::path& editorBuildDir,
            const std::filesystem::path& expectedDllName,
            const std::filesystem::path& cmakeExe)
        {
            PluginBuildResult res;
            res.PluginId = pluginId;
            res.SourceDir = sourceDir;
            res.OutputBinDir = outputBinDir;

            AppendLog("=== [PluginSourceBuilder] Starting Build: " + pluginId + " ===\n");
            AppendLog("Source: " + sourceDir.string() + "\n");
            AppendLog("Target Binaries: " + outputBinDir.string() + "\n");

            std::error_code ec;
            const auto stagingDir = sourceDir / "build_stage";
            std::filesystem::create_directories(stagingDir, ec);

#ifdef _DEBUG
            const std::wstring config = L"Debug";
#else
            const std::wstring config = L"Release";
#endif

            const std::string generator = editorBuildDir.empty()
                ? std::string{}
                : ReadCMakeCacheValue(editorBuildDir / "CMakeCache.txt", "CMAKE_GENERATOR:INTERNAL=");
            const bool isVisualStudio = generator.empty() || generator.rfind("Visual Studio", 0) == 0;

            if (!generator.empty())
            {
                AppendLog("Editor Generator: " + generator + "\n");
                const auto stageCache = stagingDir / "CMakeCache.txt";
                if (std::filesystem::is_regular_file(stageCache, ec))
                {
                    const auto stageGen = ReadCMakeCacheValue(stageCache, "CMAKE_GENERATOR:INTERNAL=");
                    if (!stageGen.empty() && stageGen != generator)
                    {
                        AppendLog("Generator changed (" + stageGen + " -> " + generator + "); resetting CMake cache.\n");
                        std::filesystem::remove(stageCache, ec);
                        std::filesystem::remove_all(stagingDir / "CMakeFiles", ec);
                    }
                }
            }

            std::wstring devEnv;
            if (!isVisualStudio && !HasMsvcEnvironment())
            {
                devEnv = FindDevEnvironmentCommand(editorBuildDir);
                if (devEnv.empty())
                {
                    res.Success = false;
                    res.ErrorMessage = "MSVC environment (INCLUDE) is missing and VsDevCmd.bat could not be found in the editor build tree.";
                    AppendLog("\n[ERROR] " + res.ErrorMessage + "\n");
                    Finalize(res);
                    return;
                }
            }

            const auto wrapCommand = [&](const std::wstring& rawCmd, std::wstring& outApp, std::wstring& outCmd)
            {
                if (!devEnv.empty())
                {
                    outApp = GetCommandProcessorPath();
                    outCmd = L"\"" + outApp + L"\" /d /s /c \"" + devEnv + L" && " + rawCmd + L"\"";
                }
                else
                {
                    outApp.clear();
                    outCmd = rawCmd;
                }
            };

            // Find vcpkg_installed for ImGui package discovery
            const auto vcpkgDir = FindVcpkgInstalled(sourceDir, editorBuildDir);
            std::wstring extraConfigArgs;
            if (!vcpkgDir.empty())
            {
                const auto triplet = vcpkgDir / "x64-windows";
                extraConfigArgs += L" \"-DCMAKE_PREFIX_PATH=" + triplet.wstring() + L"\"";
                extraConfigArgs += L" \"-DVCPKG_INSTALLED_DIR=" + vcpkgDir.wstring() + L"\"";
                extraConfigArgs += L" \"-DVCPKG_TARGET_TRIPLET=x64-windows\"";
            }

            // 1. CMake Configure
            AppendLog("\n--- [CMake Configure] ---\n");
            std::wstring rawConfigCmd = L"\"" + cmakeExe.wstring() + L"\" -S \"" + sourceDir.wstring() +
                L"\" -B \"" + stagingDir.wstring() + L"\"";
            if (!generator.empty())
                rawConfigCmd += L" -G \"" + Utf8ToWide(generator) + L"\"";
            if (isVisualStudio)
                rawConfigCmd += L" -A x64";
            rawConfigCmd += L" \"-DCMAKE_BUILD_TYPE=" + config + L"\"" + extraConfigArgs;

            std::wstring configApp, configCmd;
            wrapCommand(rawConfigCmd, configApp, configCmd);

            int configExit = -1;
            if (!RunProcess(configApp, configCmd, sourceDir, configExit) || configExit != 0)
            {
                res.Success = false;
                res.ExitCode = configExit;
                res.ErrorMessage = "CMake configure failed with exit code " + std::to_string(configExit);
                AppendLog("\n[ERROR] " + res.ErrorMessage + "\n");
                Finalize(res);
                return;
            }

            // 2. CMake Build
            AppendLog("\n--- [CMake Build] ---\n");
            std::wstring rawBuildCmd = L"\"" + cmakeExe.wstring() + L"\" --build \"" + stagingDir.wstring() +
                L"\" --config " + config + L" --parallel";

            std::wstring buildApp, buildCmd;
            wrapCommand(rawBuildCmd, buildApp, buildCmd);

            int buildExit = -1;
            if (!RunProcess(buildApp, buildCmd, stagingDir, buildExit) || buildExit != 0)
            {
                res.Success = false;
                res.ExitCode = buildExit;
                res.ErrorMessage = "CMake build failed with exit code " + std::to_string(buildExit);
                AppendLog("\n[ERROR] " + res.ErrorMessage + "\n");
                Finalize(res);
                return;
            }

            // 3. Find and Stage Output DLL
            AppendLog("\n--- [Staging & Publish] ---\n");
            std::filesystem::path foundDll;
            std::filesystem::path fallbackDll;

            auto checkDir = [&](const std::filesystem::path& dir) {
                if (!std::filesystem::exists(dir, ec)) return;
                for (auto it = std::filesystem::recursive_directory_iterator(dir, std::filesystem::directory_options::skip_permission_denied, ec);
                     !ec && it != std::filesystem::recursive_directory_iterator(); it.increment(ec))
                {
                    if (it->is_directory(ec) && it->path().filename() == "CMakeFiles")
                    {
                        it.disable_recursion_pending();
                        continue;
                    }
                    if (it->is_regular_file(ec) && it->path().extension() == ".dll")
                    {
                        if (!expectedDllName.empty() && it->path().filename() == expectedDllName)
                        {
                            foundDll = it->path();
                            return;
                        }
                        if (fallbackDll.empty())
                            fallbackDll = it->path();
                    }
                }
            };

            checkDir(stagingDir);
            if (foundDll.empty())
                checkDir(sourceDir / "Binaries");
            if (foundDll.empty())
                foundDll = fallbackDll;

            if (foundDll.empty())
            {
                res.Success = false;
                res.ErrorMessage = "No output DLL found in staging directory: " + stagingDir.string();
                AppendLog("\n[ERROR] " + res.ErrorMessage + "\n");
                Finalize(res);
                return;
            }

            res.StagedDll = foundDll;
            AppendLog("Found built DLL: " + foundDll.string() + "\n");

            // Publish atomically into outputBinDir
            std::filesystem::create_directories(outputBinDir, ec);
            const auto targetFileName = expectedDllName.empty() ? foundDll.filename() : expectedDllName;
            const auto publishedDll = outputBinDir / targetFileName;

#if defined(_WIN32)
            if (foundDll != publishedDll && !std::filesystem::equivalent(foundDll, publishedDll, ec))
            {
                if (std::filesystem::exists(publishedDll, ec))
                {
                    const auto oldName = publishedDll.stem().wstring() + L"_old_" + std::to_wstring(::GetTickCount64()) + publishedDll.extension().wstring();
                    const auto oldPath = outputBinDir / oldName;
                    std::filesystem::rename(publishedDll, oldPath, ec);
                }

                BOOL moveOk = ::MoveFileExW(foundDll.c_str(), publishedDll.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED);
                if (!moveOk)
                {
                    if (!std::filesystem::copy_file(foundDll, publishedDll, std::filesystem::copy_options::overwrite_existing, ec))
                    {
                        res.Success = false;
                        res.ErrorMessage = "Failed to publish DLL to " + publishedDll.string();
                        AppendLog("\n[ERROR] " + res.ErrorMessage + "\n");
                        Finalize(res);
                        return;
                    }
                }

                // Also copy .pdb if present
                const auto foundPdb = foundDll.parent_path() / (foundDll.stem().wstring() + L".pdb");
                if (std::filesystem::exists(foundPdb, ec))
                {
                    const auto publishedPdb = outputBinDir / (publishedDll.stem().wstring() + L".pdb");
                    if (std::filesystem::exists(publishedPdb, ec))
                    {
                        const auto oldPdbName = publishedPdb.stem().wstring() + L"_old_" + std::to_wstring(::GetTickCount64()) + L".pdb";
                        std::filesystem::rename(publishedPdb, outputBinDir / oldPdbName, ec);
                    }
                    ::MoveFileExW(foundPdb.c_str(), publishedPdb.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED);
                }
            }
#endif

            res.PublishedDll = publishedDll;
            res.Success = true;
            AppendLog("Successfully published DLL: " + publishedDll.string() + "\n");
            AppendLog("=== [PluginSourceBuilder] Build Succeeded ===\n");
            Finalize(res);
        }

        void Finalize(PluginBuildResult& res)
        {
            {
                const std::lock_guard lock(m_LogMutex);
                res.Logs = m_Logs;
            }
            {
                const std::lock_guard lock(m_ResultMutex);
                m_LastResult = res;
            }
            m_HasNewResult.store(true, std::memory_order_release);
            m_IsBuilding.store(false, std::memory_order_release);
        }

    private:
        std::thread m_WorkerThread;
        std::atomic<bool> m_IsBuilding{ false };
        std::atomic<bool> m_HasNewResult{ false };

        mutable std::mutex m_LogMutex;
        std::string m_Logs;

        mutable std::mutex m_ResultMutex;
        PluginBuildResult m_LastResult;
    };
}
