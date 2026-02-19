#pragma once

#include "Layer.h"
#include "LayerManager.h"

IMPLEMENT_SINGLETON(LayerManager)


#pragma region Constructor&Destructor
EResult LayerManager::Initialize(void* arg)
{
	return EResult::Success;
}

void LayerManager::Free()
{
	m_LayerInfos.clear();
}
#pragma endregion


#pragma region Layer Management
EResult LayerManager::AddLayer(wstring name, bool isBuiltIn)
{
	LAYERINFO layerInfo;
	layerInfo.layerName = name;
	layerInfo.isBuiltIn = isBuiltIn;
	m_LayerInfos.push_back(layerInfo);
	return EResult::Success;
}
#pragma endregion