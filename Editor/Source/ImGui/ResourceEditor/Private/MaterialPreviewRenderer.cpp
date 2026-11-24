#include "MaterialPreviewRenderer.h"
#include "EditorUI.h"
#include "ModelConverter.h"
#include "PlatformPath.h"
#include "ResourceManager.h"
#include "TransformComponent.h"
#include "StaticMeshRendererComponent.h"
#include "IconsFontAwesome7.h"
#include <algorithm>
#include <filesystem>
#include <glm/gtc/type_ptr.hpp>
#include <limits>

namespace
{
	glm::mat4 ComputeNodeWorldMatrix(const std::vector<Tools::ModelConversion::NodeData>& nodes, size_t nodeIndex,
		std::vector<glm::mat4>& cachedMatrices, std::vector<uint8>& visited)
	{
		if (visited[nodeIndex] == 2) return cachedMatrices[nodeIndex];
		if (visited[nodeIndex] == 1) return glm::mat4(1.0f);

		visited[nodeIndex] = 1;
		const auto& node = nodes[nodeIndex];
		const glm::mat4 local = glm::make_mat4(node.LocalTransform.data());
		glm::mat4 world = local;
		if (node.ParentIndex >= 0 && static_cast<size_t>(node.ParentIndex) < nodes.size())
		{
			world = ComputeNodeWorldMatrix(nodes, static_cast<size_t>(node.ParentIndex), cachedMatrices, visited) * local;
		}
		cachedMatrices[nodeIndex] = world;
		visited[nodeIndex] = 2;
		return world;
	}

