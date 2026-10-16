#pragma once

#include "Renderer.h"
#include "RHI.h"

#include "SDLGPURHI.h"

#include "ResourceManager.h"

#include "PipelineManager.h"
#include "RenderPassManager.h"
#include "RenderTargetManager.h"
#include "RenderComponent.h"
#include "UIRenderComponent.h"

#include "RHIPipeline.h"

#include "GameObject.h"

#include "Shader.h"
#include "Camera.h"

#include "Transform.h"
#include "LightManager.h"

#include "Texture.h"

#include "Mesh.h"
#include "MaterialInterface.h"
#include "SamplerManager.h"
#include "SceneManager.h"
#include "CoreComponents.h"

IMPLEMENT_SINGLETON(Renderer)

#pragma region Constructor&Destructor
EResult Renderer::Initialize(void* arg)
{	
	if (!arg) return EResult::InvalidArgument;

	CAST_DESC
	m_RHIType = desc->rhiType;
	switch (m_RHIType)
	{
	case ERHIType::SDLGPU:
		m_RHI = SDLGPURHI::Create(desc->rhiDesc);
		break;
	case ERHIType::DirectX12:
		m_RHI = DirectX12RHI::Create(desc->rhiDesc);
		break;
	default:
		return EResult::Fail;
	}
	
	if (!m_RHI)
		return EResult::Fail;

	return EResult::Success;
}

void Renderer::Free()
{	
	ClearPickingState();
	m_RenderPassManager = nullptr;
	//Safe_Release(m_SceneBuffer);
	//Safe_Release(m_DepthBuffer);

	if (m_RHI)
	{
		//순환참조 때문에 명시적으로 해제
		//m_RHI->Free();
		Safe_Release(m_RHI);
	}
}
#pragma endregion

#pragma region Render Management	
EResult Renderer::BeginFrame()
{
	++m_PickingFrameSerial;
	ClearPickingTargets(m_PickingTargets);
	m_PickingIds.clear();
	m_PickingPasses.clear();
	if (!m_RenderPassManager)
		m_RenderPassManager = &RenderPassManager::Get();
	if (m_RHI)
	{
		return m_RHI->BeginFrame();
	}
	return EResult::Fail;
}

