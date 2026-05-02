#pragma once

#include "Base.h"
#include "LightSource.h"

#define MAX_LIGHTS 32

struct tagLightManagerDesc
{
	RHI* RHI = { nullptr };
};

BEGIN(Engine)
class ENGINE_API LightManager : public Base
{
	DECLARE_SINGLETON(LightManager)
	using DESC = tagLightManagerDesc;
#pragma region Constructor&Destructor
private:
	LightManager() {}
	virtual ~LightManager() = default;
	EResult Initialize(void* arg = nullptr);
public:
	virtual void Free() override;
#pragma endregion


#pragma region Loop
public:
	void Update(f32 dt);
#pragma endregion


#pragma region Light Management
public:
	EResult AddLightSource(LightSource* lightSource);
	EResult RemoveLightSource(LightSource* lightSource);
	const vector<LightSource*>& GetLightSources() const { return m_LightSources; }
	EResult ClearLightSources();
public:
	EResult Bind(uint32 slot);
private:
	EResult UpdateLightBuffer();
	EResult EnsureBuffer();
#pragma endregion

#pragma region Member Variables
private:
	vector<LightSource*> m_LightSources;
private:
	class RHIBuffer* m_LightBuffer = { nullptr };
	uint32 m_LightCount = { 0 };
	bool m_BufferDirty = { false };
private:
	RHI* m_RHI = { nullptr };
#pragma endregion
};
END