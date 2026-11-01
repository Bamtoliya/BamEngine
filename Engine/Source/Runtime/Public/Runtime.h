#pragma once

#include "Base.h"
#include "Renderer.h"
#include "TimeManager.h"
#include "InputManager.h"
#include "ResourceManager.h"
#include "SystemManager.h"
#include "SceneManager.h"
#include "RenderPassManager.h"
#include "RenderTargetManager.h"
#include "PipelineManager.h"
#include "SamplerManager.h"
#include "LightManager.h"
#include "LocalizationManager.h"
#include "ReflectionRegistry.h"
#include "PickingManager.h"
#include "AudioManager.h"
#include "ModuleManager.h"
#include "PlatformPath.h"

BEGIN(Engine)
typedef struct RuntimeCreateInfo
{
	RendererDesc RendererDesc;

	// 호스트가 결정하여 엔진에 전달하는 경로들 (엔진 독립성 유지)
	std::filesystem::path ProjectRoot;
	std::filesystem::path AssetRoot = "Resources";
} RUNTIMEDESC;

class ENGINE_API Runtime : public Base
{
	DECLARE_SINGLETON(Runtime)
#pragma region Constructor&Destructor
private:
	Runtime() {}
	virtual ~Runtime() = default;
	EResult Initialize(void* arg = nullptr);
public:
	virtual void Free() override;
#pragma endregion

#pragma region Loop
public:
	// 호스트가 고정 스텝의 크기와 실행 횟수를 결정합니다.
	void FixedUpdate(f32 dt);
	// 호스트에서 입력을 먼저 갱신했다면 같은 입력 샘플을 재사용합니다.
	void Update(f32 dt, bool updateInput = true);
	void LateUpdate(f32 dt);
	EResult Render(f32 dt);
#pragma endregion

#pragma region Paths
public:
	const std::filesystem::path& GetProjectRoot() const { return m_ProjectRoot; }
	const std::filesystem::path& GetAssetRoot() const { return m_AssetRoot; }
	// 상대 경로를 주입받은 ProjectRoot 기준 절대 경로로 안전하게 결합
	std::filesystem::path ResolveProjectRelativePath(const std::filesystem::path& relativePath) const
	{
		if (relativePath.is_absolute()) return PlatformPath::Normalize(relativePath);
		return PlatformPath::Normalize(m_ProjectRoot / relativePath);
	}
#pragma endregion


#pragma region Variables
private:
	entt::registry m_GlobalRegistry;
	ReflectionRegistry* m_ReflectionRegistry = { nullptr };
	TimeManager* m_TimeManager = { nullptr };
	InputManager* m_InputManager = { nullptr };
	Renderer* m_Renderer = { nullptr };
	RenderPassManager* m_RenderPassManager = { nullptr };
	RenderTargetManager* m_RenderTargetManager = { nullptr };
	PipelineManager* m_PipelineManager = { nullptr };
	SamplerManager* m_SamplerManager = { nullptr };
	LightManager* m_LightManager = { nullptr };
	ResourceManager* m_ResourceManager = { nullptr };
	SceneManager* m_SceneManager = { nullptr };
	SystemManager* m_SystemManager = { nullptr };
	LocalizationManager* m_LocalizationManager = { nullptr };
	PickingManager* m_PickingManager = { nullptr };
	AudioManager* m_AudioManager = { nullptr };
	ModuleManager* m_ModuleManager = { nullptr };
private:
	std::filesystem::path m_ProjectRoot;
	std::filesystem::path m_AssetRoot;
#pragma endregion
};

END
