#pragma once

#include "Base.h"

BEGIN(Engine)
class ENGINE_API LayerManager : public Base
{
	DECLARE_SINGLETON(LayerManager)

#pragma region Struct
	typedef struct tagLayerInfo
	{
		wstring layerName = L"";
		bool isBuiltIn = false;
	} LAYERINFO;
#pragma endregion

#pragma region Constructor&Destructor
private:
	LayerManager() {}
	virtual ~LayerManager() = default;
	EResult Initialize(void* arg = nullptr);
public:
	virtual void Free() override;
#pragma endregion

#pragma region Layer Management
public:
	uint32 GetLayerCount() const { return static_cast<uint32>(m_LayerInfos.size()); }
	EResult AddLayer(wstring name, bool isBuiltIn = false);
#pragma endregion
#pragma region Variable
private:
	vector<LAYERINFO> m_LayerInfos;
#pragma endregion
};
END