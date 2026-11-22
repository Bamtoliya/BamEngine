#pragma once

#include "Editor_Includes.h"
#include <filesystem>
#include <string>
#include <vector>
#include <queue>
#include <unordered_map>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <fstream>
#include <algorithm>
#include "ResourceEditorInterface.h"

BEGIN(Editor)

/**
 * @brief Common async resource editor open service.
 * Manages background disk I/O and metadata pre-checks on an owned worker thread,
 * and steps per-frame main-thread finalization within a shared budget (2ms/frame default)
 * with generation-numbered request tracking so stale requests are cancelled cleanly.
 */
class AsyncResourceOpenService
{
public:
	static AsyncResourceOpenService& Get()
	{
		static AsyncResourceOpenService s_Instance;
		return s_Instance;
	}

	AsyncResourceOpenService()
	{
		m_WorkerThread = std::thread([this]() { WorkerLoop(); });
	}

	~AsyncResourceOpenService()
	{
		Shutdown();
	}

	void Shutdown()
	{
		{
			std::lock_guard<std::mutex> lock(m_Mutex);
			if (m_Stopping)
				return;
			m_Stopping = true;
			while (!m_Queue.empty())
			{
				auto& req = m_Queue.front();
				if (req)
					req->Cancelled.store(true, std::memory_order_release);
				m_Queue.pop();
			}
		}
		m_Cv.notify_all();

		if (m_WorkerThread.joinable())
		{
			m_WorkerThread.join();
		}

		std::lock_guard<std::mutex> lock(m_Mutex);
		for (auto& req : m_ActiveRequests)
		{
			if (req)
				req->Cancelled.store(true, std::memory_order_release);
		}
		m_ActiveRequests.clear();
		m_EditorGenerations.clear();
	}

	struct Request
	{
		uint64 Generation = 0;
		ResourceEditorInterface* Editor = nullptr;
		std::filesystem::path Path;
		std::atomic<bool> Cancelled{ false };
		std::atomic<bool> BackgroundDone{ false };
		std::atomic<bool> Succeeded{ false };
		std::string ErrorMessage;
		uintmax_t FileSize = 0;
		std::vector<uint8_t> PreloadedBytes;
	};

	/**
	 * @brief Requests an asynchronous open of an asset for the specified editor.
	 * Calls editor->BeginAsyncOpen(path). If accepted, queues background work with a new generation.
	 * If the editor does not support async open, returns false so caller can fall back to SetTargetResource.
	 */
	bool RequestOpen(ResourceEditorInterface* editor, const std::filesystem::path& path)
	{
		if (!editor || path.empty())
			return false;

		// Ask editor if it supports and initiates async open
		if (!editor->BeginAsyncOpen(path))
			return false;

		std::shared_ptr<Request> req = std::make_shared<Request>();
		{
			std::lock_guard<std::mutex> lock(m_Mutex);
			const uint64 gen = ++m_NextGeneration;
			m_EditorGenerations[editor] = gen;

			// Cancel any older requests for this editor
			for (auto& active : m_ActiveRequests)
			{
				if (active->Editor == editor && active->Generation < gen)
				{
					active->Cancelled.store(true, std::memory_order_release);
				}
			}

			req->Generation = gen;
			req->Editor = editor;
			req->Path = path;

			m_Queue.push(req);
			m_ActiveRequests.push_back(req);
		}
		m_Cv.notify_one();
		return true;
	}

	/**
	 * @brief Cancels any pending or active async open requests for the given editor.
	 */
	void CancelRequestsForEditor(ResourceEditorInterface* editor)
	{
		if (!editor)
			return;

		std::lock_guard<std::mutex> lock(m_Mutex);
		m_EditorGenerations.erase(editor);
		for (auto& req : m_ActiveRequests)
		{
			if (req->Editor == editor)
				req->Cancelled.store(true, std::memory_order_release);
		}
	}

