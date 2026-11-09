#include "EditorEditContext.h"
#include "SelectionManager.h"
#include "SceneManager.h"
#include "Scene.h"
#include "Entity.h"
#include "IDComponent.h"
#include "TerrainComponent.h"

#pragma region Constructor&Destructor
EditorEditContext::~EditorEditContext()
{
	if (m_SelectionManager)
		Shutdown();
}

EResult EditorEditContext::Initialize(SelectionManager& selectionManager)
{
	if (m_SelectionManager)
		return EResult::InvalidArgument;

	m_SelectionManager = &selectionManager;

	m_History.BeforeReplay = [this] { OnBeforeReplay(); };
	m_History.AfterReplay = [this] { OnAfterReplay(); };

	return EResult::Success;
}

void EditorEditContext::Shutdown()
{
	m_History.BeforeReplay = {};
	m_History.AfterReplay = {};

	m_ReplaySelection.clear();
	m_History.Commands().OnHistoryChanged().Clear();
	m_History.Clear();

	m_SelectionManager = nullptr;
}
#pragma endregion

#pragma region History
void EditorEditContext::OnBeforeReplay()
{
	m_ReplaySelection.clear();

	if (!m_SelectionManager)
		return;

	for (auto* entity : m_SelectionManager->GetSelectedEntities())
	{
		if (!entity || !entity->GetScene())
			continue;

		auto* scene = entity->GetScene();
		const auto* id = scene->GetRegistry().try_get<Engine::IDComponent>(entity->GetEntityHandle());

		if (id)
			m_ReplaySelection.emplace_back(scene, id->id);
	}

	m_SelectionManager->ClearSelection(true);
}

void EditorEditContext::OnAfterReplay()
{
	if (!m_SelectionManager)
		return;

	const bool locked = m_SelectionManager->IsPrimarySelectionLocked();
	m_SelectionManager->SetPrimarySelectionLock(false);

	for (const auto& [scene, id] : m_ReplaySelection)
	{
		if (auto* entity = scene->FindEntityById(id))
			m_SelectionManager->AddToEntitySelection(entity);
	}

	m_SelectionManager->SetPrimarySelectionLock(locked);
	m_ReplaySelection.clear();

	for (auto* scene : SceneManager::Get().GetActiveScenes())
	{
		if (!scene)
			continue;

		for (auto [entity, terrain] : scene->GetRegistry().view<Engine::TerrainComponent>().each())
			terrain.MarkHeightDirty();
	}
}
#pragma endregion