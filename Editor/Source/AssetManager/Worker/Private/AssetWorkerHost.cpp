#include "AssetWorkerHost.h"
#include "Functions.h"
#include "PlatformPath.h"
#include <stb_image.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <memory>
#include <sstream>

IMPLEMENT_SINGLETON(AssetWorkerHost)

namespace
{
    bool DecodePngToRgba(const std::filesystem::path& pngPath, int32& outWidth, int32& outHeight, std::vector<uint8_t>& outRgba)
    {
        outWidth = 0;
        outHeight = 0;
        outRgba.clear();

        std::ifstream file(pngPath, std::ios::binary | std::ios::ate);
        if (!file.is_open()) return false;

        const std::streamsize size = file.tellg();
        if (size <= 0 || size > 2 * 1024 * 1024) return false;

        file.seekg(0, std::ios::beg);
        std::vector<uint8_t> encoded(static_cast<size_t>(size));
        if (!file.read(reinterpret_cast<char*>(encoded.data()), size)) return false;

        int w = 0, h = 0, c = 0;
        std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> pixels(
            stbi_load_from_memory(encoded.data(), static_cast<int>(size), &w, &h, &c, STBI_rgb_alpha), stbi_image_free);
        if (!pixels || w <= 0 || h <= 0) return false;

        outWidth = w;
        outHeight = h;
        outRgba.assign(pixels.get(), pixels.get() + static_cast<size_t>(w) * h * 4);
        return true;
    }

    std::string SanitizeField(std::string value)
    {
        for (char& ch : value)
        {
            if (ch == '\t' || ch == '\r' || ch == '\n') ch = ' ';
        }
        return value;
    }

    struct ModelSnapshotFile
    {
        std::filesystem::path Path;

        ModelSnapshotFile()
        {
            wchar_t tempDirectory[MAX_PATH]{};
            wchar_t tempFile[MAX_PATH]{};
            const DWORD length = GetTempPathW(MAX_PATH, tempDirectory);
            if (length > 0 && length < MAX_PATH && GetTempFileNameW(tempDirectory, L"BAM", 0, tempFile))
                Path = tempFile;
        }

        ~ModelSnapshotFile()
        {
            if (Path.empty()) return;
            std::error_code error;
            std::filesystem::remove(Path, error);
        }
    };

    bool CreateSiblingTempFile(const std::filesystem::path& targetPath, std::filesystem::path& outPath)
    {
        std::filesystem::path directory = targetPath.parent_path();
        if (directory.empty())
        {
            std::error_code error;
            directory = std::filesystem::current_path(error);
            if (error) return false;
        }

        std::wstring directoryString = directory.wstring();
        if (directoryString.empty()) return false;
        if (directoryString.back() != L'\\' && directoryString.back() != L'/')
            directoryString.push_back(L'\\');

        wchar_t tempFile[MAX_PATH]{};
        if (!GetTempFileNameW(directoryString.c_str(), L"BAM", 0, tempFile))
            return false;
        outPath = tempFile;
        return true;
    }

    struct TextureOutputFiles
    {
        std::filesystem::path BamtexPath;
        std::filesystem::path DdsPath;

        TextureOutputFiles(const std::filesystem::path& finalBamtexPath, const std::filesystem::path& finalDdsPath)
        {
            if (!CreateSiblingTempFile(finalBamtexPath, BamtexPath) ||
                !CreateSiblingTempFile(finalDdsPath, DdsPath))
            {
                Remove(BamtexPath);
                Remove(DdsPath);
                BamtexPath.clear();
                DdsPath.clear();
            }
        }

        ~TextureOutputFiles()
        {
            Remove(BamtexPath);
            Remove(DdsPath);
        }

    private:
        static void Remove(const std::filesystem::path& path)
        {
            if (path.empty()) return;
            std::error_code error;
            std::filesystem::remove(path, error);
        }
    };

    bool MoveTextureOutputFile(const std::filesystem::path& from, const std::filesystem::path& to, DWORD flags)
    {
        return MoveFileExW(from.c_str(), to.c_str(), flags) != FALSE;
    }

