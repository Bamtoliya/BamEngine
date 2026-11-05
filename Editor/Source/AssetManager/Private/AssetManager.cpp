#pragma once

#include "AssetManager.h"
#include "Logger.h"
#include "ModelImportTask.h"
#include "ThumbnailRenderer.h"

#pragma region Importer
#include "SpriteImporter.h"
#include "TextureImporter.h"

#include "ModelImporter.h"
#include "AnimationImporter.h"

#include "ShaderImporter.h"
#pragma endregion

#pragma region Exporter
//#include "TextureExporter.h"
//#include "ModelExporter.h"
//#include "AnimationExporter.h"
#pragma endregion

IMPLEMENT_SINGLETON(AssetManager);

#pragma region Constructor&Destructor
EResult AssetManager::Initialize(void* arg)
{
	m_Importers[".png"] = TextureImporter::Create();

	m_Importers[".jpg"] = m_Importers[".png"];
	Safe_AddRef(m_Importers[".jpg"]);

	m_Importers[".tga"] = m_Importers[".png"];
	Safe_AddRef(m_Importers[".tga"]);

	m_Importers[".bmp"] = m_Importers[".png"];
	Safe_AddRef(m_Importers[".bmp"]);

	m_Importers[".bamtex"] = SpriteImporter::Create();

	m_Importers[".fbx"] = ModelImporter::Create();

	m_Importers[".obj"] = m_Importers[".fbx"];
	Safe_AddRef(m_Importers[".obj"]);

	m_Importers[".gltf"] = m_Importers[".fbx"];
	Safe_AddRef(m_Importers[".gltf"]);

	m_Importers[".glb"] = m_Importers[".fbx"];
	Safe_AddRef(m_Importers[".glb"]);

	m_Importers[".anim"] = AnimationImporter::Create();
	m_Importers[".spv"] = ShaderImporter::Create();
	m_Importers[".hlsl"] = m_Importers[".spv"];
	Safe_AddRef(m_Importers[".hlsl"]);

	m_AssetCache = AssetCache::Create();
	if (!m_AssetCache)
	{
		RELEASE_MAP(m_Importers);
		return EResult::Fail;
	}

	return EResult::Success;
}
void AssetManager::Free()
{
	for (auto& task : m_ActiveTasks)
	{
		if (task.Future.valid())
			task.Future.wait();
	}

	for (auto& task : m_PendingTasks)
	{
		if (task.Future.valid())
			task.Future.wait();
	}
	m_ActiveTasks.clear();
	m_PendingTasks.clear();

	RELEASE_MAP(m_Importers);
	RELEASE_MAP(m_Exporters);
	m_OnAsyncDelegate.Clear();
	ThumbnailRenderer::Destroy();
	AssetCache::Destroy();
	m_AssetCache = nullptr;
}

#pragma endregion