EResult Renderer::Render(f32 dt)
{
	unordered_map<RenderPassID, vector<Camera*>> jobsPerPass;
	uint32 rtWidth = m_RHI->GetSwapChainWidth();
	uint32 rtHeight = m_RHI->GetSwapChainHeight();
	for (const auto& viewportInfo : m_ViewportCameras)
	{
    	if (!viewportInfo.renderPass) continue;
    	jobsPerPass[viewportInfo.renderPass->GetID()].push_back(viewportInfo.camera);
	}

	const vector<RenderPass*>& renderPasses = m_RenderPassManager->GetAllRenderPasses();

	for (const auto& pass : renderPasses)
	{
		const ERenderPassType passType = pass->GetPassType();
		if (passType == ERenderPassType::Shadow && LightManager::Get().GetShadowCastingLights().empty())
			continue;
		auto jobsIt = jobsPerPass.find(pass->GetID());

		vector<Camera*> cameras;
		if (jobsIt != jobsPerPass.end())
			cameras = jobsIt->second;
		else
			cameras.push_back(nullptr);

		for (Camera* cam : cameras)
		{
			bool pickingPassSucceeded = true;
			if (IsFailure(m_RHI->BeginRenderPass(pass)))
				continue;

			if (pass->GetRenderTargetCount() > 0)
			{
				RenderTarget* rt = RenderTargetManager::Get().GetRenderTarget(pass->GetRenderTargetName(0));
				if (rt)
				{
					rtWidth = rt->GetWidth();
					rtHeight = rt->GetHeight();
				}
			}
			else if (!pass->GetDepthStencilName().empty())
			{
				RenderTarget* rt = RenderTargetManager::Get().GetRenderTarget(pass->GetDepthStencilName());
				if (rt)
				{
					rtWidth = rt->GetWidth();
					rtHeight = rt->GetHeight();
				}
			}

			CameraBuffer cameraBuffer = {};

			if (passType == ERenderPassType::Shadow)
			{
				cameraBuffer = LightManager::Get().GetShadowCameraBuffer(uint32(0));
			}
			else if (passType == ERenderPassType::UI)
			{
				cameraBuffer.viewMatrix = glm::identity<mat4>();
				cameraBuffer.projMatrix = glm::ortho(0.0f, static_cast<f32>(rtWidth), static_cast<f32>(rtHeight), 0.0f, -1.0f, 1.0f);
				cameraBuffer.viewProjMatrix = cameraBuffer.projMatrix * cameraBuffer.viewMatrix;
				cameraBuffer.cameraPosition = vec3(0.f);
			}
			else if (cam)
			{
				cameraBuffer = cam->GetCameraBuffer();
			}
			else
			{
				cameraBuffer.viewMatrix = glm::identity<mat4>();
				cameraBuffer.projMatrix = glm::identity<mat4>();
				cameraBuffer.viewProjMatrix = glm::identity<mat4>();
				cameraBuffer.cameraPosition = vec3(0.f);
			}
			cameraBuffer.time = dt;
			if (IsFailure(m_RHI->BindConstantBuffer(&cameraBuffer, sizeof(CameraBuffer), 0)))
			{
				ENGINE_LOG_ERROR("Failed to bind camera constant buffer.");
				return EResult::Fail;
			}

			m_RHI->SetViewport(0, 0, rtWidth, rtHeight);

			auto meshIt = m_StaticDrawCommands.find(pass->GetID());

			if (meshIt != m_StaticDrawCommands.end())
			{
				const EResult result = RenderMeshes(
					dt,
					meshIt->second,
					pass->GetSortType(),
					pass);

				if (IsFailure(result))
				{
					pickingPassSucceeded = false;
					ENGINE_LOG_ERROR(
						"ECS mesh rendering failed. Pass ID: {}",
						pass->GetID());
				}
			}

			auto spriteIt = m_SpriteDrawCommands.find(pass->GetID());

			if (spriteIt != m_SpriteDrawCommands.end())
			{
				const EResult result = RenderSprite(
					dt,
					spriteIt->second,
					pass->GetSortType(),
					pass);

				if (IsFailure(result))
				{
					pickingPassSucceeded = false;
					ENGINE_LOG_ERROR(
						"ECS sprite rendering failed. Pass ID: {}",
						pass->GetID());
				}
			}

			auto it = m_RenderQueues.find(pass->GetID());
			if (it != m_RenderQueues.end())
				RenderComponents(dt, it->second, pass->GetSortType(), pass);

			auto uiIt = m_UIRenderQueues.find(pass->GetID());
			if (const auto images = m_UIImageDrawCommands.find(pass->GetID()); images != m_UIImageDrawCommands.end())
			{
				if (IsFailure(RenderUIImages(images->second, pass)))
					ENGINE_LOG_ERROR("ECS UI image rendering failed. Pass ID: {}", pass->GetID());
			}
			if (uiIt != m_UIRenderQueues.end())
				RenderUIComponents(dt, uiIt->second, pass->GetSortType(), pass);
			
			auto customIt = m_CustomRenderQueues.find(pass->GetID());
			if (customIt != m_CustomRenderQueues.end())
			{
				for (const auto& command : customIt->second)
					command(dt, pass);
			}

			if (IsFailure(m_RHI->EndRenderPass())) pickingPassSucceeded = false;
			if (passType == ERenderPassType::Geometry && pickingPassSucceeded)
				m_PickingPasses.insert(pass->GetID());
		}

		GetRenderPassDelegate(pass->GetID()).Broadcast(dt);
	}

	return EResult::Success;
}

