#ifdef _DEBUG
#pragma once

#include "DebugRenderer.h"
#include "Renderer.h"
#include "ResourceManager.h"
#include "PipelineManager.h"
#include "RenderTargetManager.h"
#include "SceneManager.h"
#include "CoreComponents.h"
#include "PhysicsComponent.h"
#include "TransformSystem.h"
#include "Mesh.h"

void DebugRenderer::Initialize(const wstring& prefix)
{
	ResourceManager& rm = ResourceManager::Get();

	ShaderDesc vsDesc = {};
	{
		vsDesc.Key			= L"DebugLineVS";
		vsDesc.Path			= L"Resources/Shader/bin/dxil/debugLineVS.cso";
		vsDesc.spirvPath	= L"Resources/Shader/bin/spirv/debugLineVS.spv";
		vsDesc.shaderType = EShaderType::Vertex;
		vsDesc.entryPoint = "main";
		auto handle = rm.LoadResource<Shader>(&vsDesc);
		//rm.SaveToBinaryFile(handle.Get(), L"Resources/Shader/debugLine.vert.bamshader");
	}

	ShaderDesc psDesc = {};
	{
		psDesc.Key			= L"DebugLinePS";
		psDesc.Path			= L"Resources/Shader/bin/dxil/debugLinePS.cso";
		psDesc.spirvPath	= L"Resources/Shader/bin/spirv/debugLinePS.spv";
		psDesc.shaderType = EShaderType::Fragment;
		psDesc.entryPoint = "main";
		auto handle = rm.LoadResource<Shader>(&psDesc);
		//rm.SaveToBinaryFile(handle.Get(), L"Resources/Shader/debugLine.frag.bamshader");
	}

	m_Vertices.reserve(2 * m_MaxLines);

	m_VertexBuffer = Engine::Renderer::Get().GetRHI()->CreateBuffer(
		nullptr,
		2 * m_MaxLines * sizeof(DebugVertex),
		sizeof(DebugVertex),
		ERHIBufferType::Vertex);

	m_RenderPassID = Engine::RenderPassManager::Get().RegisterRenderPass(
		prefix + L"_DebugPass", {}, L"",
		ERenderPassLoadOperation::RPLO_Load, ERenderPassStoreOperation::RPSO_Store,
		ERenderPassLoadOperation::RPLO_Load, ERenderPassStoreOperation::RPSO_Store,
		vec4(0.0f, 0.f, 0.f, -1.f), 500, ERenderSortType::None, ERenderPassType::Custom, EBlendMode::None);
}

void DebugRenderer::Free()
{
	Safe_Release(m_VertexBuffer);
}

void DebugRenderer::SubmitDebugDraw(const CameraBuffer& cameraBuffer, const wstring& colorRTName)
{
	RegisterColliderDebug(cameraBuffer);
	if (m_Vertices.empty() || colorRTName.empty() || !m_VertexBuffer) return;
	RenderPass* pass = Engine::RenderPassManager::Get().GetRenderPassByID(m_RenderPassID);
	pass->SetColorAttachments({ colorRTName });
	Renderer::Get().RegisterViewportCamera(cameraBuffer, pass);

	Engine::Renderer::Get().SubmitCustomCommand(
		[this](f32 dt, Engine::RenderPass* pass)->EResult
		{
			if (m_Vertices.empty() || !m_VertexBuffer) return EResult::Success;

			Engine::RHI* rhi = Engine::Renderer::Get().GetRHI();
			ResourceManager& rm = ResourceManager::Get();

			// 파이프라인 디스크립터 (Grid와 동일한 방식으로 RenderPass에서 포맷 동적 매칭)
			RHIPipelineDesc desc = {};
			desc.pipelineType = EPipelineType::Graphics;
			desc.blendState = Engine::BlendState{};
			desc.fillMode = EFillMode::Solid;
			desc.cullMode = ECullMode::None;
			desc.topology = ETopology::LineList;

			auto debugVSHandle = rm.GetResourceHandle<Shader>(L"DebugLineVS");
			auto debugPSHandle = rm.GetResourceHandle<Shader>(L"DebugLinePS");
			if (!debugVSHandle || !debugPSHandle)
			{
				m_Vertices.clear();
				return EResult::Fail;
			}
			desc.vertexShader = debugVSHandle->GetRHIShader();
			desc.pixelShader = debugPSHandle->GetRHIShader();
			desc.inputLayouts.push_back(Engine::DebugVertex::Layout);

			desc.depthStencilState.depthTestEnable = false;
			desc.depthStencilState.depthWriteEnable = false;
			desc.depthStencilState.stencilTestEnable = false;
			desc.depthStencilState.depthCompareOp = ECompareOp::Always;

			desc.colorAttachmentCount = pass->GetRenderTargetCount();
			for (uint32 i = 0; i < desc.colorAttachmentCount; ++i)
			{
				desc.colorAttachmentFormats[i] = Engine::RenderTargetManager::Get()
					.GetRenderTarget(pass->GetRenderTargetName(i))->GetFormat();
			}

			// [핵심] RenderPass에서 Depth Stencil 포맷 동적 매칭
			wstring depthStencilName = pass->GetDepthStencilName();
			if (!depthStencilName.empty())
			{
				desc.depthStencilAttachmentFormat = Engine::RenderTargetManager::Get()
					.GetRenderTarget(depthStencilName)->GetFormat();
			}
			else
			{
				desc.depthStencilAttachmentFormat = Engine::ETextureFormat::UNKNOWN;
			}

			RHIPipeline* pipeline = PipelineManager::Get().GetOrCreatePipeline(desc);
			if (!pipeline || IsFailure(rhi->BindPipeline(pipeline)))
			{
				m_Vertices.clear();
				return EResult::Fail;
			}

			m_VertexBuffer->SetData(m_Vertices.data(), (uint32)m_Vertices.size() * sizeof(DebugVertex));

			RHIBuffer* buffers[1] = { m_VertexBuffer };
			rhi->BindVertexBuffers(0, buffers, 1);

			EResult res = rhi->Draw((uint32)m_Vertices.size());
			m_Vertices.clear();
			return res;
		}, m_RenderPassID);
}

