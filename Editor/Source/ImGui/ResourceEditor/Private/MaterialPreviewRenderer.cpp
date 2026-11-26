#include "MaterialPreviewRenderer.h"
#include "EditorUI.h"
#include "Model.h"
#include "ModelConverter.h"
#include "PlatformPath.h"
#include "ResourceManager.h"
#include "TransformComponent.h"
#include "StaticMeshRendererComponent.h"
#include "IconsFontAwesome7.h"
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <glm/gtc/type_ptr.hpp>
#include <limits>

namespace
{
	template<typename TNode, typename GetLocalFn>
	glm::mat4 ComputeHierarchyWorldMatrix(const std::vector<TNode>& nodes, size_t nodeIndex,
		std::vector<glm::mat4>& cachedMatrices, std::vector<uint8>& visited, const GetLocalFn& getLocal)
	{
		if (visited[nodeIndex] == 2) return cachedMatrices[nodeIndex];
		if (visited[nodeIndex] == 1) return glm::mat4(1.0f);

		visited[nodeIndex] = 1;
		const auto& node = nodes[nodeIndex];
		const glm::mat4 local = getLocal(node);
		glm::mat4 world = local;
		if (node.ParentIndex >= 0 && static_cast<size_t>(node.ParentIndex) < nodes.size())
		{
			world = ComputeHierarchyWorldMatrix(
				nodes, static_cast<size_t>(node.ParentIndex), cachedMatrices, visited, getLocal) * local;
		}
		cachedMatrices[nodeIndex] = world;
		visited[nodeIndex] = 2;
		return world;
	}

	glm::vec3 SafeNormalizeVec3(const glm::vec3& vec, const glm::vec3& fallback)
	{
		const float len2 = glm::dot(vec, vec);
		return len2 > 1e-8f ? (vec / std::sqrt(len2)) : fallback;
	}

	Engine::ResourceHandle<Engine::Mesh> FinalizeNormalizedPreviewMesh(
		Engine::ResourceManager& rm, const wchar_t* previewMeshKey,
		std::vector<Engine::VertexPosition>& positions,
		std::vector<Engine::VertexMaterial>& attributes,
		std::vector<Engine::uint32>& indices,
		const glm::vec3& boundsMin, const glm::vec3& boundsMax)
	{
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

		return rm.AddResource<Engine::Mesh>(previewMeshKey, mesh);
	}