	Engine::ResourceHandle<Engine::Mesh> LoadMaterialBallPreviewMesh()
	{
		constexpr const wchar_t* kPreviewMeshKey = L"MaterialBallPreviewMesh";
		auto& rm = Engine::ResourceManager::Get();
		if (auto existing = rm.GetResourceHandle<Engine::Mesh>(kPreviewMeshKey); existing.IsValid())
			return existing;

		const std::filesystem::path glbPath = Engine::PlatformPath::ResolveUri(L"Engine://Model/material_ball_in_3d-coat.glb");
		std::error_code ec;
		if (glbPath.empty() || !std::filesystem::is_regular_file(glbPath, ec))
			return {};

		Tools::ModelConversion::Options opt{};
		opt.FlipUVs = true;
		opt.GenerateLODs = false;
		const auto result = Tools::ModelConversion::ConvertStaticModel(glbPath, opt);
		if (!result || !result.Model.has_value() || result.Model->Meshes.empty())
			return {};

		const auto& model = *result.Model;
		std::vector<Engine::VertexPosition> positions;
		std::vector<Engine::VertexMaterial> attributes;
		std::vector<Engine::uint32> indices;
		glm::vec3 boundsMin(std::numeric_limits<float>::max());
		glm::vec3 boundsMax(std::numeric_limits<float>::lowest());

		auto appendMesh = [&](const Tools::ModelConversion::MeshData& srcMesh, const glm::mat4& worldMat)
		{
			if (srcMesh.Vertices.empty() || srcMesh.Indices.empty()) return;

			const glm::mat3 linearMat(worldMat);
			const float det = glm::determinant(linearMat);
			const glm::mat3 normalMat = std::abs(det) > 1e-6f ? glm::transpose(glm::inverse(linearMat)) : glm::mat3(1.0f);
			const auto baseVertex = static_cast<Engine::uint32>(positions.size());

			positions.reserve(positions.size() + srcMesh.Vertices.size());
			attributes.reserve(attributes.size() + srcMesh.Vertices.size());
			for (const auto& v : srcMesh.Vertices)
			{
				const glm::vec4 worldPos = worldMat * glm::vec4(v.Position[0], v.Position[1], v.Position[2], 1.0f);
				const glm::vec3 pos(worldPos);
				boundsMin = glm::min(boundsMin, pos);
				boundsMax = glm::max(boundsMax, pos);
				positions.push_back({ pos });

				auto safeNormalize = [](const glm::vec3& vec, const glm::vec3& fallback) -> glm::vec3
				{
					const float len2 = glm::dot(vec, vec);
					return len2 > 1e-8f ? (vec / std::sqrt(len2)) : fallback;
				};

				Engine::VertexMaterial attr{};
				attr.normal = safeNormalize(normalMat * glm::vec3(v.Normal[0], v.Normal[1], v.Normal[2]), glm::vec3(0.0f, 1.0f, 0.0f));
				attr.texCoord = glm::vec2(v.TexCoord[0], v.TexCoord[1]);
				attr.texCoord1 = glm::vec2(v.TexCoord1[0], v.TexCoord1[1]);
				attr.tangent = safeNormalize(normalMat * glm::vec3(v.Tangent[0], v.Tangent[1], v.Tangent[2]), glm::vec3(1.0f, 0.0f, 0.0f));
				attr.binormal = safeNormalize(normalMat * glm::vec3(v.Bitangent[0], v.Bitangent[1], v.Bitangent[2]), glm::vec3(0.0f, 0.0f, 1.0f));
				attr.color = glm::vec4(v.Color[0], v.Color[1], v.Color[2], v.Color[3]);
				attributes.push_back(attr);
			}

			indices.reserve(indices.size() + srcMesh.Indices.size());
			for (const auto idx : srcMesh.Indices)
				indices.push_back(baseVertex + idx);
		};

		bool appendedAnyNodeMesh = false;
		if (!model.Nodes.empty())
		{
			std::vector<glm::mat4> cachedMatrices(model.Nodes.size(), glm::mat4(1.0f));
			std::vector<uint8> visited(model.Nodes.size(), 0);
			for (size_t i = 0; i < model.Nodes.size(); ++i)
			{
				if (model.Nodes[i].MeshIndices.empty()) continue;
				const glm::mat4 worldMat = ComputeNodeWorldMatrix(model.Nodes, i, cachedMatrices, visited);
				for (const auto meshIdx : model.Nodes[i].MeshIndices)
				{
					if (meshIdx < model.Meshes.size())
					{
						appendMesh(model.Meshes[meshIdx], worldMat);
						appendedAnyNodeMesh = true;
					}
				}
			}
		}

		if (!appendedAnyNodeMesh)
		{
			for (const auto& meshData : model.Meshes)
				appendMesh(meshData, glm::mat4(1.0f));
		}

		if (positions.empty() || indices.empty())
			return {};

		const glm::vec3 center = (boundsMin + boundsMax) * 0.5f;
		const glm::vec3 extent = boundsMax - boundsMin;
		const float maxExtent = (std::max)({ extent.x, extent.y, extent.z });
		const float invScale = maxExtent > 1e-5f ? (1.0f / maxExtent) : 1.0f;
		for (auto& vp : positions)
			vp.position = (vp.position - center) * invScale;

		const auto vertexCount = static_cast<Engine::uint32>(positions.size());
		Engine::MeshCreateDesc meshDesc{};
		meshDesc.Streams[static_cast<Engine::uint32>(Engine::EMeshStream::Position)] = { positions.data(), vertexCount, sizeof(Engine::VertexPosition) };
		meshDesc.Streams[static_cast<Engine::uint32>(Engine::EMeshStream::Material)] = { attributes.data(), vertexCount, sizeof(Engine::VertexMaterial) };
		meshDesc.IndexData = indices.data();
		meshDesc.IndexCount = static_cast<Engine::uint32>(indices.size());
		meshDesc.IndexStride = sizeof(Engine::uint32);
		meshDesc.BoundingBoxMin = (boundsMin - center) * invScale;
		meshDesc.BoundingBoxMax = (boundsMax - center) * invScale;

		auto* mesh = Engine::Mesh::Create(&meshDesc);
		if (!mesh)
			return {};

		return rm.AddResource<Engine::Mesh>(kPreviewMeshKey, mesh);
	}
}