void DebugRenderer::RegisterColliderDebug(const CameraBuffer& cameraBuffer)
{
	Scene* scene = SceneManager::Get().GetCurrentScene();
	if (!scene) return;
	auto& registry = scene->GetRegistry();
	TransformSystem::UpdateHierarchy(registry);
	const Frustum frustum = FrustumCuller::ExtractFrustum(cameraBuffer.viewProjMatrix);
	const auto isVisibleInFrustum = [&](const vec3& localCenter, const vec3& localExtent, const mat4& worldMatrix)
		{
			const AABB localAABB(localCenter - localExtent, localCenter + localExtent);
			const AABB worldAABB = FrustumCuller::TransformAABB(localAABB, worldMatrix);
			return FrustumCuller::TestAABB(frustum, worldAABB);
		};
	const auto drawCircleXY = [&](const vec3& localCenter, f32 radius, const vec4& color, const mat4& worldMatrix)
		{
			constexpr uint32 segments = 32;
			const f32 step = glm::two_pi<f32>() / static_cast<f32>(segments);
			for (uint32 i = 0; i < segments; ++i)
			{
				const f32 a0 = step * static_cast<f32>(i);
				const f32 a1 = step * static_cast<f32>(i + 1);
				const vec3 p0 = localCenter + vec3(std::cos(a0) * radius, std::sin(a0) * radius, 0.f);
				const vec3 p1 = localCenter + vec3(std::cos(a1) * radius, std::sin(a1) * radius, 0.f);
				DrawLine(vec3(worldMatrix * vec4(p0, 1.f)), vec3(worldMatrix * vec4(p1, 1.f)), color);
			}
		};
	for (auto [entity, collider, tr, flag] : registry.view<ColliderComponent, WorldTransformComponent, FlagComponent>().each())
	{
		if (!HasFlag(flag.flags, EEntityFlag::Active) || HasFlag(flag.flags, EEntityFlag::Dead)) continue;
		const auto& settings = collider.GetSettings();
		if (!settings.Enabled) continue;
		const vec4 color = settings.IsTrigger ? vec4(0.f, 1.f, 1.f, 1.f) : vec4(0.f, 1.f, 0.f, 1.f);
		const vec3 offset = collider.GetOffset();
		const mat4& worldMatrix = tr.worldMatrix;
		const f32 maxScale = glm::max(glm::max(glm::length(vec3(worldMatrix[0])), glm::length(vec3(worldMatrix[1]))),
			glm::length(vec3(worldMatrix[2])));
		std::visit([&](const auto& shape)
			{
				using ShapeType = std::decay_t<decltype(shape)>;
				if constexpr (std::is_same_v<ShapeType, BoxShape>)
				{
					const vec3 extent = shape.Size * 0.5f;
					if (isVisibleInFrustum(offset, extent, worldMatrix))
						DrawBox(offset, extent, color, worldMatrix);
				}
				else if constexpr (std::is_same_v<ShapeType, SphereShape>)
				{
					const vec3 extent(shape.Radius);
					if (isVisibleInFrustum(offset, extent, worldMatrix))
						DrawSphere(vec3(worldMatrix * vec4(offset, 1.f)), shape.Radius * maxScale, color);
				}
				else if constexpr (std::is_same_v<ShapeType, CapsuleShape>)
				{
					const f32 halfHeight = shape.Height * 0.5f;
					const vec3 extent(shape.Radius, halfHeight, shape.Radius);
					if (!isVisibleInFrustum(offset, extent, worldMatrix)) return;
					const f32 halfSegment = glm::max(0.f, halfHeight - shape.Radius);
					const vec3 topLocal = offset + vec3(0.f, halfSegment, 0.f);
					const vec3 bottomLocal = offset - vec3(0.f, halfSegment, 0.f);
					DrawSphere(vec3(worldMatrix * vec4(topLocal, 1.f)), shape.Radius * maxScale, color);
					DrawSphere(vec3(worldMatrix * vec4(bottomLocal, 1.f)), shape.Radius * maxScale, color);
					const vec3 sideOffsets[4] = {
						vec3(shape.Radius, 0.f, 0.f), vec3(-shape.Radius, 0.f, 0.f),
						vec3(0.f, 0.f, shape.Radius), vec3(0.f, 0.f, -shape.Radius)
					};
					for (const vec3& side : sideOffsets)
					{
						DrawLine(vec3(worldMatrix * vec4(topLocal + side, 1.f)),
							vec3(worldMatrix * vec4(bottomLocal + side, 1.f)), color);
					}
				}
				else if constexpr (std::is_same_v<ShapeType, MeshShape>)
				{
					if (const Mesh* mesh = shape.MeshHandle.Get())
					{
						const vec3 center = offset + mesh->GetCenter();
						const vec3 extent = mesh->GetExtents();
						if (isVisibleInFrustum(center, extent, worldMatrix))
							DrawBox(center, extent, color, worldMatrix);
					}
				}
			}, collider.GetShape());
	}
	for (auto [entity, collider2D, tr, flag] : registry.view<Collider2DComponent, WorldTransformComponent, FlagComponent>().each())
	{
		if (!HasFlag(flag.flags, EEntityFlag::Active) || HasFlag(flag.flags, EEntityFlag::Dead)) continue;
		const auto& settings = collider2D.GetSettings();
		if (!settings.Enabled) continue;
		const vec4 color = settings.IsTrigger ? vec4(0.f, 1.f, 1.f, 1.f) : vec4(0.f, 1.f, 0.f, 1.f);
		const vec3 offset(collider2D.GetOffset(), 0.f);
		const mat4& worldMatrix = tr.worldMatrix;
		std::visit([&](const auto& shape)
			{
				using ShapeType = std::decay_t<decltype(shape)>;
				if constexpr (std::is_same_v<ShapeType, Box2DShape>)
				{
					const vec3 extent(shape.Size * 0.5f, 0.f);
					if (isVisibleInFrustum(offset, extent, worldMatrix))
						DrawBox(offset, extent, color, worldMatrix);
				}
				else if constexpr (std::is_same_v<ShapeType, Circle2DShape>)
				{
					const vec3 extent(shape.Radius, shape.Radius, 0.f);
					if (isVisibleInFrustum(offset, extent, worldMatrix))
						drawCircleXY(offset, shape.Radius, color, worldMatrix);
				}
				else if constexpr (std::is_same_v<ShapeType, Capsule2DShape>)
				{
					const f32 halfHeight = shape.Height * 0.5f;
					const vec3 extent(shape.Radius, halfHeight, 0.f);
					if (!isVisibleInFrustum(offset, extent, worldMatrix)) return;
					const f32 halfSegment = glm::max(0.f, halfHeight - shape.Radius);
					const vec3 topLocal = offset + vec3(0.f, halfSegment, 0.f);
					const vec3 bottomLocal = offset - vec3(0.f, halfSegment, 0.f);
					drawCircleXY(topLocal, shape.Radius, color, worldMatrix);
					drawCircleXY(bottomLocal, shape.Radius, color, worldMatrix);
					DrawLine(vec3(worldMatrix * vec4(topLocal + vec3(shape.Radius, 0.f, 0.f), 1.f)),
						vec3(worldMatrix * vec4(bottomLocal + vec3(shape.Radius, 0.f, 0.f), 1.f)), color);
					DrawLine(vec3(worldMatrix * vec4(topLocal - vec3(shape.Radius, 0.f, 0.f), 1.f)),
						vec3(worldMatrix * vec4(bottomLocal - vec3(shape.Radius, 0.f, 0.f), 1.f)), color);
				}
			}, collider2D.GetShape());
	}
}
#pragma region Draw Functions
void DebugRenderer::DrawLine(const vec3& start, const vec3& end, const vec4& color)
{
	if (m_Vertices.size() + 2 > 2 * static_cast<sizet>(m_MaxLines)) return;
	m_Vertices.push_back({ start, color });
	m_Vertices.push_back({ end, color });
}

