#pragma once

#include <atomic>
#include <chrono>
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
            const std::filesystem::path exe = cmakeExe.empty() ? FindDefaultCMakeExe() : cmakeExe;

            m_WorkerThread = std::thread([this, idStr, src, bin, exe]()
            {
                ExecuteBuildPipeline(idStr, src, bin, exe);
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

        void AppendLog(std::string_view text)
        {
            const std::lock_guard lock(m_LogMutex);
            m_Logs.append(text);
        }

        static std::filesystem::path FindVcpkgInstalled(const std::filesystem::path& sourceDir)
        {
            std::error_code ec;
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

        bool RunProcess(const std::wstring& cmd, const std::filesystem::path& workingDir, int& exitCode)
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
            const BOOL success = CreateProcessW(nullptr, cmdBuffer.data(), nullptr, nullptr, TRUE,
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

            // Find vcpkg_installed for ImGui package discovery
            const auto vcpkgDir = FindVcpkgInstalled(sourceDir);
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
            std::wstring configCmd = L"\"" + cmakeExe.wstring() + L"\" -S \"" + sourceDir.wstring() +
                L"\" -B \"" + stagingDir.wstring() + L"\" -A x64" +
                L" \"-DCMAKE_BUILD_TYPE=" + config + L"\"" + extraConfigArgs;

            int configExit = -1;
            if (!RunProcess(configCmd, sourceDir, configExit) || configExit != 0)
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
            std::wstring buildCmd = L"\"" + cmakeExe.wstring() + L"\" --build \"" + stagingDir.wstring() +
                L"\" --config " + config + L" --parallel";

            int buildExit = -1;
            if (!RunProcess(buildCmd, stagingDir, buildExit) || buildExit != 0)
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

            auto checkDir = [&](const std::filesystem::path& dir) {
                if (!std::filesystem::exists(dir, ec)) return;
                for (const auto& entry : std::filesystem::recursive_directory_iterator(dir, std::filesystem::directory_options::skip_permission_denied, ec))
                {
                    if (entry.is_regular_file(ec) && entry.path().extension() == ".dll")
                    {
                        foundDll = entry.path();
                        break;
                    }
                }
            };

            checkDir(stagingDir);
            if (foundDll.empty())
            {
                checkDir(sourceDir / "Binaries");
            }

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
            const auto publishedDll = outputBinDir / foundDll.filename();

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

        void Finalize(const PluginBuildResult& res)
        {
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
