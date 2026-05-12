#pragma once

#include "SkyRenderer.h"
#include "Mesh.h"
#include "Material.h"
#include "MaterialInstance.h"
#include "ResourceManager.h"
#include "Renderer.h"
#include "Camera.h"
#include "Transform.h"
#include "GameObject.h"
#include "RenderPass.h"
#include "Vertex.h"
#include "ComponentRegistry.h"


REGISTER_COMPONENT(SkyRenderer)

#pragma region Constructor&Destructor
EResult SkyRenderer::Initialize(void* arg)
{
	if(IsFailure(__super::Initialize(arg)))
		return EResult::Fail;

	m_DrawShadow = false;
	m_ReceiveShadow = false;
    
    if (arg)
    {
        CAST_DESC
        m_Shape = desc->SkyType;
    }
	return EResult::Success;
}

SkyRenderer* SkyRenderer::Create(void* arg)
{
	SkyRenderer* instance = new SkyRenderer();
	if (instance->Initialize(arg) != EResult::Success)
	{
		delete instance;
		return nullptr;
	}
	return instance;
}

Component* SkyRenderer::Clone(GameObject* owner, void* arg)
{
	//SkyRenderer* clone = new SkyRenderer();
	//clone->SetMaterial(GetMaterial(0));
	//clone->m_SunDirection = m_SunDirection;
	//clone->m_SunColor = m_SunColor;
	return nullptr;
}

void SkyRenderer::Free()
{
	RenderComponent::Free();
}

#pragma endregion

#pragma region Loop
EResult SkyRenderer::Render(f32 dt, RenderPass* renderPass)
{
    // 쉐도우 패스는 스킵
    if (renderPass && renderPass->GetPassType() == ERenderPassType::Shadow)
        return EResult::Success;

    if (!m_Mesh.IsValid() || m_LastBuiltShape != m_Shape)
    {
        if (IsFailure(RebuildMesh()))
            return EResult::Fail;
    }

    Mesh* mesh = m_Mesh.Get();
    if (!mesh) return EResult::Fail;

    MaterialInterface* material = GetMaterial();
    if (!material)
    {
        ENGINE_LOG_WARN("SkyRenderer material is null. Owner: {}", WStrToStr(m_Owner->GetName()));
        return EResult::Fail;
    }

    RHI* rhi = Renderer::Get().GetRHI();

    if (IsFailure(material->Bind(2)))
        return EResult::Fail;

    if (IsFailure(BindPipeline(mesh, material, renderPass)))
        return EResult::Fail;

    // ── SceneUBO: 스카이를 카메라 위치에 고정 (무한 원경) ──
    vec3 cameraPos = vec3(0.0f);
    Camera* camera = Renderer::Get().GetViewportCamera(renderPass->GetID());
    if (camera && camera->GetOwner())
    {
        Transform* camTransform = camera->GetOwner()->GetComponent<Transform>();
        if (camTransform)
            cameraPos = camTransform->GetWorldPosition();
    }

    float skyScale = 500.0f;
    if (camera && camera->GetIsOrthographic())
    {
        const float orthoHalfH = camera->GetOrthoSize();
        const float orthoHalfW = orthoHalfH * camera->GetAspect();
        const float orthoMaxHalfExtent = glm::max(orthoHalfW, orthoHalfH);

        // 여유 배수(4~8 사이 취향 조정 가능). 먼저 6으로 시작.
        skyScale = glm::max(orthoMaxHalfExtent * 6.0f, 10.0f);
    }

    SceneUBO sceneUBO;
    sceneUBO.worldMatrix = glm::scale(mat4(1.0f), vec3(skyScale));

    if (IsFailure(rhi->BindConstantBuffer((void*)&sceneUBO, sizeof(SceneUBO), 1, EShaderType::Vertex)))
        return EResult::Fail;

    // ── SkyAtmosphereUBO: 대기 파라미터 (slot 3) ──
	tagSkyAtmosphereDesc atmosphereUBO = BuildSkyAtmosphereDesc();

    if (IsFailure(rhi->BindConstantBuffer((void*)&atmosphereUBO, sizeof(tagSkyAtmosphereDesc), 1, EShaderType::Pixel)))
        return EResult::Fail;

    if (IsFailure(mesh->Bind(0)))
        return EResult::Fail;

    return mesh->GetIndexBuffer()
        ? rhi->DrawIndexed(mesh->GetIndexCount())
        : rhi->Draw(mesh->GetVertexCount());
}
#pragma endregion

#pragma region Getter
tagSkyAtmosphereDesc SkyRenderer::BuildSkyAtmosphereDesc() const
{
    tagSkyAtmosphereDesc atmosphereDesc;
    atmosphereDesc.ZenithColor = m_ZenithColor;
    atmosphereDesc.HorizonColor = m_HorizonColor;
    atmosphereDesc.GroundColor = m_GroundColor;
    atmosphereDesc.SunDirection = glm::normalize(m_SunDirection);
    atmosphereDesc.HorizonBlend = m_HorizonBlend;
    atmosphereDesc.SunColor = m_SunColor;
    atmosphereDesc.SunIntensity = m_SunIntensity;
    atmosphereDesc.SkyIntensity = m_SkyIntensity;
    atmosphereDesc.SunSize = m_SunSize;
    return atmosphereDesc;
}
#pragma endregion