EResult Renderer::RenderComponents(f32 dt, vector<class RenderComponent*> queue, ERenderSortType sortType, RenderPass* renderPass)
{
	if (sortType != ERenderSortType::None && !queue.empty())
	{
		const Camera* cam = GetViewportCamera(renderPass->GetID());
		const vec3 camPos = cam ? cam->GetCameraBuffer().cameraPosition : vec3(0.f);

		const bool bBackToFront = (sortType == ERenderSortType::BackToFront);

		std::sort(queue.begin(), queue.end(),
			[&camPos, bBackToFront](const RenderComponent* a, const RenderComponent* b)
			{
				Transform* ta = a->GetOwner()->GetComponent<Transform>();
				Transform* tb = b->GetOwner()->GetComponent<Transform>();
				if (!ta || !tb) return false;
				const float da = glm::distance2(vec3(ta->GetWorldMatrix()[3]), camPos);
				const float db = glm::distance2(vec3(tb->GetWorldMatrix()[3]), camPos);
				return bBackToFront ? (da > db) : (da < db);
			});
	}

	for (auto* component : queue)
	{
		if (component)
		{
			if (IsFailure(component->Render(dt, renderPass)))
			{
				ENGINE_LOG_ERROR("Component Render Failed! Component Name: {0}", WStrToStr(component->GetOwner()->GetName()));
				continue;
			}
		}
	}
	return EResult::Success;
}

EResult Renderer::RenderUIComponents(f32 dt, vector<class UIRenderComponent*> queue, ERenderSortType sortType, RenderPass* renderPass)
{

	for (auto* component : queue)
	{
		if (component)
		{
			if (IsFailure(component->Render(dt, renderPass)))
			{
				ENGINE_LOG_ERROR("UI Component Render Failed! Component Name: {0}", WStrToStr(component->GetOwner()->GetName()));
				continue;
			}
		}
	}
	return EResult();
}

TODO("Renderer::RenderMeshes, RenderSkinnedMeshes, RenderSprite 함수에서 Material과 Mesh를 Bind하고 DrawIndexed 호출하는 부분을 RHI에 맞게 구현 필요")
EResult Renderer::RenderMeshes(f32 dt, vector<StaticDrawCommand>& commands, ERenderSortType sortType, RenderPass* renderPass)
{
	(void)dt;
	(void)sortType;

	if (!m_RHI || !renderPass)
		return EResult::Fail;

	for (const auto& command : commands)
	{
		Mesh* mesh = command.mesh;
		MaterialInterface* material = command.material;

		if (!mesh || !material)
			return EResult::Fail;

		if (IsFailure(BindMeshPipeline(mesh, material, renderPass)))
			return EResult::Fail;

		if (IsFailure(material->Bind(2)))
			return EResult::Fail;

		SceneUBO objectData = {};
		objectData.worldMatrix = command.worldMatrix;
		objectData.pickingData.x = static_cast<float>(command.pickingId);

		if (IsFailure(m_RHI->BindConstantBuffer(&objectData, sizeof(objectData), 1)))
		{
			return EResult::Fail;
		}

		if (IsFailure(mesh->Bind(0)))
			return EResult::Fail;

		const EResult result = mesh->GetIndexBuffer()
			? m_RHI->DrawIndexed(mesh->GetIndexCount())
			: m_RHI->Draw(mesh->GetVertexCount());

		if (IsFailure(result))
			return result;
	}

	return EResult::Success;
}

EResult Renderer::RenderSkinnedMeshes(f32 dt, vector<SkinnedDrawCommand>& commands, ERenderSortType sortType, RenderPass* renderPass)
{
	for (auto& command : commands)
	{
		if (command.mesh && command.material)
		{
			if (IsFailure(command.material->Bind(0)))
				return EResult::Fail;
			if (IsFailure(command.mesh->Bind(0)))
				return EResult::Fail;
			//m_RHI->BindConstantBuffer(command.pBoneMatrices, sizeof(mat4) * command.pMesh->GetBoneCount(), 1, EShaderType::Vertex);
			m_RHI->DrawIndexed(command.mesh->GetIndexCount());
		}
	}
	return EResult();
}

