#pragma once

#include "DebugRenderer.h"
#include "Renderer.h"
#include "ResourceManager.h"
#include "PipelineManager.h"
#include "RenderTargetManager.h"
#include "Layer.h"

void DebugRenderer::Initialize(const wstring& prefix)
{
	ResourceManager& rm = ResourceManager::Get();

	tagShaderDesc vsDesc = {};
	vsDesc.Key = L"DebugLineVS";
	vsDesc.Path = L"Resources/Shader/debugLine.vert.spv";
	vsDesc.SpirvPath = L"Resources/Shader/debugLine.vert.spv";
	vsDesc.ShaderType = EShaderType::Vertex;
	vsDesc.EntryPoint = "main";
	rm.LoadResource<Shader>(&vsDesc);
	rm.SaveToBinaryFile(rm.GetResourceHandle<Shader>(L"DebugLineVS").Get(), L"Resources/Shader/debugLine.vert.bamshader");

	tagShaderDesc psDesc = {};
	psDesc.Key = L"DebugLinePS";
	psDesc.Path = L"Resources/Shader/debugLine.frag.spv";
	psDesc.SpirvPath = L"Resources/Shader/debugLine.frag.spv";
	psDesc.ShaderType = EShaderType::Pixel;
	psDesc.EntryPoint = "main";
	rm.LoadResource<Shader>(&psDesc);
	rm.SaveToBinaryFile(rm.GetResourceHandle<Shader>(L"DebugLinePS").Get(), L"Resources/Shader/debugLine.frag.bamshader");

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
		vec4(0.0f, 0.f, 0.f, -1.f), 500, ERenderSortType::None, ERenderPassType::Custom);
}

void DebugRenderer::Free()
{
	Safe_Release(m_VertexBuffer);
}

void DebugRenderer::SubmitDebugDraw(Camera* camera, const wstring& colorRTName)
{
	RegisterColliderDebug(camera);
	if (m_Vertices.empty() || !camera || colorRTName.empty()) return;
	RenderPass* pass = Engine::RenderPassManager::Get().GetRenderPassByID(m_RenderPassID);
	pass->SetColorAttachments({	colorRTName	});
	Renderer::Get().RegisterViewportCamera(camera, pass);

	Engine::Renderer::Get().SubmitCustomCommand(
		[this](f32 dt, Engine::RenderPass* pass)->EResult
		{
			if (m_Vertices.empty()) return EResult::Success;

			Engine::RHI* rhi = Engine::Renderer::Get().GetRHI();
			ResourceManager& rm = ResourceManager::Get();

			// 파이프라인 디스크립터 (Grid와 동일한 방식으로 RenderPass에서 포맷 동적 매칭)
			tagRHIPipelineDesc desc = {};
			desc.PipelineType = EPipelineType::Graphics;
			desc.BlendMode = EBlendMode::AlphaBlend;
			desc.FillMode = EFillMode::Solid;
			desc.CullMode = ECullMode::None;
			desc.Topology = ETopology::LineList;

			desc.VertexShader = rm.GetResourceHandle<Shader>(L"DebugLineVS").Get()->GetRHIShader();
			desc.PixelShader = rm.GetResourceHandle<Shader>(L"DebugLinePS").Get()->GetRHIShader();
			desc.InputLayouts.push_back(Engine::DebugVertex::Layout);

			desc.DepthStencilState.DepthTestEnable = false;
			desc.DepthStencilState.DepthWriteEnable = false;
			desc.DepthStencilState.StencilTestEnable = false;
			desc.DepthStencilState.DepthCompareOp = ECompareOp::Always;

			// [핵심] RenderPass에서 Color Attachment 포맷 동적 매칭
			desc.ColorAttachmentCount = pass->GetRenderTargetCount();
			//for (uint32 i = 0; i < desc.ColorAttachmentCount; ++i)
			//{
			//	desc.ColorAttachmentFormats[i] = Engine::RenderTargetManager::Get()
			//		.GetRenderTarget(pass->GetRenderTargetName(i))->GetFormat();
			//}

			// [핵심] RenderPass에서 Depth Stencil 포맷 동적 매칭
			//wstring depthStencilName = pass->GetDepthStencilName();
			//if (!depthStencilName.empty())
			//{
			//	desc.DepthStencilAttachmentFormat = Engine::RenderTargetManager::Get()
			//		.GetRenderTarget(depthStencilName)->GetFormat();
			//}

			RHIPipeline* pipeline = PipelineManager::Get().GetOrCreatePipeline(desc);
			if (!pipeline || IsFailure(rhi->BindPipeline(pipeline)))
				return EResult::Fail;

			m_VertexBuffer->SetData(m_Vertices.data(), (uint32)m_Vertices.size() * sizeof(DebugVertex));

			RHIBuffer* buffers[1] = { m_VertexBuffer };
			rhi->BindVertexBuffers(0, buffers, 1);

			return rhi->Draw((uint32)m_Vertices.size());
		}, m_RenderPassID);

	m_Vertices.clear();
}

void DebugRenderer::RegisterColliderDebug(Camera* camera)
{
	Scene* scene = SceneManager::Get().GetCurrentScene();
	if (!scene) return;
	// 카메라의 ViewProjection으로 프러스텀 추출
	tagFrustum frustum = {};
	bool useFrustum = false;
	if (camera)
	{
		frustum = FrustumCuller::ExtractFrustum(camera->GetCameraBuffer().viewProjMatrix);
		useFrustum = true;
	}
	const vec4 color = vec4(0.f, 1.f, 0.f, 1.f);
	for (auto* layer : scene->GetAllLayers())
	{
		for (auto* go : layer->GetAllGameObjects())
		{
			if (!go || !go->IsActive()) continue;
			auto* col = go->GetComponent<Collider>();
			if (!col || !col->IsActive()) continue;
			mat4 worldMatrix = go->GetTransform()->GetWorldMatrix();
			tagColliderDesc desc = col->GetColliderDesc();
			// 프러스텀 컬링: 콜라이더 AABB가 카메라 밖이면 스킵
			if (useFrustum)
			{
				AABB localAABB(desc.center - desc.extent, desc.center + desc.extent);
				AABB worldAABB = FrustumCuller::TransformAABB(localAABB, worldMatrix);
				if (!FrustumCuller::TestAABB(frustum, worldAABB))
					continue;
			}
			switch (desc.type)
			{
			case EColliderType::Box:
			case EColliderType::Box2D:
				DrawBox(desc.center, desc.extent, color, worldMatrix);
				break;
			case EColliderType::Sphere:
				DrawSphere(vec3(worldMatrix[3]) + desc.center, desc.extent.x, color);
				break;
			case EColliderType::Capsule:
				break;
			case EColliderType::Mesh:
				break;
			}
		}
	}
}

#pragma region Draw Functions
void DebugRenderer::DrawLine(const vec3& start, const vec3& end, const vec4& color)
{
	m_Vertices.push_back({ start, color });
	m_Vertices.push_back({ end, color });
}

void DebugRenderer::DrawRect(const Rect& rect, const vec4& color)
{
	DrawLine(vec3(rect.Left, rect.Top, 0.f), vec3(rect.Right(), rect.Top, 0.f), color);
	DrawLine(vec3(rect.Right(), rect.Top, 0.f), vec3(rect.Right(), rect.Bottom(), 0.f), color);
	DrawLine(vec3(rect.Right(), rect.Bottom(), 0.f), vec3(rect.Left, rect.Bottom(), 0.f), color);
	DrawLine(vec3(rect.Left, rect.Bottom(), 0.f), vec3(rect.Left, rect.Top, 0.f), color);
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
