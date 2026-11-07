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
    }
}