EResult Renderer::RenderSprite(
	f32 dt,
	vector<SpriteDrawCommand>& commands,
	ERenderSortType sortType,
	RenderPass* renderPass)
{
	if (!m_RHI || !renderPass)
		return EResult::Fail;

	struct SpriteUBO
	{
		mat4 worldMatrix;
		vec4 color;
		vec4 uvTransform;
		vec4 pickingData;
	};

	static_assert(offsetof(SpriteUBO, color) == 64);
	static_assert(offsetof(SpriteUBO, uvTransform) == 80);
	static_assert(offsetof(SpriteUBO, pickingData) == 96);
	static_assert(sizeof(SpriteUBO) == 112);

	RHISampler* sampler = SamplerManager::Get().GetDefaultSampler();

	if (!sampler)
		return EResult::Fail;

	for (const auto& command : commands)
	{
		if (!command.mesh || !command.material || !command.texture)
			return EResult::Fail;

		RHITexture* texture = command.texture->GetRHITexture();

		if (!texture || !command.mesh->GetIndexBuffer())
			return EResult::Fail;

		if (IsFailure(BindMeshPipeline(
			command.mesh, command.material, renderPass)))
		{
			return EResult::Fail;
		}

		// 이번 Sprite 셰이더의 t0는 Sprite 리소스가 제공합니다.
		if (IsFailure(m_RHI->BindTextureSampler(texture, sampler, 0)))
			return EResult::Fail;

		SpriteUBO objectData{};
		objectData.worldMatrix = command.worldMatrix;
		objectData.color = command.color;
		objectData.uvTransform = command.uvTransform;
		objectData.pickingData = vec4(static_cast<float>(command.pickingId), 0.0f, 0.0f, 0.0f);

		if (IsFailure(m_RHI->BindConstantBuffer(
			&objectData, sizeof(objectData), 1)))
		{
			return EResult::Fail;
		}

		if (IsFailure(command.mesh->Bind(0)))
			return EResult::Fail;

		if (IsFailure(m_RHI->DrawIndexed(command.mesh->GetIndexCount())))
			return EResult::Fail;
	}

	return EResult::Success;
}

EResult Renderer::RenderUIImages(const vector<UIImageDrawCommand>& commands, RenderPass* renderPass)
{
	if (!m_RHI || !renderPass) return EResult::Fail;
	struct ImageUBO
	{
		mat4 worldMatrix;
		vec4 uvRect;
		vec2 tiling;
		vec2 offset;
		vec4 color;
	};
	static_assert(sizeof(ImageUBO) == 112);
	static_assert(offsetof(ImageUBO, color) == 96);
	auto* sampler = SamplerManager::Get().GetDefaultSampler();
	if (!sampler) return EResult::Fail;
	for (const auto& command : commands)
	{
		Texture* texture = command.textureHandle.Get();
		MaterialInterface* material = command.materialHandle.Get();
		if (!texture || !material || !texture->GetRHITexture() ||
			command.canvasSize.x <= 0.0f || command.canvasSize.y <= 0.0f) return EResult::InvalidArgument;
		if (IsFailure(BindMeshPipeline(nullptr, material, renderPass))) return EResult::Fail;
		CameraBuffer camera{};
		camera.viewMatrix = camera.invViewMatrix = mat4(1.0f);
		camera.projMatrix = glm::ortho(0.0f, command.canvasSize.x, command.canvasSize.y, 0.0f, -1.0f, 1.0f);
		camera.invProjMatrix = glm::inverse(camera.projMatrix);
		camera.viewProjMatrix = camera.projMatrix;
		camera.invViewProjMatrix = camera.invProjMatrix;
		ImageUBO data{};
		data.worldMatrix = command.worldMatrix;
		data.uvRect = vec4(vec2(command.uvTransform),
			vec2(command.uvTransform) + vec2(command.uvTransform.z, command.uvTransform.w));
		data.tiling = vec2(1.0f);
		data.color = command.color;
		if (IsFailure(m_RHI->BindConstantBuffer(&camera, sizeof(camera), 0)) ||
			IsFailure(m_RHI->BindConstantBuffer(&data, sizeof(data), 1)) ||
			IsFailure(m_RHI->BindTextureSampler(texture->GetRHITexture(), sampler, 0)) ||
			IsFailure(m_RHI->Draw(6))) return EResult::Fail;
	}
	// Restore the pass projection for legacy UI components drawn afterwards.
	if (renderPass->GetRenderTargetCount() > 0)
	{
		if (auto* target = RenderTargetManager::Get().GetRenderTarget(renderPass->GetRenderTargetName(0)))
		{
			CameraBuffer camera{};
			camera.viewMatrix = camera.invViewMatrix = mat4(1.0f);
			camera.projMatrix = glm::ortho(0.0f, float(target->GetWidth()),
				float(target->GetHeight()), 0.0f, -1.0f, 1.0f);
			camera.invProjMatrix = glm::inverse(camera.projMatrix);
			camera.viewProjMatrix = camera.projMatrix;
			camera.invViewProjMatrix = camera.invProjMatrix;
			return m_RHI->BindConstantBuffer(&camera, sizeof(camera), 0);
		}
	}
	return EResult::Success;
}