	Engine::ResourceHandle<Engine::Mesh> BuildPreviewMeshFromModel(
		Engine::ResourceManager& rm, const wchar_t* previewMeshKey, const Engine::Model& model)
	{
		const auto& meshes = model.GetMeshes();
		if (meshes.empty())
			return {};

		std::vector<Engine::VertexPosition> positions;
		std::vector<Engine::VertexMaterial> attributes;
		std::vector<Engine::uint32> indices;
		glm::vec3 boundsMin(std::numeric_limits<float>::max());
		glm::vec3 boundsMax(std::numeric_limits<float>::lowest());

		auto appendRuntimeMesh = [&](const Engine::Mesh* srcMesh, const glm::mat4& worldMat) -> bool
		{
			if (!srcMesh) return false;
			const auto& rawPos = srcMesh->GetRawStreamData(Engine::EMeshStream::Position);
			const auto& rawMat = srcMesh->GetRawStreamData(Engine::EMeshStream::Material);
			const auto& rawIdx = srcMesh->GetRawIndexData();
			const Engine::uint32 posCount = srcMesh->GetStreamCount(Engine::EMeshStream::Position);
			const Engine::uint32 posStride = srcMesh->GetStreamStride(Engine::EMeshStream::Position);
			const Engine::uint32 matCount = srcMesh->GetStreamCount(Engine::EMeshStream::Material);
			const Engine::uint32 matStride = srcMesh->GetStreamStride(Engine::EMeshStream::Material);
			const Engine::uint32 idxCount = srcMesh->GetIndexCount();
			const Engine::uint32 idxStride = srcMesh->GetIndexStride();

			if (posCount == 0 || posStride < sizeof(Engine::VertexPosition) ||
				rawPos.size() < static_cast<size_t>(posCount) * posStride ||
				matCount != posCount || matStride < sizeof(Engine::VertexMaterial) ||
				rawMat.size() < static_cast<size_t>(matCount) * matStride ||
				idxCount == 0 || (idxStride != sizeof(Engine::uint16) && idxStride != sizeof(Engine::uint32)) ||
				rawIdx.size() < static_cast<size_t>(idxCount) * idxStride)
			{
				return false;
			}

			const glm::mat3 linearMat(worldMat);
			const float det = glm::determinant(linearMat);
			const glm::mat3 normalMat = std::abs(det) > 1e-6f ? glm::transpose(glm::inverse(linearMat)) : glm::mat3(1.0f);
			const auto baseVertex = static_cast<Engine::uint32>(positions.size());

			positions.reserve(positions.size() + posCount);
			attributes.reserve(attributes.size() + posCount);
			for (Engine::uint32 i = 0; i < posCount; ++i)
			{
				Engine::VertexPosition vp{};
				std::memcpy(&vp, rawPos.data() + static_cast<size_t>(i) * posStride, sizeof(Engine::VertexPosition));
				const glm::vec3 pos(worldMat * glm::vec4(vp.position, 1.0f));
				boundsMin = glm::min(boundsMin, pos);
				boundsMax = glm::max(boundsMax, pos);
				positions.push_back({ pos });

				Engine::VertexMaterial attr{};
				std::memcpy(&attr, rawMat.data() + static_cast<size_t>(i) * matStride, sizeof(Engine::VertexMaterial));
				attr.normal = SafeNormalizeVec3(normalMat * attr.normal, glm::vec3(0.0f, 1.0f, 0.0f));
				attr.tangent = SafeNormalizeVec3(normalMat * attr.tangent, glm::vec3(1.0f, 0.0f, 0.0f));
				attr.binormal = SafeNormalizeVec3(normalMat * attr.binormal, glm::vec3(0.0f, 0.0f, 1.0f));
				attributes.push_back(attr);
			}

			indices.reserve(indices.size() + idxCount);
			for (Engine::uint32 i = 0; i < idxCount; ++i)
			{
				Engine::uint32 idx = 0;
				if (idxStride == sizeof(Engine::uint16))
				{
					Engine::uint16 idx16 = 0;
					std::memcpy(&idx16, rawIdx.data() + static_cast<size_t>(i) * idxStride, sizeof(Engine::uint16));
					idx = idx16;
				}
				else
				{
					std::memcpy(&idx, rawIdx.data() + static_cast<size_t>(i) * idxStride, sizeof(Engine::uint32));
				}
				indices.push_back(baseVertex + idx);
			}
			return true;
		};

		bool appendedAnyNodeMesh = false;
		const auto& nodes = model.GetNodes();
		if (!nodes.empty())
		{
			std::vector<glm::mat4> cachedMatrices(nodes.size(), glm::mat4(1.0f));
			std::vector<uint8> visited(nodes.size(), 0);
			auto getLocal = [](const Engine::ModelNode& n) { return n.LocalTransform; };
			for (size_t i = 0; i < nodes.size(); ++i)
			{
				if (nodes[i].MeshIndices.empty()) continue;
				const glm::mat4 worldMat = ComputeHierarchyWorldMatrix(nodes, i, cachedMatrices, visited, getLocal);
				for (const auto meshIdx : nodes[i].MeshIndices)
				{
					if (meshIdx < meshes.size() && appendRuntimeMesh(meshes[meshIdx].Get(), worldMat))
						appendedAnyNodeMesh = true;
				}
			}
		}

		if (!appendedAnyNodeMesh)
		{
			for (const auto& meshHandle : meshes)
				appendRuntimeMesh(meshHandle.Get(), glm::mat4(1.0f));
		}

		return FinalizeNormalizedPreviewMesh(rm, previewMeshKey, positions, attributes, indices, boundsMin, boundsMax);
	}
}

