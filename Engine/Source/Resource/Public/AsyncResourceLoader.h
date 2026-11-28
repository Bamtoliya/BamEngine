#pragma once

#include "Base.h"
#include "Handle.h"
#include "Macro.h"
#include "Types.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

BEGIN(Engine)

enum class EAsyncLoadState : uint8
{
	Pending,
	ReadingDisk,
	ReadyToFinalize,
	Completed,
	Failed,
	Cancelled
};

struct AsyncLoadRequest
{
	uint64 id = 0;
	std::filesystem::path filePath;
	std::atomic<EAsyncLoadState> state{ EAsyncLoadState::Pending };
	Handle resultHandle;
	std::vector<uint8_t> preloadedBytes;
	std::string errorMessage;
};

class ENGINE_API AsyncResourceLoader : public Base
{
	DECLARE_SINGLETON(AsyncResourceLoader)
public:
	using RequestId = uint64;

	AsyncResourceLoader();
	virtual ~AsyncResourceLoader();

	virtual EResult Initialize(void* arg = nullptr);
	virtual void Free() override;

	void Shutdown();

	RequestId RequestLoad(const std::filesystem::path& filePath);
	void StepMainThread(f64 budgetMs = 2.0);

	[[nodiscard]] EAsyncLoadState GetState(RequestId id) const;
	[[nodiscard]] Handle GetHandle(RequestId id) const;
	bool Cancel(RequestId id);
	[[nodiscard]] size_t GetPendingCount() const;

	bool CancelAllAndWaitIdle(std::chrono::milliseconds timeout);

private:
	void WorkerLoop();

private:
	mutable std::mutex m_Mutex;
	std::condition_variable m_Cv;
	std::atomic<bool> m_Stop{ false };
	std::thread m_WorkerThread;

	std::atomic<RequestId> m_NextRequestId{ 0 };
	std::unordered_map<RequestId, std::shared_ptr<AsyncLoadRequest>> m_Requests;
	std::deque<std::shared_ptr<AsyncLoadRequest>> m_PendingQueue;
	std::deque<std::shared_ptr<AsyncLoadRequest>> m_ReadyQueue;

	bool m_ReadInProgress = false;

	const std::thread::id m_OwnerThread = std::this_thread::get_id();
};

END