EResult Renderer::EndFrame()
{
	EResult frameResult = EResult::Fail;
	if (m_RHI)
	{
		m_RHI->BindRenderTarget(nullptr, nullptr);
		m_RHI->SetViewport(0, 0, m_RHI->GetSwapChainWidth(), m_RHI->GetSwapChainHeight());
		frameResult = m_RHI->EndFrame();
	}
	ClearPickingTargets(m_CompletedPickingTargets);
	m_CompletedPickingPasses.clear();
	m_CompletedPickingFrameSerial = 0;
	if (IsSuccess(frameResult))
	{
		m_CompletedPickingTargets.swap(m_PickingTargets);
		m_CompletedPickingPasses.swap(m_PickingPasses);
		m_CompletedPickingFrameSerial = m_PickingFrameSerial;
	}

	for (auto& pair : m_RenderQueues)
	{
		RELEASE_VECTOR(pair.second);
	}

	m_RenderQueues.clear();
	m_UIRenderQueues.clear();
	m_CustomRenderQueues.clear();
	m_ViewportCameras.clear(); 
	m_PassFrustums.clear();
	m_PassFrustumIsShadow.clear();
	m_StaticDrawCommands.clear();
	m_SkinnedDrawCommands.clear();
	m_SpriteDrawCommands.clear();
	m_UIImageDrawCommands.clear();
	return frameResult;
}
#pragma endregion

void Renderer::ClearPickingTargets(vector<PickingTarget>& targets)
{
	for (auto& target : targets)
		Safe_Release(target.scene);
	targets.clear();
}

void Renderer::ClearPickingState()
{
	ClearPickingTargets(m_PickingTargets);
	ClearPickingTargets(m_CompletedPickingTargets);
	m_PickingIds.clear();
	m_PickingPasses.clear();
	m_CompletedPickingPasses.clear();
	m_CompletedPickingFrameSerial = 0;
}

uint32 Renderer::RegisterPickingEntity(Scene* scene, entt::entity entity)
{
	if (!scene || !scene->GetRegistry().valid(entity)) return 0;
	const auto* id = scene->GetRegistry().try_get<IDComponent>(entity);
	if (!id || id->id == 0) return 0;
	auto& ids = m_PickingIds[scene];
	if (const auto found = ids.find(entity); found != ids.end()) return found->second;
	// Float32 can represent every integer in this range without rounding.
	if (m_PickingTargets.size() >= 0x00ffffffu) return 0;
	m_PickingTargets.push_back({ scene, entity, id->id });
	Safe_AddRef(scene);
	const uint32 pickingId = static_cast<uint32>(m_PickingTargets.size());
	ids.emplace(entity, pickingId);
	return pickingId;
}

Entity* Renderer::ResolvePickingEntity(uint32 pickingId) const
{
	if (pickingId == 0 || pickingId > m_CompletedPickingTargets.size()) return nullptr;
	const auto& target = m_CompletedPickingTargets[pickingId - 1];
	const auto scenes = SceneManager::Get().GetActiveScenes();
	if (std::find(scenes.begin(), scenes.end(), target.scene) == scenes.end()) return nullptr;
	const auto& world = target.scene->GetRegistry();
	if (!world.valid(target.entity)) return nullptr;
	const auto* id = world.try_get<IDComponent>(target.entity);
	const auto* flag = world.try_get<FlagComponent>(target.entity);
	if (!id || id->id != target.persistentId || !flag || !HasFlag(flag->flags, EEntityFlag::Active) ||
		!HasFlag(flag->flags, EEntityFlag::Visible) || HasFlag(flag->flags, EEntityFlag::Dead)) return nullptr;
	return target.scene->FindEntity(target.entity);
}