#pragma region Geometry
EResult SkyRenderer::BuildSphereMesh()
{
    m_Mesh = ResourceManager::Get().GetResourceHandle<Mesh>(L"__SkyMesh_Sphere__");
    if (m_Mesh.IsValid())
        return EResult::Success;

    const int32 stacks = m_SphereStacks;
    const int32 slices = m_SphereSlices;

    vector<VertexPosition> positions;
    vector<VertexMaterial> materials;
    vector<uint32>         indices;

    positions.reserve((stacks + 1) * (slices + 1));
    materials.reserve((stacks + 1) * (slices + 1));
    indices.reserve(stacks * slices * 6);

    for (int32 s = 0; s <= stacks; ++s)
    {
        f32 phi = glm::pi<f32>() * s / stacks;
        f32 y = glm::cos(phi);
        f32 r = glm::sin(phi);

        for (int32 sl = 0; sl <= slices; ++sl)
        {
            f32 theta = glm::two_pi<f32>() * sl / slices;
            f32 x = r * glm::cos(theta);
            f32 z = r * glm::sin(theta);

            vec3 pos = vec3(x, y, z);
            vec3 normal = -pos;                  // 안쪽 법선
            vec2 uv = vec2((f32)sl / slices, (f32)s / stacks);

            positions.push_back({ pos });
            materials.push_back({ normal, uv, vec3(0.0f), vec3(0.0f), vec4(1.0f) });
        }
    }

    // CW 와인딩 (내부에서 볼 때 올바른 방향)
    for (int32 s = 0; s < stacks; ++s)
    {
        for (int32 sl = 0; sl < slices; ++sl)
        {
            uint32 curr = s * (slices + 1) + sl;
            uint32 next = curr + (slices + 1);

            indices.push_back(curr);     indices.push_back(next);     indices.push_back(curr + 1);
            indices.push_back(curr + 1); indices.push_back(next);     indices.push_back(next + 1);
        }
    }

    tagMeshCreateDesc desc;
    desc.Streams[(uint32)EMeshStream::Position] = { positions.data(), (uint32)positions.size(), sizeof(VertexPosition) };
    desc.Streams[(uint32)EMeshStream::Material] = { materials.data(), (uint32)materials.size(), sizeof(VertexMaterial) };
    desc.IndexData = indices.data();
    desc.IndexCount = (uint32)indices.size();
    desc.IndexStride = sizeof(uint32);
    desc.BoundingBoxMin = vec3(-1.0f);
    desc.BoundingBoxMax = vec3(1.0f);

    m_Mesh = ResourceManager::Get().AddResource(L"__SkyMesh_Sphere__", Mesh::Create(&desc));
    return m_Mesh.IsValid() ? EResult::Success : EResult::Fail;
}
EResult SkyRenderer::BuildCubeMesh()
{
    m_Mesh = ResourceManager::Get().GetResourceHandle<Mesh>(L"__SkyMesh_Cube__");
    if (m_Mesh.IsValid())
        return EResult::Success;

    struct FaceInfo { vec3 right; vec3 up; vec3 forward; };
    const FaceInfo faces[6] = {
        { vec3(0, 0,-1), vec3(0, 1, 0), vec3(1, 0, 0) },  // +X
        { vec3(0, 0, 1), vec3(0, 1, 0), vec3(-1, 0, 0) },  // -X
        { vec3(1, 0, 0), vec3(0, 0,-1), vec3(0, 1, 0) },  // +Y
        { vec3(1, 0, 0), vec3(0, 0, 1), vec3(0,-1, 0) },  // -Y
        { vec3(1, 0, 0), vec3(0, 1, 0), vec3(0, 0, 1) },  // +Z
        { vec3(-1,0, 0), vec3(0, 1, 0), vec3(0, 0,-1) },  // -Z
    };

    vector<VertexPosition> positions;
    vector<VertexMaterial> materials;
    vector<uint32>         indices;

    positions.reserve(24);
    materials.reserve(24);
    indices.reserve(36);

    for (int32 f = 0; f < 6; ++f)
    {
        const FaceInfo& face = faces[f];
        vec3 inwardNormal = -face.forward;

        vec3 corners[4] = {
            face.forward - face.right - face.up,
            face.forward + face.right - face.up,
            face.forward + face.right + face.up,
            face.forward - face.right + face.up,
        };
        vec2 uvs[4] = { vec2(0,1), vec2(1,1), vec2(1,0), vec2(0,0) };

        uint32 base = (uint32)positions.size();
        for (int32 i = 0; i < 4; ++i)
        {
            positions.push_back({ corners[i] });
            materials.push_back({ inwardNormal, uvs[i], vec3(0.0f), vec3(0.0f), vec4(1.0f) });
        }

        // CW 와인딩
        indices.push_back(base + 0); indices.push_back(base + 2); indices.push_back(base + 1);
        indices.push_back(base + 0); indices.push_back(base + 3); indices.push_back(base + 2);
    }

    tagMeshCreateDesc desc;
    desc.Streams[(uint32)EMeshStream::Position] = { positions.data(), (uint32)positions.size(), sizeof(VertexPosition) };
    desc.Streams[(uint32)EMeshStream::Material] = { materials.data(), (uint32)materials.size(), sizeof(VertexMaterial) };
    desc.IndexData = indices.data();
    desc.IndexCount = (uint32)indices.size();
    desc.IndexStride = sizeof(uint32);
    desc.BoundingBoxMin = vec3(-1.0f);
    desc.BoundingBoxMax = vec3(1.0f);

    m_Mesh = ResourceManager::Get().AddResource(L"__SkyMesh_Cube__", Mesh::Create(&desc));
    return m_Mesh.IsValid() ? EResult::Success : EResult::Fail;
}
EResult SkyRenderer::RebuildMesh()
{
	EResult result = (m_Shape == ESkyShape::Sphere)
		? BuildSphereMesh()
		: BuildCubeMesh();

	if (IsSuccess(result))
		m_LastBuiltShape = m_Shape;

	return result;
}
#pragma endregion