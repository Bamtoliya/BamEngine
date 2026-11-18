#include "AssetWorkerHost.h"
#include "Functions.h"
#include "PlatformPath.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <cstdlib>
#include <sstream>

IMPLEMENT_SINGLETON(AssetWorkerHost)

namespace
{
    std::string SanitizeField(std::string value)
    {
        for (char& ch : value)
        {
            if (ch == '\t' || ch == '\r' || ch == '\n') ch = ' ';
        }
        return value;
    }

    struct WorkerPipeProcess
    {
        HANDLE Process = nullptr;
        HANDLE Thread = nullptr;
        HANDLE StdInWrite = nullptr;
        HANDLE StdOutRead = nullptr;
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
                    TerminateProcess(Process, 0);
                CloseHandle(Process);
                Process = nullptr;
            }
            if (Thread) { CloseHandle(Thread); Thread = nullptr; }
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

    bool ReadLineFromPipe(WorkerPipeProcess& proc, std::string& outLine)
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

            char buf[512];
            DWORD bytesRead = 0;
            if (!ReadFile(proc.StdOutRead, buf, sizeof(buf), &bytesRead, nullptr) || bytesRead == 0)
                return false;
            proc.ReadBuffer.append(buf, bytesRead);
        }
    }

    bool SpawnWorkerProcess(const std::filesystem::path& exePath, const std::filesystem::path& projectRoot,
        HANDLE jobObject, WorkerPipeProcess& outProc)
    {
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
    if (m_StopRequested || m_PendingQueue.size() >= MAX_QUEUE_SIZE) return false;

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

    for (;;)
    {
        PendingThumbnailTask task;
        {
            std::unique_lock lock(m_QueueMutex);
            m_QueueCv.wait(lock, [&]() { return m_StopRequested || !m_PendingQueue.empty(); });
            if (m_StopRequested) break;
            task = std::move(m_PendingQueue.front());
            m_PendingQueue.pop_front();
        }

        if (!proc.IsAlive())
        {
            if (!SpawnWorkerProcess(m_WorkerExePath, m_ProjectRoot, static_cast<HANDLE>(m_JobObject), proc))
            {
                std::lock_guard lock(m_CompletedMutex);
                m_CompletedTasks.push_back({ std::move(task.AssetKey), task.RequestId, std::move(task.OutputPngPath), false });
                continue;
            }
            setActiveHandle(proc.Process);
        }

        const std::string srcUtf8 = SanitizeField(Engine::WStrToStr(task.SourcePath.wstring()));
        const std::string dstUtf8 = SanitizeField(Engine::WStrToStr(task.OutputPngPath.wstring()));
        const std::string cmd = "THUMB\t" + std::to_string(task.RequestId) + "\t" + std::to_string(task.Resolution) +
            "\t" + srcUtf8 + "\t" + dstUtf8 + "\n";

        bool success = false;
        bool gotResult = false;
        if (WriteAllToPipe(proc.StdInWrite, cmd))
        {
            std::string line;
            while (ReadLineFromPipe(proc, line))
            {
                if (line.rfind("RESULT\t", 0) == 0)
                {
                    std::stringstream ss(line);
                    std::string tag, idStr, okStr;
                    std::getline(ss, tag, '\t');
                    std::getline(ss, idStr, '\t');
                    std::getline(ss, okStr, '\t');
                    const Engine::uint64 respId = std::strtoull(idStr.c_str(), nullptr, 10);
                    if (respId == task.RequestId)
                    {
                        success = (okStr == "1");
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
        else
        {
            setActiveHandle(nullptr);
            proc.Close();
        }

        {
            std::lock_guard lock(m_CompletedMutex);
            m_CompletedTasks.push_back({ std::move(task.AssetKey), task.RequestId, std::move(task.OutputPngPath), success });
        }
    }

    setActiveHandle(nullptr);
    proc.Close(true);
}