void DebugRenderer::DrawRect(const Rect& rect, const vec4& color)
{
	DrawLine(vec3(rect.left, rect.top, 0.f), vec3(rect.Right(), rect.top, 0.f), color);
	DrawLine(vec3(rect.Right(), rect.top, 0.f), vec3(rect.Right(), rect.Bottom(), 0.f), color);
	DrawLine(vec3(rect.Right(), rect.Bottom(), 0.f), vec3(rect.left, rect.Bottom(), 0.f), color);
	DrawLine(vec3(rect.left, rect.Bottom(), 0.f), vec3(rect.left, rect.top, 0.f), color);
}

void DebugRenderer::DrawBox(const vec3& center, const vec3& extent, const vec4& color, const mat4& transform)
{
	vec3 localCorners[8] = {
		vec3(center.x - extent.x, center.y - extent.y, center.z - extent.z),
		vec3(center.x + extent.x, center.y - extent.y, center.z - extent.z),
		vec3(center.x + extent.x, center.y + extent.y, center.z - extent.z),
		vec3(center.x - extent.x, center.y + extent.y, center.z - extent.z),
		vec3(center.x - extent.x, center.y - extent.y, center.z + extent.z),
		vec3(center.x + extent.x, center.y - extent.y, center.z + extent.z),
		vec3(center.x + extent.x, center.y + extent.y, center.z + extent.z),
		vec3(center.x - extent.x, center.y + extent.y, center.z + extent.z)
	};

	vec3 worldCorners[8];
	for (int i = 0; i < 8; ++i)
		worldCorners[i] = vec3(transform * vec4(localCorners[i], 1.0f));

	// Bottom
	DrawLine(worldCorners[0], worldCorners[1], color); DrawLine(worldCorners[1], worldCorners[2], color);
	DrawLine(worldCorners[2], worldCorners[3], color); DrawLine(worldCorners[3], worldCorners[0], color);
	// Top
	DrawLine(worldCorners[4], worldCorners[5], color); DrawLine(worldCorners[5], worldCorners[6], color);
	DrawLine(worldCorners[6], worldCorners[7], color); DrawLine(worldCorners[7], worldCorners[4], color);
	// Pillars
	DrawLine(worldCorners[0], worldCorners[4], color); DrawLine(worldCorners[1], worldCorners[5], color);
	DrawLine(worldCorners[2], worldCorners[6], color); DrawLine(worldCorners[3], worldCorners[7], color);
}