namespace Editor
{
	MaterialPreviewRenderer::~MaterialPreviewRenderer()
	{
		Free();
	}

	void MaterialPreviewRenderer::Initialize(uint32 width, uint32 height)
	{
		// 1. 공통 3D 프리뷰 위젯 초기화 (바닥 그리드 기본 On)
		m_Widget.Initialize(L"MatPreview", width, height, true);

		// 2. 프리뷰 메쉬 리소스 로드
		auto& rm = Engine::ResourceManager::Get();
		m_SphereMesh = rm.GetResourceHandle<Engine::Mesh>(L"SphereMesh");
		m_CubeMesh = rm.GetResourceHandle<Engine::Mesh>(L"CubeMesh");
		m_QuadMesh = rm.GetResourceHandle<Engine::Mesh>(L"QuadMesh");
		m_PlaneMesh = rm.GetResourceHandle<Engine::Mesh>(L"PlaneMesh");
		m_MaterialBallMesh = LoadMaterialBallPreviewMesh();

		// 3. 프리뷰 대상 메쉬 엔티티 생성 (컴포넌트만 부착하면 위젯이 자동 렌더링)
		auto& entity = m_Widget.CreateEntity(L"MaterialPreviewMesh");
		m_MeshEntity = entity.GetEntityHandle();

		auto& meshComp = m_Widget.GetRegistry().emplace<Engine::StaticMeshRendererComponent>(m_MeshEntity);
		meshComp.meshHandle = m_MaterialBallMesh.IsValid() ? m_MaterialBallMesh : m_SphereMesh;
		meshComp.castShadow = true;

		// 4. 카메라 초기 포커스 세팅
		m_Widget.FocusTarget(Engine::vec3(0.0f, 0.0f, 0.0f), 3.0f);
	}

	void MaterialPreviewRenderer::Free()
	{
		m_Widget.Free();
		m_MeshEntity = entt::null;
	}

	void MaterialPreviewRenderer::ReleaseMaterial()
	{
		if (m_MeshEntity == entt::null) return;
		if (auto* mesh = m_Widget.GetRegistry().try_get<Engine::StaticMeshRendererComponent>(m_MeshEntity))
			mesh->materialHandle.Reset();
	}

	void MaterialPreviewRenderer::UpdatePreviewMesh(Engine::MaterialInterface* material)
	{
		if (m_MeshEntity == entt::null) return;

		auto& registry = m_Widget.GetRegistry();
		auto* meshComp = registry.try_get<Engine::StaticMeshRendererComponent>(m_MeshEntity);
		if (!meshComp) return;

		switch (m_CurrentMeshType)
		{
		case EPreviewMeshType::MaterialBall:
			meshComp->meshHandle = m_MaterialBallMesh.IsValid() ? m_MaterialBallMesh : m_SphereMesh;
			break;
		case EPreviewMeshType::Sphere: meshComp->meshHandle = m_SphereMesh; break;
		case EPreviewMeshType::Cube:   meshComp->meshHandle = m_CubeMesh;   break;
		case EPreviewMeshType::Quad:   meshComp->meshHandle = m_QuadMesh;   break;
		case EPreviewMeshType::Plane:  meshComp->meshHandle = m_PlaneMesh;  break;
		default: break;
		}

		if (material)
		{
			auto handle = Engine::ResourceManager::Get().GetResourceHandle<Engine::MaterialInterface>(material->GetKey());
			if (meshComp->materialHandle != handle)
			{
				meshComp->materialHandle = handle;
			}
		}
	}