#pragma region Queue Management
void Renderer::Submit(class RenderComponent* component, RenderPassID passID)
{
	Safe_AddRef(component);
	m_RenderQueues[passID].push_back(component);
}
void Renderer::SubmitAllPass(RenderComponent* component)
{
	for (auto& pair : m_RenderPassManager->GetAllRenderPasses())
	{
		m_RenderQueues[pair->GetID()].push_back(component);
		Safe_AddRef(component);
	}
}
void Renderer::SubmitUI(UIRenderComponent* uiRenderer, RenderPassID passID)
{
	Safe_AddRef(uiRenderer);
	m_UIRenderQueues[passID].push_back(uiRenderer);
}
void Renderer::SubmitCustomCommand(const CustomRenderCommand& command, RenderPassID passID)
{
	m_CustomRenderQueues[passID].push_back(command);
}

void Renderer::ClearRenderQueue(RenderPassID passID)
{
	auto it = m_RenderQueues.find(passID);
	if (it != m_RenderQueues.end())
	{
		RELEASE_VECTOR(it->second);
		m_RenderQueues.erase(it);
	}
	auto customIt = m_CustomRenderQueues.find(passID);
	if (customIt != m_CustomRenderQueues.end())
	{
		customIt->second.clear();
		m_CustomRenderQueues.erase(customIt);
	}
	m_StaticDrawCommands.erase(passID);
	m_SkinnedDrawCommands.erase(passID);
	m_SpriteDrawCommands.erase(passID);
	m_UIImageDrawCommands.erase(passID);
}

void Renderer::ClearAllRenderQueues()
{
	for (auto& pair : m_RenderQueues)
	{
		RELEASE_VECTOR(pair.second);
	}
	m_RenderQueues.clear();
	for (auto& pair : m_CustomRenderQueues)
	{
		pair.second.clear();
	}
	m_CustomRenderQueues.clear();
	m_StaticDrawCommands.clear();
	m_SkinnedDrawCommands.clear();
	m_SpriteDrawCommands.clear();
	m_UIImageDrawCommands.clear();
}

//Entity Submit
void Renderer::SubmitStatic(const StaticDrawCommand& command, RenderPassID passID)
{
	m_StaticDrawCommands[passID].push_back(command);
}
void Renderer::SubmitSkinned(const SkinnedDrawCommand& command, RenderPassID passID)
{
	m_SkinnedDrawCommands[passID].push_back(command);
}
void Renderer::SubmitSprite(const SpriteDrawCommand& command, RenderPassID passID)
{
	m_SpriteDrawCommands[passID].push_back(command);
}

void Renderer::SubmitUIImage(const UIImageDrawCommand& command, RenderPassID passID)
{
	m_UIImageDrawCommands[passID].push_back(command);
}
#pragma endregion

#pragma region Viewport Camera Management

void Renderer::RegisterViewportCamera(Camera* camera, RenderPass* renderPass)
{
	m_ViewportCameras.push_back({ camera, renderPass });
	if (!renderPass) return;

	const RenderPassID passID = renderPass->GetID();
	const bool isShadow = (renderPass->GetPassType() == ERenderPassType::Shadow);

	if (isShadow)
	{
		// Shadow Pass: 라이트의 View-Projection 기준으로 프러스텀 생성
		if (!LightManager::Get().GetShadowCastingLights().empty())
		{
			m_PassFrustums[passID] = FrustumCuller::ExtractFrustum(
				LightManager::Get().GetShadowCameraBuffer(uint32(0)).viewProjMatrix);
			m_PassFrustumIsShadow[passID] = true;
		}
	}
	else if (camera)
	{
		// 일반 Pass: 카메라의 View-Projection 기준으로 프러스텀 생성
		m_PassFrustums[passID] = FrustumCuller::ExtractFrustum(
			camera->GetCameraBuffer().viewProjMatrix);
		m_PassFrustumIsShadow[passID] = false;
	}
}

