#pragma once

#include "Editor_Includes.h"
#include "SceneEditHistory.h"

BEGIN(Editor)

class SelectionManager;

class EditorEditContext final
{
#pragma region Constructor&Destructor
public:
	EditorEditContext() = default;
	~EditorEditContext();

	EditorEditContext(const EditorEditContext&) = delete;
	EditorEditContext& operator=(const EditorEditContext&) = delete;
	EditorEditContext(EditorEditContext&&) = delete;
	EditorEditContext& operator=(EditorEditContext&&) = delete;

	EResult Initialize(SelectionManager& selectionManager);
	void Shutdown();
#pragma endregion

#pragma region History
public:
	SceneEditHistory& History() { return m_History; }
	const SceneEditHistory& History() const { return m_History; }
private:
	void OnBeforeReplay();
	void OnAfterReplay();
#pragma endregion

#pragma region Member Variables
private:
	SceneEditHistory m_History;
	std::vector<std::pair<Engine::Scene*, uint64>> m_ReplaySelection;

	SelectionManager* m_SelectionManager = nullptr;
#pragma endregion
};

END