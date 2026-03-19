#pragma once

#include "AssetManager.h"

#pragma region Importer
#include "TextureImporter.h"
#include "ModelImporter.h"
#include "AnimationImporter.h"
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
	m_Importers[".tga"] = m_Importers[".png"];
	m_Importers[".bmp"] = m_Importers[".png"];

	m_Importers[".fbx"] = ModelImporter::Create();
	m_Importers[".obj"] = m_Importers[".fbx"];
	m_Importers[".gltf"] = m_Importers[".fbx"];

	m_Importers[".anim"] = AnimationImporter::Create();

	return EResult::Success;
}

void AssetManager::Free()
{
	for (auto& task : m_ActiveTasks)
	{
		if (task.valid()) task.wait();
	}
	m_ActiveTasks.clear();

	RELEASE_MAP(m_Importers);
	RELEASE_MAP(m_Exporters);
	m_OnAsyncDelegate.Clear();
}

#pragma endregion

#pragma region Loop
void AssetManager::Update(f32 dt)
{
	for (auto it = m_ActiveTasks.begin(); it != m_ActiveTasks.end(); )
	{
		// wait_for(0초): 진동벨이 울렸는지 안 울렸는지만 즉시 체크 (블로킹 없음)
		if (it->wait_for(std::chrono::seconds(0)) == std::future_status::ready)
		{
			// ready 상태라면 작업이 끝난 것입니다!
			EResult result = it->get(); // 결과를 가져옴과 동시에 future 객체 정리

			if (IsFailure(result))
			{
				std::cerr << "[AssetManager] Async Import Failed." << std::endl;
			}
			else
			{
				std::cout << "[AssetManager] Async Import Success." << std::endl;
				m_OnAsyncDelegate.Broadcast();
			}

			// 완료된 작업은 리스트에서 지웁니다.
			it = m_ActiveTasks.erase(it);
		}
		else
		{
			// 아직 작업 중이면 다음 작업 확인으로 넘어감
			++it;
		}
	}
}
#pragma endregion



#pragma region Codec
EResult AssetManager::Import(const filesystem::path& sourcePath, const filesystem::path& destDir, void* arg)
{
	string extension = sourcePath.extension().string();
	for (char& c : extension) c = tolower(c);
	if (m_Importers.find(extension) != m_Importers.end())
	{
		return m_Importers[extension]->Import(sourcePath, destDir);
	}
	return EResult::Fail;
}

void AssetManager::ImportAsync(const filesystem::path& sourcePath, const filesystem::path& destDir, void* arg)
{
	auto futureTask = std::async(std::launch::async, [this, sourcePath, destDir, arg]() -> EResult
		{
			// 이 안은 백그라운드 스레드이므로 여기서 무거운 Import를 호출해도 UI가 멈추지 않습니다!
			return this->Import(sourcePath, destDir);
		});

	m_ActiveTasks.push_back(std::move(futureTask));
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
			return this->Import(sourcePath, destDir);
		});

	m_ActiveTasks.push_back(std::move(futureTask));
}
#pragma endregion