	void MaterialPreviewRenderer::Submit(Engine::MaterialInterface* material, f32 dt)
	{
		if (!material || m_MeshEntity == entt::null) return;

		// 1. 머티리얼 및 메쉬 동기화
		UpdatePreviewMesh(material);

		// 2. 메쉬 자전 (자전 시 조명 하이라이트가 회전하면서 PBR 특성 시각화)
		if (m_AutoRotate)
		{
			auto* trans = m_Widget.GetRegistry().try_get<Engine::TransformComponent>(m_MeshEntity);
			if (trans)
			{
				(void)trans->RotateLocal(glm::vec3(0.0f, 1.0f, 0.0f), static_cast<double>(m_AutoRotateSpeed * dt));
			}
		}

		// 3. 위젯 프레임 틱 & 자동 렌더링 제출
		m_Widget.BeginFrame(dt);
		m_Widget.Submit(dt);
	}

	void MaterialPreviewRenderer::ResetView()
	{
		auto* trans = m_Widget.GetRegistry().try_get<Engine::TransformComponent>(m_MeshEntity);
		if (trans) (void)trans->SetEulerRotation(Engine::vec3(0.0f));
		m_Widget.FocusTarget(Engine::vec3(0.0f), 3.0f);
		m_AutoRotate = true;
	}

	void MaterialPreviewRenderer::DrawViewportOnly(const ImVec2& size)
	{
		ImVec2 targetSize = size;
		if (targetSize.x <= 0.0f || targetSize.y <= 0.0f)
		{
			targetSize = ImGui::GetContentRegionAvail();
			targetSize.x = (std::max)(64.0f, targetSize.x);
			targetSize.y = (std::max)(64.0f, targetSize.y);
		}
		m_Widget.Draw(targetSize);
	}

	void MaterialPreviewRenderer::DrawUI(Engine::MaterialInterface* material)
	{
		EditorUI::ToolbarLayout toolbar;
		EditorUI::IconButtonOptions meshOptions;
		meshOptions.Active = m_CurrentMeshType == EPreviewMeshType::MaterialBall;
		if (toolbar.Icon("MaterialBall", ICON_FA_SHAPES, "MaterialPreview.MaterialBall", meshOptions))
			m_CurrentMeshType = EPreviewMeshType::MaterialBall;

		meshOptions.Active = m_CurrentMeshType == EPreviewMeshType::Sphere;
		if (toolbar.Icon("Sphere", ICON_FA_GLOBE, "MaterialPreview.Sphere", meshOptions))
			m_CurrentMeshType = EPreviewMeshType::Sphere;

		meshOptions.Active = m_CurrentMeshType == EPreviewMeshType::Cube;
		if (toolbar.Icon("Cube", ICON_FA_CUBE, "MaterialPreview.Cube", meshOptions))
			m_CurrentMeshType = EPreviewMeshType::Cube;

		meshOptions.Active = m_CurrentMeshType == EPreviewMeshType::Quad;
		if (toolbar.Icon("Quad", ICON_FA_SQUARE, "MaterialEditor.Quad", meshOptions))
			m_CurrentMeshType = EPreviewMeshType::Quad;

		meshOptions.Active = m_CurrentMeshType == EPreviewMeshType::Plane;
		if (toolbar.Icon("Plane", ICON_FA_TABLE_CELLS_LARGE, "MaterialPreview.Plane", meshOptions))
			m_CurrentMeshType = EPreviewMeshType::Plane;

		if (toolbar.Icon("AutoRotate", m_AutoRotate ? ICON_FA_PAUSE : ICON_FA_PLAY,
			m_AutoRotate ? "EditorCommon.Pause" : "EditorCommon.Play"))
			m_AutoRotate = !m_AutoRotate;

		if (toolbar.Icon("ResetView", ICON_FA_ROTATE_LEFT, "GraphPanel.ResetView"))
		{
			ResetView();
		}

		ImGui::Separator();

		// 2. 가용 영역 전체에 맞추어 공통 프리뷰 위젯 렌더링
		DrawViewportOnly();
	}
}