#pragma region Loop
void AssetManager::Update(f32 dt)
{
	if (m_AssetCache) m_AssetCache->Update();

	// 1단계: 백그라운드에서 예약된 작업들을 메인 큐로 이동 (가장 짧은 락 시간)
	{
		std::lock_guard lock(m_PendingMutex);
		if (!m_PendingTasks.empty())
		{
			m_ActiveTasks.insert(
				m_ActiveTasks.end(),
				std::make_move_iterator(m_PendingTasks.begin()),
				std::make_move_iterator(m_PendingTasks.end())
			);
			m_PendingTasks.clear();
		}
	}

	Engine::uint64 succeededThisUpdate = 0;

	using Clock = std::chrono::steady_clock;
	const auto completionStart = Clock::now();
	const auto finalizeDeadline = completionStart + std::chrono::milliseconds(2);
	bool ranFinalizer = false;

	for (auto it = m_ActiveTasks.begin(); it != m_ActiveTasks.end(); )
	{
		std::optional<EResult> completedResult;

		try
		{
			if (!it->PreparedResult)
			{
				if (it->Future.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
				{
					++it;
					continue;
				}

				it->PreparedResult = it->Future.get();
			}

			if (it->Complete)
			{
				if (ranFinalizer && Clock::now() >= finalizeDeadline)
				{
					++it;
					continue;
				}

				ranFinalizer = true;
				completedResult = it->Complete(*it->PreparedResult);
			}
			else
			{
				completedResult = *it->PreparedResult;
			}
		}
		catch (const std::exception& exception)
		{
			completedResult = EResult::Fail;
			BAM_LOG(Error, "Editor", "Asset", "Async task threw an exception: {}", exception.what());
		}
		catch (...)
		{
			completedResult = EResult::Fail;
			BAM_LOG(Error, "Editor", "Asset", "Async task threw an unknown exception.");
		}

		if (!completedResult)
		{
			++it;
			continue;
		}

		if (*completedResult == EResult::Success)
		{
			++m_SucceededTasks;
			++succeededThisUpdate;
			BAM_LOG(Info, "Editor", "Asset", "Async task succeeded.");
		}
		else
		{
			++m_FailedTasks;
			BAM_LOG(Error, "Editor", "Asset", "Async task failed: {}", ResultToString(*completedResult));
		}

		it = m_ActiveTasks.erase(it);
	}

	// 작업 목록을 순회한 뒤 알립니다.
	// 성공한 작업마다 한 번 알리는 기존 동작은 유지합니다.
	for (Engine::uint64 i = 0; i < succeededThisUpdate; ++i)
	{
		m_OnAsyncDelegate.Broadcast();
	}
	if (ranFinalizer)
	{
		const double completionMs =
			std::chrono::duration<double, std::milli>(Clock::now() - completionStart).count();

		if (completionMs >= 8.0)
		{
			BAM_LOG(Warn, "Editor", "Asset",
				"Async completion frame timing: completion_ms={:.3f}, remaining_tasks={}",
				completionMs, m_ActiveTasks.size());
		}
	}
}
#pragma endregion

#pragma region Codec
void AssetManager::ExecuteAsync(std::function<EResult()> task)
{
	auto futureTask = std::async(std::launch::async, task);
	std::lock_guard lock(m_PendingMutex);
	m_PendingTasks.push_back({ std::move(futureTask), {} });
}
EResult AssetManager::Import(const filesystem::path& sourcePath, const filesystem::path& destDir, void* arg)
{
	string extension = sourcePath.extension().string();
	for (char& c : extension) c = tolower(c);
	if (m_Importers.find(extension) != m_Importers.end())
	{
		return m_Importers[extension]->Import(sourcePath, destDir, arg);
	}
	return EResult::Fail;
}

void AssetManager::ImportAsync(const filesystem::path& sourcePath, const filesystem::path& destDir, void* arg)
{
	std::string extension = sourcePath.extension().string();
	for (char& character : extension)
		character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));

	const auto readyFuture = [](EResult result)
		{
			std::promise<EResult> promise;
			auto future = promise.get_future();
			promise.set_value(result);
			return future;
		};

	AsyncTask pending;

	try
	{
		const bool isModel = extension == ".fbx" || extension == ".obj" || extension == ".gltf" || extension == ".glb";
		if (isModel)
		{
			const auto found = m_Importers.find(extension);
			auto* importer = found != m_Importers.end() ?
				dynamic_cast<ModelImporter*>(found->second) : nullptr;

			std::unique_ptr<ImportJob> job;
			EResult result = importer ?
				importer->CreateJob(sourcePath, destDir, arg, job) : EResult::NotImplemented;

			std::shared_ptr<ModelImportTask> task;
			if (result == EResult::Success)
			{
				task = std::dynamic_pointer_cast<ModelImportTask>(
					std::shared_ptr<ImportJob>(std::move(job)));

				if (!task)
					result = EResult::InvalidArgument;
			}

			if (result != EResult::Success)
			{
				BAM_LOG(Error, "Editor", "Asset", "Model import request failed: source={}, result={}",
					sourcePath.string(), ResultToString(result));
				pending.Future = readyFuture(result);
			}
			else
			{
				pending.Complete = [task, importer](EResult prepareResult) -> std::optional<EResult>
					{
						if (prepareResult != EResult::Success)
						{
							BAM_LOG(Error, "Editor", "Asset", "Model preparation failed: source={}, code={}, error={}",
								task->GetRequest().SourcePath.string(),
								static_cast<int>(task->GetErrorCode()), task->GetErrorMessage());
							return prepareResult;
						}

						return importer->FinalizeStep(*task);
					};

				pending.Future = std::async(std::launch::async, [task]() -> EResult
					{
						return task->Prepare();
					});
			}
		}
		else
		{
			pending.Future = std::async(std::launch::async,
				[this, sourcePath, destDir, arg]() -> EResult
				{
					return Import(sourcePath, destDir, arg);
				});
		}
	}
	catch (...)
	{
		pending.Complete = {};

		std::promise<EResult> failure;
		pending.Future = failure.get_future();
		failure.set_exception(std::current_exception());
	}

	std::lock_guard lock(m_PendingMutex);
	m_PendingTasks.push_back(std::move(pending));
}

EResult AssetManager::Export(const filesystem::path& sourcePath, const filesystem::path& destDir, void* arg)
{
	string extension = sourcePath.extension().string();
	for (char& c : extension) c = tolower(c);
	if(m_Exporters.find(extension) != m_Exporters.end())
	{
		return m_Exporters[extension]->Export(sourcePath, destDir);
	}
	return EResult::Fail;
}
void AssetManager::ExportAsync(const filesystem::path& sourcePath, const filesystem::path& destDir, void* arg)
{
	auto futureTask = std::async(std::launch::async, [this, sourcePath, destDir, arg]() -> EResult
		{
			// 이 안은 백그라운드 스레드이므로 여기서 무거운 Export를 호출해도 UI가 멈추지 않습니다!
			return this->Export(sourcePath, destDir, arg);
		});

	std::lock_guard lock(m_PendingMutex);
	m_PendingTasks.push_back({ std::move(futureTask), {} });
}
#pragma endregion

#pragma region Metrics
AssetTaskMetrics AssetManager::GetTaskMetrics() const
{
	// PendingTasks는 백그라운드에서도 추가하므로 잠금이 필요합니다.
	std::lock_guard lock(m_PendingMutex);

	AssetTaskMetrics metrics;
	metrics.Outstanding =
		static_cast<Engine::uint64>(m_ActiveTasks.size()) +
		static_cast<Engine::uint64>(m_PendingTasks.size());

	metrics.Succeeded = m_SucceededTasks;
	metrics.Failed = m_FailedTasks;

	return metrics;
}

size_t AssetManager::GetActiveTaskCount() const
{
	return static_cast<size_t>(GetTaskMetrics().Outstanding);
}
#pragma endregion