void Renderer::RegisterUICanvas(Scene* scene, entt::entity canvas, RenderPass* renderPass)
{
	if (renderPass) m_ViewportCameras.push_back({ nullptr, renderPass, scene, canvas, true });
}

void Renderer::UnregisterViewportCamera(RenderPassID passID)
{
	m_ViewportCameras.erase(std::remove_if(m_ViewportCameras.begin(), m_ViewportCameras.end(),
		[passID](const ViewportCameraInfo& info) { return info.renderPass->GetID() == passID; }), m_ViewportCameras.end());
}

Camera* Renderer::GetViewportCamera(RenderPassID passID) const
{
	for (const auto& info : m_ViewportCameras)
	{
		if (info.renderPass->GetID() == passID)
		{
			return info.camera;
		}
	}
	return nullptr;
}
#pragma endregion

#pragma region Frustum Culling
bool Renderer::TryGetPassFrustum(RenderPassID passID, Frustum& outFrustum, bool& outIsShadow) const
{
	const auto it = m_PassFrustums.find(passID);
	if (it == m_PassFrustums.end())
		return false;

	outFrustum = it->second;
	outIsShadow = m_PassFrustumIsShadow.at(passID);
	return true;
}

#pragma endregion

#pragma region Binding
EResult Renderer::BindMeshPipeline(Mesh* mesh, MaterialInterface* material, RenderPass* renderPass) 
{
	if (!m_RHI || !material || !renderPass)
		return EResult::InvalidArgument;

	Shader* vs = material->GetVertexShader();
	Shader* ps = material->GetPixelShader();

	if (!vs || !ps)
		return EResult::InvalidArgument;

	if (!vs->GetRHIShader() || !ps->GetRHIShader())
		return EResult::InvalidArgument;

	RHIPipelineDesc desc;
	desc.pipelineType = EPipelineType::Graphics;
	desc.topology = mesh ? mesh->GetTopology() : ETopology::TriangleList;
	if (mesh) desc.inputLayouts = mesh->GetInputLayoutDescs();

	desc.vertexShader = vs->GetRHIShader();
	desc.pixelShader = ps->GetRHIShader();

	desc.frontFace = material->GetFrontFace();
	desc.cullMode = material->GetCullMode();
	desc.fillMode = material->GetFillMode();

	desc.blendState = material->GetBlendState();

	RenderTargetManager& rtManager = RenderTargetManager::Get();

	desc.colorAttachmentCount = renderPass->GetRenderTargetCount();

	if(desc.colorAttachmentCount > MAX_RENDER_TARGET_COUNT)
		return EResult::InvalidArgument;

	for (uint32 i = 0; i < desc.colorAttachmentCount; ++i)
	{
		const wstring& rtName = renderPass->GetRenderTargetName(i);
		RenderTarget* rt = rtManager.GetRenderTarget(rtName);
		if (!rt)
			return EResult::InvalidArgument;
		desc.colorAttachmentFormats[i] = rt->GetFormat();
	}

	desc.depthStencilAttachmentFormat = ETextureFormat::UNKNOWN;

	const wstring depthStencilName = renderPass->GetDepthStencilName();
	const bool hasDepthStencil = !depthStencilName.empty();

	if (hasDepthStencil)
	{
		RenderTarget* ds = rtManager.GetRenderTarget(depthStencilName);
		if (!ds)
			return EResult::InvalidArgument;
		desc.depthStencilAttachmentFormat = ds->GetFormat();
	}

	desc.depthStencilState.depthTestEnable = hasDepthStencil && material->GetDepthMode() != EDepthMode::None;
	desc.depthStencilState.depthWriteEnable = hasDepthStencil && material->GetDepthMode() == EDepthMode::ReadWrite;
	desc.depthStencilState.depthCompareOp = material->GetDepthCompareOp();

	RHIPipeline* pipeline = PipelineManager::Get().GetOrCreatePipeline(desc);

	if (!pipeline)
		return EResult::Fail;
	
	return m_RHI->BindPipeline(pipeline);
}
#pragma endregion