void DebugRenderer::DrawSphere(const vec3& center, float radius, const vec4& color)
{
	// 원형 근사 (3개 축 기준 원)
	const uint32 segments = 32;
	const float step = glm::two_pi<float>() / (float)segments;

	// XY 평면 (Z축 기준)
	for (uint32 i = 0; i < segments; ++i)
	{
		float a0 = step * i;
		float a1 = step * (i + 1);
		DrawLine(
			center + vec3(cos(a0) * radius, sin(a0) * radius, 0.f),
			center + vec3(cos(a1) * radius, sin(a1) * radius, 0.f), color);
	}
	// XZ 평면 (Y축 기준)
	for (uint32 i = 0; i < segments; ++i)
	{
		float a0 = step * i;
		float a1 = step * (i + 1);
		DrawLine(
			center + vec3(cos(a0) * radius, 0.f, sin(a0) * radius),
			center + vec3(cos(a1) * radius, 0.f, sin(a1) * radius), color);
	}
	// YZ 평면 (X축 기준)
	for (uint32 i = 0; i < segments; ++i)
	{
		float a0 = step * i;
		float a1 = step * (i + 1);
		DrawLine(
			center + vec3(0.f, cos(a0) * radius, sin(a0) * radius),
			center + vec3(0.f, cos(a1) * radius, sin(a1) * radius), color);
	}
}
#pragma endregion
#endif