namespace Editor
{
	Engine::ResourceHandle<Engine::Mesh> MaterialPreviewRenderer::GetOrLoadMaterialBallPreviewMesh()
	{
		constexpr const wchar_t* kPreviewMeshKey = L"MaterialBallPreviewMesh";
		auto& rm = Engine::ResourceManager::Get();
		if (auto existing = rm.GetResourceHandle<Engine::Mesh>(kPreviewMeshKey); existing.IsValid())
			return existing;

		// 1. .bammodel 우선 로드 (노드 계층의 축 변환/파츠 배치를 그대로 반영)
		constexpr const wchar_t* kBamModelUris[] = {
			L"Editor://Model/material_ball_in_3d-coat/material_ball_in_3d-coat.bammodel",
			L"Engine://Model/material_ball_in_3d-coat/material_ball_in_3d-coat.bammodel"
		};
		std::error_code ec;
		for (const wchar_t* uri : kBamModelUris)
		{
			const std::filesystem::path modelPath = Engine::PlatformPath::ResolveUri(uri);
			if (modelPath.empty() || !std::filesystem::is_regular_file(modelPath, ec))
				continue;

			auto loadedModel = rm.GetResourceHandle<Engine::Model>(modelPath.wstring());
			if (!loadedModel.IsValid())
				loadedModel = Engine::ResourceHandle<Engine::Model>(rm.LoadFile(modelPath.wstring()));

			if (const auto* modelPtr = loadedModel.Get())
			{
				if (auto built = BuildPreviewMeshFromModel(rm, kPreviewMeshKey, *modelPtr); built.IsValid())
					return built;
			}
		}

		// 2. 폴백: .glb 원본에서 정적 모델 변환
		std::filesystem::path glbPath;
		constexpr const wchar_t* kGlbUris[] = {
			L"Editor://Model/material_ball_in_3d-coat.glb",
			L"Engine://Model/material_ball_in_3d-coat.glb"
		};
		for (const wchar_t* uri : kGlbUris)
		{
			const auto candidate = Engine::PlatformPath::ResolveUri(uri);
			if (!candidate.empty() && std::filesystem::is_regular_file(candidate, ec))
			{
				glbPath = candidate;
				break;
			}
		}
		if (glbPath.empty())
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
				const glm::vec3 pos(worldMat * glm::vec4(v.Position[0], v.Position[1], v.Position[2], 1.0f));
				boundsMin = glm::min(boundsMin, pos);
				boundsMax = glm::max(boundsMax, pos);
				positions.push_back({ pos });

				Engine::VertexMaterial attr{};
				attr.normal = SafeNormalizeVec3(normalMat * glm::vec3(v.Normal[0], v.Normal[1], v.Normal[2]), glm::vec3(0.0f, 1.0f, 0.0f));
				attr.texCoord = glm::vec2(v.TexCoord[0], v.TexCoord[1]);
				attr.texCoord1 = glm::vec2(v.TexCoord1[0], v.TexCoord1[1]);
				attr.tangent = SafeNormalizeVec3(normalMat * glm::vec3(v.Tangent[0], v.Tangent[1], v.Tangent[2]), glm::vec3(1.0f, 0.0f, 0.0f));
				attr.binormal = SafeNormalizeVec3(normalMat * glm::vec3(v.Bitangent[0], v.Bitangent[1], v.Bitangent[2]), glm::vec3(0.0f, 0.0f, 1.0f));
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
			auto getLocal = [](const Tools::ModelConversion::NodeData& n) { return glm::make_mat4(n.LocalTransform.data()); };
			for (size_t i = 0; i < model.Nodes.size(); ++i)
			{
				if (model.Nodes[i].MeshIndices.empty()) continue;
				const glm::mat4 worldMat = ComputeHierarchyWorldMatrix(model.Nodes, i, cachedMatrices, visited, getLocal);
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

		return FinalizeNormalizedPreviewMesh(rm, kPreviewMeshKey, positions, attributes, indices, boundsMin, boundsMax);
	}

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
		m_MaterialBallMesh = GetOrLoadMaterialBallPreviewMesh();

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