    bool CommitTextureOutputs(const std::filesystem::path& stagedBamtexPath,
        const std::filesystem::path& stagedDdsPath, const std::filesystem::path& finalBamtexPath,
        const std::filesystem::path& finalDdsPath)
    {
        std::error_code error;
        if (!std::filesystem::is_regular_file(stagedBamtexPath, error) || error ||
            !std::filesystem::is_regular_file(stagedDdsPath, error) || error)
            return false;

        std::filesystem::path bamtexBackupPath;
        std::filesystem::path ddsBackupPath;
        bool hasBamtexBackup = false;
        bool hasDdsBackup = false;

        const auto backupExisting = [&](const std::filesystem::path& finalPath,
            std::filesystem::path& backupPath, bool& hasBackup)
        {
            error.clear();
            if (!std::filesystem::exists(finalPath, error))
                return !error;
            if (!CreateSiblingTempFile(finalPath, backupPath))
                return false;
            DeleteFileW(backupPath.c_str());
            if (!MoveTextureOutputFile(finalPath, backupPath, MOVEFILE_WRITE_THROUGH))
                return false;
            hasBackup = true;
            return true;
        };

        if (!backupExisting(finalBamtexPath, bamtexBackupPath, hasBamtexBackup) ||
            !backupExisting(finalDdsPath, ddsBackupPath, hasDdsBackup))
        {
            const bool restoredBamtex = !hasBamtexBackup ||
                MoveTextureOutputFile(bamtexBackupPath, finalBamtexPath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
            const bool restoredDds = !hasDdsBackup ||
                MoveTextureOutputFile(ddsBackupPath, finalDdsPath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
            if (!restoredBamtex)
                BAM_LOG(Error, "Editor", "Asset", "Failed to restore texture output: {}",
                    Engine::WStrToStr(finalBamtexPath.wstring()));
            if (!restoredDds)
                BAM_LOG(Error, "Editor", "Asset", "Failed to restore texture output: {}",
                    Engine::WStrToStr(finalDdsPath.wstring()));
            if (restoredBamtex && !bamtexBackupPath.empty()) DeleteFileW(bamtexBackupPath.c_str());
            if (restoredDds && !ddsBackupPath.empty()) DeleteFileW(ddsBackupPath.c_str());
            return false;
        }

        const bool bamtexCommitted = MoveTextureOutputFile(stagedBamtexPath, finalBamtexPath,
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
        const bool ddsCommitted = bamtexCommitted && MoveTextureOutputFile(stagedDdsPath, finalDdsPath,
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
        if (!ddsCommitted)
        {
            if (bamtexCommitted) DeleteFileW(finalBamtexPath.c_str());
            const bool restoredBamtex = !hasBamtexBackup ||
                MoveTextureOutputFile(bamtexBackupPath, finalBamtexPath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
            const bool restoredDds = !hasDdsBackup ||
                MoveTextureOutputFile(ddsBackupPath, finalDdsPath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
            if (!restoredBamtex)
                BAM_LOG(Error, "Editor", "Asset", "Failed to restore texture output: {}",
                    Engine::WStrToStr(finalBamtexPath.wstring()));
            if (!restoredDds)
                BAM_LOG(Error, "Editor", "Asset", "Failed to restore texture output: {}",
                    Engine::WStrToStr(finalDdsPath.wstring()));
            if (restoredBamtex && !bamtexBackupPath.empty()) DeleteFileW(bamtexBackupPath.c_str());
            if (restoredDds && !ddsBackupPath.empty()) DeleteFileW(ddsBackupPath.c_str());
            return false;
        }

        if (hasBamtexBackup) DeleteFileW(bamtexBackupPath.c_str());
        if (hasDdsBackup) DeleteFileW(ddsBackupPath.c_str());
        return true;
    }

    std::string BuildModelCommand(Engine::uint64 id, const std::filesystem::path& source,
        const std::filesystem::path& snapshot, const Tools::ModelConversion::Options& options)
    {
        std::ostringstream stream;
        stream.imbue(std::locale::classic());
        stream << "IMPORT_MODEL\t" << id << '\t' << Engine::WStrToStr(source.wstring())
            << '\t' << Engine::WStrToStr(snapshot.wstring()) << '\t'
            << std::setprecision(std::numeric_limits<float>::max_digits10)
            << options.FlipUVs << ' ' << options.ConvertToLeftHanded << ' ' << options.UniformScale << ' '
            << options.FlipWindingOrder << ' ' << options.FlipPitch << ' ' << options.FlipHeading << ' '
            << options.GenerateLODs << ' ' << options.LODCount << ' ' << options.TargetError;
        for (float rate : options.LODReductionRates)
            stream << ' ' << rate;
        stream << '\n';
        return stream.str();
    }

    std::string BuildTextureCommand(Engine::uint64 id, const std::filesystem::path& sourcePath,
        const std::filesystem::path& finalBamtexPath, const std::filesystem::path& finalDdsPath,
        Engine::ETextureFormat targetFormat, ECompressFlags compressFlags, const TextureOutputFiles& outputs)
    {
        std::ostringstream stream;
        stream << "IMPORT_TEXTURE\t" << id << '\t' << Engine::WStrToStr(sourcePath.wstring())
            << '\t' << Engine::WStrToStr(finalBamtexPath.wstring())
            << '\t' << Engine::WStrToStr(finalDdsPath.wstring())
            << '\t' << Engine::WStrToStr(outputs.BamtexPath.wstring())
            << '\t' << Engine::WStrToStr(outputs.DdsPath.wstring())
            << '\t' << static_cast<uint32_t>(targetFormat)
            << '\t' << static_cast<uint32_t>(compressFlags) << '\n';
        return stream.str();
    }

    struct WorkerPipeProcess
    {
        HANDLE Process = nullptr;
        HANDLE Thread = nullptr;
        HANDLE StdInWrite = nullptr;
        HANDLE StdOutRead = nullptr;
        bool ModelImportSupported = false;
        bool TextureImportSupported = false;
        std::string ReadBuffer;

        ~WorkerPipeProcess()
        {
            Close();
        }

        void Close(bool sendQuit = false)
        {
            if (sendQuit && StdInWrite)
            {
                const char quitCmd[] = "QUIT\n";
                DWORD written = 0;
                (void)WriteFile(StdInWrite, quitCmd, sizeof(quitCmd) - 1, &written, nullptr);
            }
            if (StdInWrite) { CloseHandle(StdInWrite); StdInWrite = nullptr; }
            if (StdOutRead) { CloseHandle(StdOutRead); StdOutRead = nullptr; }
            if (Process)
            {
                if (!sendQuit || WaitForSingleObject(Process, 250) != WAIT_OBJECT_0)
                {
                    if (TerminateProcess(Process, 1))
                        WaitForSingleObject(Process, 1000);
                }
                CloseHandle(Process);
                Process = nullptr;
            }
            if (Thread) { CloseHandle(Thread); Thread = nullptr; }
            ModelImportSupported = false;
            TextureImportSupported = false;
            ReadBuffer.clear();
        }

        [[nodiscard]] bool IsAlive() const
        {
            return Process != nullptr && StdInWrite != nullptr && StdOutRead != nullptr;
        }
    };

    bool WriteAllToPipe(HANDLE pipe, const std::string& data)
    {
        size_t offset = 0;
        while (offset < data.size())
        {
            DWORD written = 0;
            const DWORD chunk = static_cast<DWORD>(data.size() - offset);
            if (!WriteFile(pipe, data.data() + offset, chunk, &written, nullptr) || written == 0)
                return false;
            offset += written;
        }
        return true;
    }

    bool ReadLineFromPipe(WorkerPipeProcess& proc, std::string& outLine,
        std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::time_point::max())
    {
        outLine.clear();
        for (;;)
        {
            const size_t nl = proc.ReadBuffer.find('\n');
            if (nl != std::string::npos)
            {
                outLine = proc.ReadBuffer.substr(0, nl);
                proc.ReadBuffer.erase(0, nl + 1);
                if (!outLine.empty() && outLine.back() == '\r') outLine.pop_back();
                return true;
            }

            if (std::chrono::steady_clock::now() >= deadline || proc.ReadBuffer.size() >= 64 * 1024)
                return false;

            DWORD available = 0;
            if (!PeekNamedPipe(proc.StdOutRead, nullptr, 0, nullptr, &available, nullptr))
                return false;
            if (available == 0)
            {
                if (WaitForSingleObject(proc.Process, 10) != WAIT_TIMEOUT)
                    return false;
                continue;
            }

            char buf[512];
            DWORD bytesRead = 0;
            if (!ReadFile(proc.StdOutRead, buf, (std::min)(available, static_cast<DWORD>(sizeof(buf))), &bytesRead, nullptr) ||
                bytesRead == 0)
                return false;
            proc.ReadBuffer.append(buf, bytesRead);
        }
    }

    bool SpawnWorkerProcess(const std::filesystem::path& exePath, const std::filesystem::path& projectRoot,
        HANDLE jobObject, WorkerPipeProcess& outProc)
    {
        // Serialize the lifetime of the inheritable pipe ends across worker launches.
        static std::mutex spawnMutex;
        std::lock_guard spawnLock(spawnMutex);
        outProc.Close();

        SECURITY_ATTRIBUTES sa{};
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;
        sa.lpSecurityDescriptor = nullptr;

        HANDLE childInRead = nullptr;
        HANDLE parentInWrite = nullptr;
        HANDLE parentOutRead = nullptr;
        HANDLE childOutWrite = nullptr;

        if (!CreatePipe(&childInRead, &parentInWrite, &sa, 0)) return false;
        if (!SetHandleInformation(parentInWrite, HANDLE_FLAG_INHERIT, 0))
        {
            CloseHandle(childInRead);
            CloseHandle(parentInWrite);
            return false;
        }

        if (!CreatePipe(&parentOutRead, &childOutWrite, &sa, 0))
        {
            CloseHandle(childInRead);
            CloseHandle(parentInWrite);
            return false;
        }
        if (!SetHandleInformation(parentOutRead, HANDLE_FLAG_INHERIT, 0))
        {
            CloseHandle(childInRead);
            CloseHandle(parentInWrite);
            CloseHandle(parentOutRead);
            CloseHandle(childOutWrite);
            return false;
        }

        HANDLE childErrNull = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
            &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

        std::wstring cmdLine = L"\"" + exePath.wstring() + L"\" --daemon";
        if (!projectRoot.empty()) cmdLine += L" --project-root \"" + projectRoot.wstring() + L"\"";

        STARTUPINFOW si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        si.hStdInput = childInRead;
        si.hStdOutput = childOutWrite;
        si.hStdError = childErrNull != INVALID_HANDLE_VALUE ? childErrNull : childOutWrite;

        PROCESS_INFORMATION pi{};
        const BOOL created = CreateProcessW(exePath.c_str(), cmdLine.data(), nullptr, nullptr, TRUE,
            CREATE_NO_WINDOW | BELOW_NORMAL_PRIORITY_CLASS, nullptr,
            projectRoot.empty() ? nullptr : projectRoot.c_str(), &si, &pi);

        CloseHandle(childInRead);
        CloseHandle(childOutWrite);
        if (childErrNull != INVALID_HANDLE_VALUE) CloseHandle(childErrNull);

        if (!created)
        {
            CloseHandle(parentInWrite);
            CloseHandle(parentOutRead);
            return false;
        }

        if (jobObject) (void)AssignProcessToJobObject(jobObject, pi.hProcess);

        outProc.Process = pi.hProcess;
        outProc.Thread = pi.hThread;
        outProc.StdInWrite = parentInWrite;
        outProc.StdOutRead = parentOutRead;
        return true;
    }
}

EResult AssetWorkerHost::Initialize(void* arg)
{
    wchar_t exeBuf[MAX_PATH]{};
    const DWORD len = GetModuleFileNameW(nullptr, exeBuf, MAX_PATH);
    if (len == 0 || len >= MAX_PATH)
    {
        m_Available = false;
        return EResult::Success;
    }

    const std::filesystem::path exeDir = std::filesystem::path(exeBuf).parent_path();
    m_WorkerExePath = exeDir / L"BamAssetWorker.exe";
    m_ProjectRoot = Engine::PlatformPath::GetProjectRoot();

    std::error_code ec;
    if (!std::filesystem::is_regular_file(m_WorkerExePath, ec))
    {
        BAM_LOG(Warn, "Editor", "Asset",
            "BamAssetWorker.exe not found at {}; using in-process fallback.",
            Engine::WStrToStr(m_WorkerExePath.wstring()));
        m_Available = false;
        return EResult::Success;
    }

    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    if (job)
    {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli{};
        jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli)))
        {
            CloseHandle(job);
            job = nullptr;
        }
    }
    m_JobObject = job;

    m_StopRequested = false;
    m_ActiveProcesses.assign(MAX_WORKERS, nullptr);
    m_Workers.reserve(MAX_WORKERS);
    for (size_t i = 0; i < MAX_WORKERS; ++i)
        m_Workers.emplace_back(&AssetWorkerHost::WorkerThreadLoop, this, i);

    m_Available = true;
    BAM_LOG(Info, "Editor", "Asset",
        "AssetWorkerHost started ({} workers): {}", MAX_WORKERS, Engine::WStrToStr(m_WorkerExePath.wstring()));
    return EResult::Success;
}

void AssetWorkerHost::Free()
{
    {
        std::lock_guard lock(m_QueueMutex);
        m_StopRequested = true;
        m_PendingQueue.clear();
        m_PendingImports.clear();
        m_PendingTextureImports.clear();
    }
    m_QueueCv.notify_all();

    {
        std::lock_guard lock(m_ActiveProcessMutex);
        for (void* rawProc : m_ActiveProcesses)
        {
            if (rawProc) TerminateProcess(static_cast<HANDLE>(rawProc), 0);
        }
    }

    for (auto& worker : m_Workers)
    {
        if (worker.joinable()) worker.join();
    }
    m_Workers.clear();
    m_ActiveProcesses.clear();

    if (m_JobObject)
    {
        CloseHandle(static_cast<HANDLE>(m_JobObject));
        m_JobObject = nullptr;
    }

    {
        std::lock_guard lock(m_CompletedMutex);
        m_CompletedTasks.clear();
    }

    m_Available = false;
}

bool AssetWorkerHost::EnqueueThumbnail(const std::string& assetKey, Engine::uint64 requestId,
    const std::filesystem::path& sourcePath, const std::filesystem::path& outputPngPath, int32 resolution)
{
    if (!m_Available || sourcePath.empty() || outputPngPath.empty()) return false;

    std::lock_guard lock(m_QueueMutex);
    if (m_StopRequested) return false;
    if (m_PendingQueue.size() >= MAX_QUEUE_SIZE) m_PendingQueue.pop_front();

    PendingThumbnailTask task;
    task.AssetKey = assetKey;
    task.RequestId = requestId;
    task.SourcePath = Engine::PlatformPath::ResolveProjectRelative(sourcePath);
    task.OutputPngPath = outputPngPath;
    task.Resolution = resolution;
    m_PendingQueue.push_back(std::move(task));

    m_QueueCv.notify_one();
    return true;
}

std::vector<AssetWorkerHost::CompletedThumbnailTask> AssetWorkerHost::PollCompletedTasks()
{
    std::vector<CompletedThumbnailTask> tasks;
    std::lock_guard lock(m_CompletedMutex);
    tasks.swap(m_CompletedTasks);
    return tasks;
}

bool AssetWorkerHost::EnqueueImportModel(const std::filesystem::path& sourcePath,
    Tools::ModelConversion::Options options, ModelImportCompletion complete)
{
    if (!m_Available || sourcePath.empty() || !complete) return false;
    if (sourcePath.native().find_first_of(L"\t\r\n") != std::wstring::npos) return false;

    std::lock_guard lock(m_QueueMutex);
    if (m_StopRequested || m_PendingImports.size() + m_PendingTextureImports.size() >= MAX_QUEUE_SIZE) return false;

    m_PendingImports.push_back({ m_NextImportRequestId++, sourcePath, std::move(options), std::move(complete) });
    m_QueueCv.notify_one();
    return true;
}

bool AssetWorkerHost::EnqueueImportTexture(const std::filesystem::path& sourcePath,
    const std::filesystem::path& finalBamtexPath, const std::filesystem::path& finalDdsPath,
    Engine::ETextureFormat targetFormat, ECompressFlags compressFlags, TextureImportCompletion complete)
{
    const auto hasProtocolDelimiter = [](const std::filesystem::path& path)
    {
        return path.native().find_first_of(L"\t\r\n") != std::wstring::npos;
    };
    if (!m_Available || sourcePath.empty() || finalBamtexPath.empty() || finalDdsPath.empty() || !complete ||
        hasProtocolDelimiter(sourcePath) || hasProtocolDelimiter(finalBamtexPath) || hasProtocolDelimiter(finalDdsPath))
        return false;

    std::lock_guard lock(m_QueueMutex);
    if (m_StopRequested || m_PendingImports.size() + m_PendingTextureImports.size() >= MAX_QUEUE_SIZE)
        return false;

    PendingTextureImportTask task;
    task.RequestId = m_NextImportRequestId++;
    task.SourcePath = sourcePath;
    task.FinalBamtexPath = finalBamtexPath;
    task.FinalDdsPath = finalDdsPath;
    task.TargetFormat = targetFormat;
    task.CompressFlags = compressFlags;
    task.Complete = std::move(complete);
    m_PendingTextureImports.push_back(std::move(task));
    m_QueueCv.notify_one();
    return true;
}

bool AssetWorkerHost::GetActiveImportStatus(ActiveImportStatus& outStatus)
{
    std::lock_guard lock(m_ImportStatusMutex);
    if (!m_ActiveImportProcess || m_ImportCancelled) return false;
    outStatus = m_ActiveImportStatus;
    return true;
}

void AssetWorkerHost::CancelActiveImport(Engine::uint64 requestId)
{
    std::lock_guard lock(m_ImportStatusMutex);
    if (!m_ActiveImportProcess || m_ActiveImportStatus.RequestId != requestId)
        return;

    if (TerminateProcess(static_cast<HANDLE>(m_ActiveImportProcess), 1))
        m_ImportCancelled = true;
}

void AssetWorkerHost::ClearPendingTasks()
{
    std::lock_guard lock(m_QueueMutex);
    m_PendingQueue.clear();
}

void AssetWorkerHost::WorkerThreadLoop(size_t workerIndex)
{
    WorkerPipeProcess proc;

    auto setActiveHandle = [&](HANDLE h)
        {
            std::lock_guard lock(m_ActiveProcessMutex);
            if (workerIndex < m_ActiveProcesses.size()) m_ActiveProcesses[workerIndex] = h;
        };

    const auto processImport = [&](const PendingImportTask& task)
    {
        using Clock = std::chrono::steady_clock;
        using Result = ModelImportResult;
        if (!proc.ModelImportSupported)
        {
            std::string response;
            const auto deadline = Clock::now() + std::chrono::seconds(2);
            if (WriteAllToPipe(proc.StdInWrite, "MODEL_WORKER_VERSION\t1\n"))
            {
                while (ReadLineFromPipe(proc, response, deadline))
                {
                    if (response == "MODEL_WORKER_VERSION\t1")
                    {
                        proc.ModelImportSupported = true;
                        break;
                    }
                }
            }
            if (!proc.ModelImportSupported)
            {
                setActiveHandle(nullptr);
                proc.Close();
                task.Complete(Result::Unavailable, {});
                return;
            }
        }

        ModelSnapshotFile snapshot;
        if (snapshot.Path.empty() || snapshot.Path.native().find_first_of(L"\t\r\n") != std::wstring::npos)
        {
            task.Complete(Result::Unavailable, {});
            return;
        }

        const std::string command = BuildModelCommand(task.RequestId, task.SourcePath, snapshot.Path, task.Options);
        {
            std::lock_guard lock(m_ImportStatusMutex);
            m_ActiveImportStatus = {
                task.RequestId, Engine::WStrToStr(task.SourcePath.filename().wstring()), 0.05f, "ModelImport.WorkerStarting"
            };
            m_ActiveImportProcess = proc.Process;
            m_ImportCancelled = false;
        }

        bool gotResult = false;
        bool success = false;
        std::string line;
        if (WriteAllToPipe(proc.StdInWrite, command))
        {
            constexpr auto timeout = std::chrono::minutes(3);
            auto deadline = Clock::now() + timeout;
            while (Clock::now() < deadline && ReadLineFromPipe(proc, line, deadline))
            {
                std::istringstream response(line);
                std::string type;
                Engine::uint64 requestId = 0;
                int value = 0;
                if (!(response >> type >> requestId >> value) || requestId != task.RequestId)
                    continue;

                if (type == "RESULT")
                {
                    success = value == 1;
                    gotResult = true;
                    break;
                }
                if (type == "PROGRESS" && (value == 1 || value == 2))
                {
                    deadline = Clock::now() + timeout;
                    std::lock_guard lock(m_ImportStatusMutex);
                    m_ActiveImportStatus.Progress = value == 1 ? 0.15f : 0.9f;
                    m_ActiveImportStatus.StepKey = value == 1
                        ? "ModelImport.WorkerConverting" : "ModelImport.WorkerReceiving";
                }
            }
        }

        bool cancelled;
        {
            std::lock_guard lock(m_ImportStatusMutex);
            cancelled = m_ImportCancelled;
            m_ActiveImportProcess = nullptr;
            m_ActiveImportStatus = {};
        }

        if (!gotResult || cancelled)
        {
            setActiveHandle(nullptr);
            proc.Close();
        }

        if (cancelled)
            task.Complete(Result::Cancelled, {});
        else if (success)
            task.Complete(Result::SnapshotReady, snapshot.Path);
        else
        {
            BAM_LOG(Error, "Editor", "Asset", "Model worker failed, disconnected, or timed out: {}",
                Engine::WStrToStr(task.SourcePath.wstring()));
            task.Complete(Result::Failed, {});
        }
    };

    const auto processTextureImport = [&](const PendingTextureImportTask& task)
    {
        using Clock = std::chrono::steady_clock;
        using Result = TextureImportResult;
        if (!proc.TextureImportSupported)
        {
            std::string response;
            const auto deadline = Clock::now() + std::chrono::seconds(2);
            if (WriteAllToPipe(proc.StdInWrite, "TEXTURE_WORKER_VERSION\t1\n"))
            {
                while (ReadLineFromPipe(proc, response, deadline))
                {
                    if (response == "TEXTURE_WORKER_VERSION\t1")
                    {
                        proc.TextureImportSupported = true;
                        break;
                    }
                }
            }
            if (!proc.TextureImportSupported)
            {
                setActiveHandle(nullptr);
                proc.Close();
                task.Complete(Result::Unavailable);
                return;
            }
        }

        TextureOutputFiles outputs(task.FinalBamtexPath, task.FinalDdsPath);
        if (outputs.BamtexPath.empty() || outputs.DdsPath.empty())
        {
            task.Complete(Result::Unavailable);
            return;
        }

        const std::string command = BuildTextureCommand(task.RequestId, task.SourcePath,
            task.FinalBamtexPath, task.FinalDdsPath, task.TargetFormat, task.CompressFlags, outputs);
        {
            std::lock_guard lock(m_ImportStatusMutex);
            m_ActiveImportStatus = {
                task.RequestId, Engine::WStrToStr(task.SourcePath.filename().wstring()), 0.05f,
                "ModelImport.TextureWorkerStarting"
            };
            m_ActiveImportProcess = proc.Process;
            m_ImportCancelled = false;
        }

        bool gotResult = false;
        bool success = false;
        std::string line;
        if (WriteAllToPipe(proc.StdInWrite, command))
        {
            constexpr auto timeout = std::chrono::minutes(3);
            auto deadline = Clock::now() + timeout;
            while (Clock::now() < deadline && ReadLineFromPipe(proc, line, deadline))
            {
                std::istringstream response(line);
                std::string type;
                Engine::uint64 requestId = 0;
                int value = 0;
                if (!(response >> type >> requestId >> value) || requestId != task.RequestId)
                    continue;

                if (type == "RESULT")
                {
                    success = value == 1;
                    gotResult = true;
                    break;
                }
                if (type == "PROGRESS" && value >= 1 && value <= 4)
                {
                    deadline = Clock::now() + timeout;
                    std::lock_guard lock(m_ImportStatusMutex);
                    m_ActiveImportStatus.Progress = value == 1 ? 0.15f : value == 2 ? 0.45f : value == 3 ? 0.7f : 0.9f;
                    m_ActiveImportStatus.StepKey = value == 1 ? "ModelImport.TextureWorkerCompressing"
                        : value == 2 ? "ModelImport.TextureWorkerWritingBamtex"
                        : value == 3 ? "ModelImport.TextureWorkerWritingDds"
                        : "ModelImport.TextureWorkerFinalizing";
                }
            }
        }

        bool cancelled;
        {
            std::lock_guard lock(m_ImportStatusMutex);
            cancelled = m_ImportCancelled;
            m_ActiveImportProcess = nullptr;
            m_ActiveImportStatus = {};
        }

        if (!gotResult || cancelled)
        {
            setActiveHandle(nullptr);
            proc.Close();
        }

        if (cancelled)
            task.Complete(Result::Cancelled);
        else if (success && CommitTextureOutputs(outputs.BamtexPath, outputs.DdsPath,
            task.FinalBamtexPath, task.FinalDdsPath))
            task.Complete(Result::OutputReady);
        else
        {
            BAM_LOG(Error, "Editor", "Asset", "Texture worker failed, disconnected, timed out, or could not commit outputs: {}",
                Engine::WStrToStr(task.SourcePath.wstring()));
            task.Complete(Result::Failed);
        }
    };

    for (;;)
    {
        bool isImportTask = false;
        bool isTextureTask = false;
        PendingImportTask importTask;
        PendingTextureImportTask textureTask;
        PendingThumbnailTask thumbTask;

        {
            std::unique_lock lock(m_QueueMutex);
            m_QueueCv.wait(lock, [&]() {
                return m_StopRequested || (!m_ImportInProgress &&
                    (!m_PendingImports.empty() || !m_PendingTextureImports.empty())) || !m_PendingQueue.empty();
            });
            if (m_StopRequested) break;

            if (!m_ImportInProgress && !m_PendingImports.empty())
            {
                isImportTask = m_ImportInProgress = true;
                importTask = std::move(m_PendingImports.front());
                m_PendingImports.pop_front();
            }
            else if (!m_ImportInProgress && !m_PendingTextureImports.empty())
            {
                isTextureTask = m_ImportInProgress = true;
                textureTask = std::move(m_PendingTextureImports.front());
                m_PendingTextureImports.pop_front();
            }
            else
            {
                thumbTask = std::move(m_PendingQueue.back());
                m_PendingQueue.pop_back();
            }
        }

        if (!proc.IsAlive() &&
            SpawnWorkerProcess(m_WorkerExePath, m_ProjectRoot, static_cast<HANDLE>(m_JobObject), proc))
        {
            std::lock_guard lock(m_QueueMutex);
            if (m_StopRequested) break;
            setActiveHandle(proc.Process);
        }

        if (isImportTask)
        {
            if (proc.IsAlive())
                processImport(importTask);
            else
                importTask.Complete(ModelImportResult::Unavailable, {});

            {
                std::lock_guard lock(m_QueueMutex);
                m_ImportInProgress = false;
            }
            m_QueueCv.notify_one();
            continue;
        }

        if (isTextureTask)
        {
            if (proc.IsAlive())
                processTextureImport(textureTask);
            else
                textureTask.Complete(TextureImportResult::Unavailable);

            {
                std::lock_guard lock(m_QueueMutex);
                m_ImportInProgress = false;
            }
            m_QueueCv.notify_one();
            continue;
        }

        bool success = false;
        bool gotResult = false;
        if (proc.IsAlive())
        {
            const std::string srcUtf8 = SanitizeField(Engine::WStrToStr(thumbTask.SourcePath.wstring()));
            const std::string dstUtf8 = SanitizeField(Engine::WStrToStr(thumbTask.OutputPngPath.wstring()));
            const std::string cmd = "THUMB\t" + std::to_string(thumbTask.RequestId) + "\t" + std::to_string(thumbTask.Resolution) +
                "\t" + srcUtf8 + "\t" + dstUtf8 + "\n";

            if (WriteAllToPipe(proc.StdInWrite, cmd))
            {
                std::string line;
                while (ReadLineFromPipe(proc, line))
                {
                    std::istringstream response(line);
                    std::string type;
                    Engine::uint64 requestId = 0;
                    int value = 0;
                    if ((response >> type >> requestId >> value) && type == "RESULT" && requestId == thumbTask.RequestId)
                    {
                        success = value == 1;
                        gotResult = true;
                        break;
                    }
                }
            }
            if (!gotResult)
            {
                setActiveHandle(nullptr);
                proc.Close();
            }
        }

        int32 width = 0, height = 0;
        std::vector<uint8_t> rgbaData;
        if (success && !DecodePngToRgba(thumbTask.OutputPngPath, width, height, rgbaData))
            success = false;

        std::lock_guard lock(m_CompletedMutex);
        m_CompletedTasks.push_back({ std::move(thumbTask.AssetKey), thumbTask.RequestId, std::move(thumbTask.OutputPngPath),
            width, height, std::move(rgbaData), success });
    }

    setActiveHandle(nullptr);
    proc.Close(true);
}
