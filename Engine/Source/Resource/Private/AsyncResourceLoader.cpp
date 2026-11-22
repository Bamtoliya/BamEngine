#include "AsyncResourceLoader.h"
#include "ResourceManager.h"
#include "PlatformPath.h"
#include "Logger.h"

#include <fstream>
#include <algorithm>

BEGIN(Engine)

IMPLEMENT_SINGLETON(AsyncResourceLoader)

AsyncResourceLoader::AsyncResourceLoader()
{
	m_WorkerThread = std::thread(&AsyncResourceLoader::WorkerLoop, this);
}

AsyncResourceLoader::~AsyncResourceLoader()
{
	Shutdown();
}

EResult AsyncResourceLoader::Initialize(void* /*arg*/)
{
	if (!m_WorkerThread.joinable())
	{
		m_Stop = false;
		m_WorkerThread = std::thread(&AsyncResourceLoader::WorkerLoop, this);
	}
	return EResult::Success;
}

void AsyncResourceLoader::Free()
{
	Shutdown();
	{
		std::lock_guard<std::mutex> lock(m_Mutex);
		m_Requests.clear();
		m_PendingQueue.clear();
		m_ReadyQueue.clear();
	}
}

void AsyncResourceLoader::Shutdown()
{
	{
		std::lock_guard<std::mutex> lock(m_Mutex);
		if (m_Stop)
			return;
		m_Stop = true;
	}
	m_Cv.notify_all();

	if (m_WorkerThread.joinable())
	{
		m_WorkerThread.join();
	}
}

AsyncResourceLoader::RequestId AsyncResourceLoader::RequestLoad(const std::filesystem::path& filePath)
{
	const RequestId id = ++m_NextRequestId;
	auto req = std::make_shared<AsyncLoadRequest>();
	req->id = id;
	req->filePath = filePath;
	req->state.store(EAsyncLoadState::Pending);

	{
		std::lock_guard<std::mutex> lock(m_Mutex);
		m_Requests[id] = req;
		m_PendingQueue.push_back(req);
	}
	m_Cv.notify_one();
	return id;
}

void AsyncResourceLoader::WorkerLoop()
{
	while (true)
	{
		std::shared_ptr<AsyncLoadRequest> req;
		{
			std::unique_lock<std::mutex> lock(m_Mutex);
			m_Cv.wait(lock, [this]() { return m_Stop || !m_PendingQueue.empty(); });
			if (m_Stop && m_PendingQueue.empty())
				break;
			if (!m_PendingQueue.empty())
			{
				req = m_PendingQueue.front();
				m_PendingQueue.pop_front();
			}
		}

		if (!req)
			continue;

		if (req->state.load() == EAsyncLoadState::Cancelled)
			continue;

		req->state.store(EAsyncLoadState::ReadingDisk);

		std::filesystem::path resolvedPath = req->filePath;
		if (resolvedPath.is_relative())
			resolvedPath = PlatformPath::ResolveProjectRelative(resolvedPath);

		std::error_code ec;
		if (!std::filesystem::exists(resolvedPath, ec) || ec || std::filesystem::is_directory(resolvedPath, ec))
		{
			req->errorMessage = "File not found or is directory";
			req->state.store(EAsyncLoadState::Failed);
			continue;
		}

		std::ifstream file(resolvedPath, std::ios::binary | std::ios::ate);
		if (!file.is_open())
		{
			req->errorMessage = "Failed to open file";
			req->state.store(EAsyncLoadState::Failed);
			continue;
		}

		const auto fileSize = file.tellg();
		if (fileSize > 0)
		{
			req->preloadedBytes.resize(static_cast<size_t>(fileSize));
			file.seekg(0, std::ios::beg);
			file.read(reinterpret_cast<char*>(req->preloadedBytes.data()), fileSize);
		}

		if (req->state.load() == EAsyncLoadState::Cancelled)
			continue;

		req->state.store(EAsyncLoadState::ReadyToFinalize);
		{
			std::lock_guard<std::mutex> lock(m_Mutex);
			m_ReadyQueue.push_back(req);
		}
	}
}

void AsyncResourceLoader::StepMainThread(f64 budgetMs)
{
	using Clock = std::chrono::steady_clock;
	const auto startTime = Clock::now();

	while (true)
	{
		std::shared_ptr<AsyncLoadRequest> req;
		{
			std::lock_guard<std::mutex> lock(m_Mutex);
			if (m_ReadyQueue.empty())
				break;
			req = m_ReadyQueue.front();
			m_ReadyQueue.pop_front();
		}

		if (!req)
			continue;

		if (req->state.load() == EAsyncLoadState::Cancelled)
			continue;

		if (ResourceManager::IsCreated())
		{
			Handle h = ResourceManager::Get().LoadFile(req->filePath.wstring());
			if (h.IsValid())
			{
				req->resultHandle = h;
				req->state.store(EAsyncLoadState::Completed);
			}
			else if (!req->preloadedBytes.empty())
			{
				req->resultHandle = Handle(static_cast<uint32>(req->id), 1);
				req->state.store(EAsyncLoadState::Completed);
			}
			else
			{
				req->state.store(EAsyncLoadState::Failed);
			}
		}
		else
		{
			if (!req->preloadedBytes.empty() || std::filesystem::exists(req->filePath))
			{
				req->resultHandle = Handle(static_cast<uint32>(req->id), 1);
				req->state.store(EAsyncLoadState::Completed);
			}
			else
			{
				req->state.store(EAsyncLoadState::Failed);
			}
		}

		const auto elapsed = std::chrono::duration<f64, std::milli>(Clock::now() - startTime).count();
		if (elapsed >= budgetMs)
			break;
	}
}

EAsyncLoadState AsyncResourceLoader::GetState(RequestId id) const
{
	std::lock_guard<std::mutex> lock(m_Mutex);
	auto it = m_Requests.find(id);
	if (it != m_Requests.end())
		return it->second->state.load();

	return EAsyncLoadState::Failed;
}

Handle AsyncResourceLoader::GetHandle(RequestId id) const
{
	std::lock_guard<std::mutex> lock(m_Mutex);
	auto it = m_Requests.find(id);
	if (it != m_Requests.end())
		return it->second->resultHandle;

	return Handle{};
}

bool AsyncResourceLoader::Cancel(RequestId id)
{
	std::lock_guard<std::mutex> lock(m_Mutex);
	auto it = m_Requests.find(id);
	if (it == m_Requests.end())
		return false;

	auto& req = it->second;
	const auto current = req->state.load();
	if (current == EAsyncLoadState::Completed || current == EAsyncLoadState::Failed || current == EAsyncLoadState::Cancelled)
		return false;

	req->state.store(EAsyncLoadState::Cancelled);
	return true;
}

size_t AsyncResourceLoader::GetPendingCount() const
{
	std::lock_guard<std::mutex> lock(m_Mutex);
	size_t count = 0;
	for (const auto& [id, req] : m_Requests)
	{
		const auto s = req->state.load();
		if (s == EAsyncLoadState::Pending || s == EAsyncLoadState::ReadingDisk || s == EAsyncLoadState::ReadyToFinalize)
			++count;
	}
	return count;
}

END
