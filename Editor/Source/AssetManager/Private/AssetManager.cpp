#pragma once

#include "AssetManager.h"
#include "Logger.h"
#include <exception>

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

#pragma region Construcotr&Destructor
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

	m_Importers[".anim"] = AnimationImporter::Create();
	m_Importers[".spv"] = ShaderImporter::Create();

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
		if (task.valid()) task.wait();
	}
	for (auto& task : m_PendingTasks)
	{
		if (task.valid()) task.wait();
	}
	m_ActiveTasks.clear();
	m_PendingTasks.clear();

	RELEASE_MAP(m_Importers);
	RELEASE_MAP(m_Exporters);
	m_OnAsyncDelegate.Clear();
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

	for (auto it = m_ActiveTasks.begin();
		it != m_ActiveTasks.end(); )
	{
		if (it->wait_for(std::chrono::seconds(0)) !=
			std::future_status::ready)
		{
			++it;
			continue;
		}

		EResult result = EResult::Fail;

		// 비동기 작업에서 발생한 C++ 예외는 get()에서 다시 전달됩니다.
		try
		{
			result = it->get();
		}
		catch (const std::exception& exception)
		{
			BAM_LOG(
				Error,
				"Asset",
				"Async task threw an exception: {}",
				exception.what());
		}
		catch (...)
		{
			BAM_LOG(
				Error,
				"Asset",
				"Async task threw an unknown exception");
		}

		if (IsFailure(result))
		{
			++m_FailedTasks;
			BAM_LOG(Error, "Asset", "Async task failed");
		}
		else
		{
			++m_SucceededTasks;
			++succeededThisUpdate;

			BAM_LOG(Info, "Asset", "Async task succeeded");
		}

		it = m_ActiveTasks.erase(it);
	}

	// 작업 목록을 순회한 뒤 알립니다.
	// 성공한 작업마다 한 번 알리는 기존 동작은 유지합니다.
	for (Engine::uint64 i = 0; i < succeededThisUpdate; ++i)
	{
		m_OnAsyncDelegate.Broadcast();
	}
}
#pragma endregion



#pragma region Codec
void AssetManager::ExecuteAsync(std::function<EResult()> task)
{
	auto futureTask = std::async(std::launch::async, task);
	std::lock_guard lock(m_PendingMutex);
	m_PendingTasks.push_back(std::move(futureTask));
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
	auto futureTask = std::async(std::launch::async, [this, sourcePath, destDir, arg]() -> EResult
		{
			// 이 안은 백그라운드 스레드이므로 여기서 무거운 Import를 호출해도 UI가 멈추지 않습니다!
			return this->Import(sourcePath, destDir, arg);
		});

	std::lock_guard lock(m_PendingMutex);
	m_PendingTasks.push_back(std::move(futureTask));
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
	m_PendingTasks.push_back(std::move(futureTask));
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