	/**
	 * @brief Steps active async requests on the main thread within the shared time budget.
	 * Default budget is 0.002s (2ms per frame) to prevent dropping 60 FPS.
	 */
	void Update(float maxBudgetSeconds = 0.002f)
	{
		using Clock = std::chrono::steady_clock;
		const auto deadline = Clock::now() + std::chrono::duration_cast<Clock::duration>(
			std::chrono::duration<float>(maxBudgetSeconds));

		std::vector<std::shared_ptr<Request>> readyRequests;
		{
			std::lock_guard<std::mutex> lock(m_Mutex);
			for (auto it = m_ActiveRequests.begin(); it != m_ActiveRequests.end();)
			{
				auto& req = *it;
				if (!req || req->Cancelled.load(std::memory_order_acquire))
				{
					it = m_ActiveRequests.erase(it);
					continue;
				}

				auto genIt = m_EditorGenerations.find(req->Editor);
				if (genIt == m_EditorGenerations.end() || genIt->second != req->Generation)
				{
					req->Cancelled.store(true, std::memory_order_release);
					it = m_ActiveRequests.erase(it);
					continue;
				}

				if (req->BackgroundDone.load(std::memory_order_acquire))
				{
					readyRequests.push_back(req);
				}
				++it;
			}
		}

		for (auto& req : readyRequests)
		{
			if (req->Cancelled.load(std::memory_order_acquire))
				continue;

			const auto now = Clock::now();
			float remainingSec = now < deadline
				? std::chrono::duration<float>(deadline - now).count()
				: 0.0001f;

			bool finished = true;
			if (req->Editor)
			{
				finished = req->Editor->StepAsyncOpen(remainingSec);
			}

			if (finished)
			{
				std::lock_guard<std::mutex> lock(m_Mutex);
				auto it = std::find(m_ActiveRequests.begin(), m_ActiveRequests.end(), req);
				if (it != m_ActiveRequests.end())
					m_ActiveRequests.erase(it);
			}

			if (Clock::now() >= deadline)
				break;
		}
	}

private:
	void WorkerLoop()
	{
		while (!m_Stopping)
		{
			std::shared_ptr<Request> req;
			{
				std::unique_lock<std::mutex> lock(m_Mutex);
				m_Cv.wait(lock, [this]() { return m_Stopping || !m_Queue.empty(); });
				if (m_Stopping)
					break;
				req = std::move(m_Queue.front());
				m_Queue.pop();
			}

			if (!req || req->Cancelled.load(std::memory_order_acquire))
				continue;

			// Disk I/O and metadata pre-check
			std::error_code ec;
			if (std::filesystem::is_regular_file(req->Path, ec))
			{
				req->FileSize = std::filesystem::file_size(req->Path, ec);
				std::ifstream file(req->Path, std::ios::binary);
				if (file)
				{
					const size_t bytesToRead = std::min<size_t>(static_cast<size_t>(req->FileSize), 16 * 1024 * 1024);
					if (bytesToRead > 0)
					{
						req->PreloadedBytes.resize(bytesToRead);
						file.read(reinterpret_cast<char*>(req->PreloadedBytes.data()), bytesToRead);
						req->PreloadedBytes.resize(file.gcount());
					}
					req->Succeeded.store(true, std::memory_order_release);
				}
				else
				{
					req->ErrorMessage = "Failed to open file for reading";
					req->Succeeded.store(false, std::memory_order_release);
				}
			}
			else
			{
				req->ErrorMessage = "File does not exist or is not a regular file";
				req->Succeeded.store(false, std::memory_order_release);
			}

			req->BackgroundDone.store(true, std::memory_order_release);
		}
	}

private:
	std::thread m_WorkerThread;
	std::mutex m_Mutex;
	std::condition_variable m_Cv;
	bool m_Stopping = false;

	uint64 m_NextGeneration = 0;
	std::queue<std::shared_ptr<Request>> m_Queue;
	std::vector<std::shared_ptr<Request>> m_ActiveRequests;
	std::unordered_map<ResourceEditorInterface*, uint64> m_EditorGenerations;
};

END
