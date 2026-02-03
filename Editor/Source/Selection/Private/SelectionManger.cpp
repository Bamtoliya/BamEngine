#pragma once

#include "SelectionManager.h"

IMPLEMENT_SINGLETON(SelectionManager)

void SelectionManager::Free()
{
	m_SelectedObjects.clear();
}