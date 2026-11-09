#include "BuiltInPrimitiveMeshes.h"
#include "Mesh.h"
#include "ResourceManager.h"
#include <glm/gtc/constants.hpp>

namespace Engine
{
    void RegisterBuiltInPrimitiveMeshes(ResourceManager& resourceManager)
    {
        using std::vector;

#pragma region Quad
        {
            vector<VertexPosition> positions = {
                { { -1.0f,  1.0f, 0.0f } },
                { {  1.0f,  1.0f, 0.0f } },
                { {  1.0f, -1.0f, 0.0f } },
                { { -1.0f, -1.0f, 0.0f } }
            };
            vector<VertexMaterial> materials = {
                { { 0, 0, -1 }, { 0, 0 }, { 0, 0, 0 }, { 0, 1, 0 }, vec4(1.0f) },
                { { 0, 0, -1 }, { 1, 0 }, { 0, 0, 0 }, { 0, 1, 0 }, vec4(1.0f) },
                { { 0, 0, -1 }, { 1, 1 }, { 0, 0, 0 }, { 0, 1, 0 }, vec4(1.0f) },
                { { 0, 0, -1 }, { 0, 1 }, { 0, 0, 0 }, { 0, 1, 0 }, vec4(1.0f) }
            };
            vector<uint32> indices = { 0, 1, 2, 0, 2, 3 };

            MeshCreateDesc meshDesc = {};
            meshDesc.Streams[(uint32)EMeshStream::Position] = { positions.data(), (uint32)positions.size(), sizeof(VertexPosition) };
            meshDesc.Streams[(uint32)EMeshStream::Material] = { materials.data(), (uint32)materials.size(), sizeof(VertexMaterial) };
            meshDesc.IndexData = indices.data();
            meshDesc.IndexCount = static_cast<uint32>(indices.size());
            meshDesc.IndexStride = sizeof(uint32);

            resourceManager.AddResource<Mesh>(L"QuadMesh", Mesh::Create(&meshDesc));
        }
#pragma endregion

#pragma region Cube
        {
            // 1. 기존 통짜 Vertex 배열은 그대로 유지 (데이터 소스)
            vector<Vertex> vertices = {
                // ---- 기존 24개 정점 데이터를 그대로 두세요 (변경 없음) ----
                // 앞면, 뒷면, 윗면, 아랫면, 우측면, 좌측면 (1m 단위 큐브: [-0.5, 0.5])
                {{ -0.5f,  0.5f,  0.5f }, { 0, 0, 1 }, { 0, 0 }, { 1, 0, 0 }},
                {{  0.5f,  0.5f,  0.5f }, { 0, 0, 1 }, { 1, 0 }, { 1, 0, 0 }},
                {{  0.5f, -0.5f,  0.5f }, { 0, 0, 1 }, { 1, 1 }, { 1, 0, 0 }},
                {{ -0.5f, -0.5f,  0.5f }, { 0, 0, 1 }, { 0, 1 }, { 1, 0, 0 }},
                {{  0.5f,  0.5f, -0.5f }, { 0, 0,-1 }, { 0, 0 }, {-1, 0, 0 }},
                {{ -0.5f,  0.5f, -0.5f }, { 0, 0,-1 }, { 1, 0 }, {-1, 0, 0 }},
                {{ -0.5f, -0.5f, -0.5f }, { 0, 0,-1 }, { 1, 1 }, {-1, 0, 0 }},
                {{  0.5f, -0.5f, -0.5f }, { 0, 0,-1 }, { 0, 1 }, {-1, 0, 0 }},
                {{ -0.5f,  0.5f, -0.5f }, { 0, 1, 0 }, { 0, 0 }, { 1, 0, 0 }},
                {{  0.5f,  0.5f, -0.5f }, { 0, 1, 0 }, { 1, 0 }, { 1, 0, 0 }},
                {{  0.5f,  0.5f,  0.5f }, { 0, 1, 0 }, { 1, 1 }, { 1, 0, 0 }},
                {{ -0.5f,  0.5f,  0.5f }, { 0, 1, 0 }, { 0, 1 }, { 1, 0, 0 }},
                {{ -0.5f, -0.5f,  0.5f }, { 0,-1, 0 }, { 0, 0 }, { 1, 0, 0 }},
                {{  0.5f, -0.5f,  0.5f }, { 0,-1, 0 }, { 1, 0 }, { 1, 0, 0 }},
                {{  0.5f, -0.5f, -0.5f }, { 0,-1, 0 }, { 1, 1 }, { 1, 0, 0 }},
                {{ -0.5f, -0.5f, -0.5f }, { 0,-1, 0 }, { 0, 1 }, { 1, 0, 0 }},
                {{  0.5f,  0.5f,  0.5f }, { 1, 0, 0 }, { 0, 0 }, { 0, 0,-1 }},
                {{  0.5f,  0.5f, -0.5f }, { 1, 0, 0 }, { 1, 0 }, { 0, 0,-1 }},
                {{  0.5f, -0.5f, -0.5f }, { 1, 0, 0 }, { 1, 1 }, { 0, 0,-1 }},
                {{  0.5f, -0.5f,  0.5f }, { 1, 0, 0 }, { 0, 1 }, { 0, 0,-1 }},
                {{ -0.5f,  0.5f, -0.5f }, {-1, 0, 0 }, { 0, 0 }, { 0, 0, 1 }},
                {{ -0.5f,  0.5f,  0.5f }, {-1, 0, 0 }, { 1, 0 }, { 0, 0, 1 }},
                {{ -0.5f, -0.5f,  0.5f }, {-1, 0, 0 }, { 1, 1 }, { 0, 0, 1 }},
                {{ -0.5f, -0.5f, -0.5f }, {-1, 0, 0 }, { 0, 1 }, { 0, 0, 1 }}
            };

            // 2. 루프를 돌며 Position과 Material로 분리 + 바운딩 박스 계산
            vector<VertexPosition> positions;
            vector<VertexMaterial> materials;
            vec3 minBound = vec3((numeric_limits<f32>::max)());
            vec3 maxBound = vec3(numeric_limits<f32>::lowest());

            positions.reserve(vertices.size());
            materials.reserve(vertices.size());

            for (const auto& v : vertices)
            {
                positions.push_back({ v.position });

                VertexMaterial mat;
                mat.normal = v.normal;
                mat.texCoord = v.texCoord;
                mat.tangent = v.tangent;
                mat.binormal = glm::cross(v.normal, v.tangent); // 자동 계산
                mat.color = vec4(1.0f);
                materials.push_back(mat);

                minBound = glm::min(minBound, v.position);
                maxBound = glm::max(maxBound, v.position);
            }

            // 3. 인덱스 생성 (기존과 동일)
            vector<uint32> indices;
            for (uint32 i = 0; i < 6; ++i)
            {
                uint32 s = i * 4;
                indices.push_back(s + 0); indices.push_back(s + 1); indices.push_back(s + 2);
                indices.push_back(s + 0); indices.push_back(s + 2); indices.push_back(s + 3);
            }

            // 4. 새로운 Streams 배열 기반 Desc 구성
            MeshCreateDesc meshDesc = {};
            meshDesc.Streams[(uint32)EMeshStream::Position] = { positions.data(), (uint32)positions.size(), sizeof(VertexPosition) };
            meshDesc.Streams[(uint32)EMeshStream::Material] = { materials.data(), (uint32)materials.size(), sizeof(VertexMaterial) };
            meshDesc.IndexData = indices.data();
            meshDesc.IndexCount = static_cast<uint32>(indices.size());
            meshDesc.IndexStride = sizeof(uint32);
            meshDesc.BoundingBoxMin = minBound;
            meshDesc.BoundingBoxMax = maxBound;

            resourceManager.AddResource<Mesh>(L"CubeMesh", Mesh::Create(&meshDesc));
        }
#pragma endregion
#pragma region Sphere
        {
            const uint32 slices = 24;
            const uint32 stacks = 16;
            const float radius = 0.5f;

            vector<VertexPosition> positions;
            vector<VertexMaterial> materials;
            vector<uint32> indices;

            for (uint32 i = 0; i <= stacks; ++i)
            {
                float phi = glm::pi<float>() * static_cast<float>(i) / static_cast<float>(stacks);
                float y = radius * std::cos(phi);
                float rSin = radius * std::sin(phi);

                for (uint32 j = 0; j <= slices; ++j)
                {
                    float theta = 2.0f * glm::pi<float>() * static_cast<float>(j) / static_cast<float>(slices);
                    float x = rSin * std::cos(theta);
                    float z = rSin * std::sin(theta);

                    vec3 pos(x, y, z);
                    vec3 normal = glm::normalize(pos);
                    vec2 uv(static_cast<float>(j) / static_cast<float>(slices), static_cast<float>(i) / static_cast<float>(stacks));
                    vec3 tangent = glm::normalize(vec3(-std::sin(theta), 0.0f, std::cos(theta)));
                    vec3 binormal = glm::cross(normal, tangent);

                    positions.push_back({ pos });
                    VertexMaterial mat{};
                    mat.normal = normal;
                    mat.texCoord = uv;
                    mat.tangent = tangent;
                    mat.binormal = binormal;
                    mat.color = vec4(1.0f);
                    materials.push_back(mat);
                }
            }

            for (uint32 i = 0; i < stacks; ++i)
            {
                for (uint32 j = 0; j < slices; ++j)
                {
                    uint32 first = i * (slices + 1) + j;
                    uint32 second = first + slices + 1;

                    indices.push_back(first);
                    indices.push_back(second);
                    indices.push_back(first + 1);

                    indices.push_back(second);
                    indices.push_back(second + 1);
                    indices.push_back(first + 1);
                }
            }

            MeshCreateDesc sphereDesc = {};
            sphereDesc.Streams[(uint32)EMeshStream::Position] = { positions.data(), static_cast<uint32>(positions.size()), sizeof(VertexPosition) };
            sphereDesc.Streams[(uint32)EMeshStream::Material] = { materials.data(), static_cast<uint32>(materials.size()), sizeof(VertexMaterial) };
            sphereDesc.IndexData = indices.data();
            sphereDesc.IndexCount = static_cast<uint32>(indices.size());
            sphereDesc.IndexStride = sizeof(uint32);
            sphereDesc.BoundingBoxMin = vec3(-radius);
            sphereDesc.BoundingBoxMax = vec3(radius);

            resourceManager.AddResource<Mesh>(L"SphereMesh", Mesh::Create(&sphereDesc));
        }
#pragma endregion

#pragma region Plane
        {
            // 1m x 1m XZ horizontal plane facing +Y
            vector<VertexPosition> positions = {
                { { -0.5f, 0.0f,  0.5f } },
                { {  0.5f, 0.0f,  0.5f } },
                { {  0.5f, 0.0f, -0.5f } },
                { { -0.5f, 0.0f, -0.5f } }
            };
            vector<VertexMaterial> materials = {
                { { 0, 1, 0 }, { 0, 0 }, { 1, 0, 0 }, { 0, 0, 1 }, vec4(1.0f) },
                { { 0, 1, 0 }, { 1, 0 }, { 1, 0, 0 }, { 0, 0, 1 }, vec4(1.0f) },
                { { 0, 1, 0 }, { 1, 1 }, { 1, 0, 0 }, { 0, 0, 1 }, vec4(1.0f) },
                { { 0, 1, 0 }, { 0, 1 }, { 1, 0, 0 }, { 0, 0, 1 }, vec4(1.0f) }
            };
            vector<uint32> indices = { 0, 1, 2, 0, 2, 3 };

            MeshCreateDesc planeDesc = {};
            planeDesc.Streams[(uint32)EMeshStream::Position] = { positions.data(), (uint32)positions.size(), sizeof(VertexPosition) };
            planeDesc.Streams[(uint32)EMeshStream::Material] = { materials.data(), (uint32)materials.size(), sizeof(VertexMaterial) };
            planeDesc.IndexData = indices.data();
            planeDesc.IndexCount = static_cast<uint32>(indices.size());
            planeDesc.IndexStride = sizeof(uint32);
            planeDesc.BoundingBoxMin = vec3(-0.5f, 0.0f, -0.5f);
            planeDesc.BoundingBoxMax = vec3(0.5f, 0.0f, 0.5f);

            resourceManager.AddResource<Mesh>(L"PlaneMesh", Mesh::Create(&planeDesc));
        }
#pragma endregion

#pragma region Cylinder
        {
            // Cylinder: radius 0.5f, height 1.0f (-0.5f to +0.5f on Y)
            const uint32 slices = 24;
            const float radius = 0.5f;
            const float halfHeight = 0.5f;

            vector<VertexPosition> positions;
            vector<VertexMaterial> materials;
            vector<uint32> indices;

            for (uint32 i = 0; i <= slices; ++i)
            {
                float theta = 2.0f * glm::pi<float>() * static_cast<float>(i) / static_cast<float>(slices);
                float x = radius * std::cos(theta);
                float z = radius * std::sin(theta);
                vec3 normal(std::cos(theta), 0.0f, std::sin(theta));
                vec3 tangent(-std::sin(theta), 0.0f, std::cos(theta));
                vec3 binormal(0.0f, 1.0f, 0.0f);
                float u = static_cast<float>(i) / static_cast<float>(slices);

                positions.push_back({ { x, halfHeight, z } });
                materials.push_back({ normal, { u, 0.0f }, tangent, binormal, vec4(1.0f) });

                positions.push_back({ { x, -halfHeight, z } });
                materials.push_back({ normal, { u, 1.0f }, tangent, binormal, vec4(1.0f) });
            }

            for (uint32 i = 0; i < slices; ++i)
            {
                uint32 top1 = i * 2;
                uint32 btm1 = top1 + 1;
                uint32 top2 = top1 + 2;
                uint32 btm2 = top1 + 3;

                indices.push_back(top1);
                indices.push_back(top2);
                indices.push_back(btm1);

                indices.push_back(top2);
                indices.push_back(btm2);
                indices.push_back(btm1);
            }

            // Top cap
            uint32 topCenterIdx = static_cast<uint32>(positions.size());
            positions.push_back({ { 0.0f, halfHeight, 0.0f } });
            materials.push_back({ { 0.0f, 1.0f, 0.0f }, { 0.5f, 0.5f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, vec4(1.0f) });

            for (uint32 i = 0; i <= slices; ++i)
            {
                float theta = 2.0f * glm::pi<float>() * static_cast<float>(i) / static_cast<float>(slices);
                float x = radius * std::cos(theta);
                float z = radius * std::sin(theta);
                positions.push_back({ { x, halfHeight, z } });
                materials.push_back({ { 0.0f, 1.0f, 0.0f }, { 0.5f + 0.5f * std::cos(theta), 0.5f + 0.5f * std::sin(theta) }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, vec4(1.0f) });
            }

            for (uint32 i = 0; i < slices; ++i)
            {
                indices.push_back(topCenterIdx);
                indices.push_back(topCenterIdx + 1 + i);
                indices.push_back(topCenterIdx + 2 + i);
            }

            // Bottom cap
            uint32 btmCenterIdx = static_cast<uint32>(positions.size());
            positions.push_back({ { 0.0f, -halfHeight, 0.0f } });
            materials.push_back({ { 0.0f, -1.0f, 0.0f }, { 0.5f, 0.5f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }, vec4(1.0f) });

            for (uint32 i = 0; i <= slices; ++i)
            {
                float theta = 2.0f * glm::pi<float>() * static_cast<float>(i) / static_cast<float>(slices);
                float x = radius * std::cos(theta);
                float z = radius * std::sin(theta);
                positions.push_back({ { x, -halfHeight, z } });
                materials.push_back({ { 0.0f, -1.0f, 0.0f }, { 0.5f + 0.5f * std::cos(theta), 0.5f - 0.5f * std::sin(theta) }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }, vec4(1.0f) });
            }

            for (uint32 i = 0; i < slices; ++i)
            {
                indices.push_back(btmCenterIdx);
                indices.push_back(btmCenterIdx + 2 + i);
                indices.push_back(btmCenterIdx + 1 + i);
            }

            MeshCreateDesc cylDesc = {};
            cylDesc.Streams[(uint32)EMeshStream::Position] = { positions.data(), static_cast<uint32>(positions.size()), sizeof(VertexPosition) };
            cylDesc.Streams[(uint32)EMeshStream::Material] = { materials.data(), static_cast<uint32>(materials.size()), sizeof(VertexMaterial) };
            cylDesc.IndexData = indices.data();
            cylDesc.IndexCount = static_cast<uint32>(indices.size());
            cylDesc.IndexStride = sizeof(uint32);
            cylDesc.BoundingBoxMin = vec3(-radius, -halfHeight, -radius);
            cylDesc.BoundingBoxMax = vec3(radius, halfHeight, radius);

            resourceManager.AddResource<Mesh>(L"CylinderMesh", Mesh::Create(&cylDesc));
        }
#pragma endregion

#pragma region Capsule
        {
            // Capsule: radius 0.25f, cylinder height 0.5f (-0.25f to +0.25f), total height 1.0f (-0.5f to +0.5f)
            const uint32 slices = 20;
            const uint32 stacks = 8;
            const float radius = 0.25f;
            const float halfHeight = 0.25f;

            vector<VertexPosition> positions;
            vector<VertexMaterial> materials;
            vector<uint32> indices;

            // Top hemisphere: phi from 0 to pi/2
            for (uint32 i = 0; i <= stacks; ++i)
            {
                float phi = (glm::pi<float>() * 0.5f) * static_cast<float>(i) / static_cast<float>(stacks);
                float y = halfHeight + radius * std::cos(phi);
                float rSin = radius * std::sin(phi);

                for (uint32 j = 0; j <= slices; ++j)
                {
                    float theta = 2.0f * glm::pi<float>() * static_cast<float>(j) / static_cast<float>(slices);
                    float x = rSin * std::cos(theta);
                    float z = rSin * std::sin(theta);

                    vec3 pos(x, y, z);
                    vec3 normal = glm::normalize(vec3(x, y - halfHeight, z));
                    vec2 uv(static_cast<float>(j) / static_cast<float>(slices), 0.25f * (static_cast<float>(i) / static_cast<float>(stacks)));
                    vec3 tangent(-std::sin(theta), 0.0f, std::cos(theta));
                    vec3 binormal = glm::cross(normal, tangent);

                    positions.push_back({ pos });
                    materials.push_back({ normal, uv, tangent, binormal, vec4(1.0f) });
                }
            }

            // Bottom hemisphere: phi from pi/2 to pi
            for (uint32 i = 0; i <= stacks; ++i)
            {
                float phi = (glm::pi<float>() * 0.5f) + (glm::pi<float>() * 0.5f) * static_cast<float>(i) / static_cast<float>(stacks);
                float y = -halfHeight + radius * std::cos(phi);
                float rSin = radius * std::sin(phi);

                for (uint32 j = 0; j <= slices; ++j)
                {
                    float theta = 2.0f * glm::pi<float>() * static_cast<float>(j) / static_cast<float>(slices);
                    float x = rSin * std::cos(theta);
                    float z = rSin * std::sin(theta);

                    vec3 pos(x, y, z);
                    vec3 normal = glm::normalize(vec3(x, y + halfHeight, z));
                    vec2 uv(static_cast<float>(j) / static_cast<float>(slices), 0.75f + 0.25f * (static_cast<float>(i) / static_cast<float>(stacks)));
                    vec3 tangent(-std::sin(theta), 0.0f, std::cos(theta));
                    vec3 binormal = glm::cross(normal, tangent);

                    positions.push_back({ pos });
                    materials.push_back({ normal, uv, tangent, binormal, vec4(1.0f) });
                }
            }

            uint32 ringStride = slices + 1;
            uint32 totalStacks = stacks * 2 + 1;
            for (uint32 i = 0; i < totalStacks; ++i)
            {
                for (uint32 j = 0; j < slices; ++j)
                {
                    uint32 first = i * ringStride + j;
                    uint32 second = first + ringStride;

                    indices.push_back(first);
                    indices.push_back(second);
                    indices.push_back(first + 1);

                    indices.push_back(second);
                    indices.push_back(second + 1);
                    indices.push_back(first + 1);
                }
            }

            MeshCreateDesc capDesc = {};
            capDesc.Streams[(uint32)EMeshStream::Position] = { positions.data(), static_cast<uint32>(positions.size()), sizeof(VertexPosition) };
            capDesc.Streams[(uint32)EMeshStream::Material] = { materials.data(), static_cast<uint32>(materials.size()), sizeof(VertexMaterial) };
            capDesc.IndexData = indices.data();
            capDesc.IndexCount = static_cast<uint32>(indices.size());
            capDesc.IndexStride = sizeof(uint32);
            capDesc.BoundingBoxMin = vec3(-radius, -halfHeight - radius, -radius);
            capDesc.BoundingBoxMax = vec3(radius, halfHeight + radius, radius);

            resourceManager.AddResource<Mesh>(L"CapsuleMesh", Mesh::Create(&capDesc));
        }
#pragma endregion
    }
}