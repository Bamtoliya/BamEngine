#include "AsyncResourceLoader.h"
#include "ResourceManager.h"
#include "PlatformPath.h"
#include "PakArchive.h"
#include "Logger.h"

#include <algorithm>

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
	if (std::this_thread::get_id() != m_OwnerThread)
		return 0;

	const RequestId id = ++m_NextRequestId;
	auto req = std::make_shared<AsyncLoadRequest>();
	req->id = id;
	req->filePath = filePath;
	req->state.store(EAsyncLoadState::Pending);

	{
		std::lock_guard<std::mutex> lock(m_Mutex);
		if (m_Stop)
			return 0;
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

			m_Cv.wait(lock, [this]()
				{
					return m_Stop || !m_PendingQueue.empty();
				});

			if (m_Stop && m_PendingQueue.empty())
				break;

			req = m_PendingQueue.front();
			m_PendingQueue.pop_front();

			if (!req || req->state.load() == EAsyncLoadState::Cancelled)
				continue;

			req->state.store(EAsyncLoadState::ReadingDisk);
			m_ReadInProgress = true;
		}

		bool succeeded = false;

		try
		{
			auto resolvedPath = req->filePath;

			if (!PlatformPath::IsVirtualUri(resolvedPath.wstring()) &&
				resolvedPath.is_relative())
			{
				resolvedPath = PlatformPath::ResolveProjectRelative(resolvedPath);
			}

			if (!PakMountManager::Get().ExistsOrOnDisk(resolvedPath))
			{
				req->errorMessage = "File not found or is directory";
			}
			else
			{
				succeeded = PakMountManager::Get().ReadBytesWithFallback(
					resolvedPath, req->preloadedBytes) == EResult::Success;

				if (!succeeded)
					req->errorMessage = "Failed to open file";
			}
		}
		catch (const std::exception& error)
		{
			req->errorMessage = error.what();
		}

		{
			std::lock_guard<std::mutex> lock(m_Mutex);

			if (req->state.load() != EAsyncLoadState::Cancelled)
			{
				if (succeeded)
				{
					m_ReadyQueue.push_back(req);
					req->state.store(EAsyncLoadState::ReadyToFinalize);
				}
				else
				{
					req->state.store(EAsyncLoadState::Failed);
				}
			}

			m_ReadInProgress = false;
		}

		m_Cv.notify_all();
	}
}

void AsyncResourceLoader::StepMainThread(f64 budgetMs)
{
	if (std::this_thread::get_id() != m_OwnerThread)
		return;

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

		Handle resultHandle;

		if (ResourceManager::IsCreated())
		{
			auto& manager = ResourceManager::Get();

			if (manager.IsOwnerThread())
			{
				const Handle loaded = manager.LoadFile(req->filePath.wstring());

				if (manager.IsValid(loaded))
					resultHandle = loaded;
			}
		}

		{
			std::lock_guard<std::mutex> lock(m_Mutex);

			if (req->state.load() != EAsyncLoadState::Cancelled)
			{
				req->resultHandle = resultHandle;

				req->state.store(resultHandle.IsValid()
					? EAsyncLoadState::Completed
					: EAsyncLoadState::Failed);
			}
		}

		const auto elapsed = std::chrono::duration<f64, std::milli>(
			Clock::now() - startTime).count();

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

	const auto it = m_Requests.find(id);

	if (it == m_Requests.end())
		return {};

	if (it->second->state.load() != EAsyncLoadState::Completed)
		return {};

	return it->second->resultHandle;
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

bool AsyncResourceLoader::CancelAllAndWaitIdle(std::chrono::milliseconds timeout)
{
	if (std::this_thread::get_id() != m_OwnerThread)
		return false;

	std::unique_lock<std::mutex> lock(m_Mutex);

	for (auto& [id, req] : m_Requests)
	{
		const auto state = req->state.load();

		if (state == EAsyncLoadState::Pending ||
			state == EAsyncLoadState::ReadingDisk ||
			state == EAsyncLoadState::ReadyToFinalize)
		{
			req->state.store(EAsyncLoadState::Cancelled);
		}
	}

	m_PendingQueue.clear();
	m_ReadyQueue.clear();

	return m_Cv.wait_for(lock, timeout, [this]()
		{
			return !m_ReadInProgress;
		});
}