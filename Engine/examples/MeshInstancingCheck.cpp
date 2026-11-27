#include <cassert>
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstddef>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Types.h"
#include "InstanceData.h"
#include "RenderCommands.h"
#include "RenderPass.h"

using namespace Engine;

// ==============================================================================
// [Test 1] Memory Layout & Struct Alignment Verification
// ==============================================================================
void CheckMemoryLayoutAndAlignment()
{
	std::cout << "[Test 1] Memory Layout & Struct Alignment Verification" << std::endl;

	// StructuredBuffer<InstanceData> 및 정적 메시 인스턴스 데이터 크기 단언
	assert(sizeof(InstanceData) == 96);
	assert(offsetof(InstanceData, worldMatrix) == 0);
	assert(offsetof(InstanceData, pickingData) == 64);
	assert(offsetof(InstanceData, customData) == 80);

	// SpriteInstanceData 크기 단언
	assert(sizeof(SpriteInstanceData) == 112);
	assert(offsetof(SpriteInstanceData, worldMatrix) == 0);
	assert(offsetof(SpriteInstanceData, color) == 64);
	assert(offsetof(SpriteInstanceData, uvTransform) == 80);
	assert(offsetof(SpriteInstanceData, pickingData) == 96);

	// 16바이트 GPU 정렬 일치 검증
	assert(sizeof(InstanceData) % 16 == 0);
	assert(sizeof(SpriteInstanceData) % 16 == 0);

	std::cout << "  -> InstanceData (96B) and SpriteInstanceData (112B) layout assertions passed." << std::endl;
}

// ==============================================================================
// [Test 2] Non-Uniform / Negative Scale Normal Transformation Math Verification
// ==============================================================================
namespace
{
	// gbufferVS.hlsl의 여인수 기저 외적 계산 공식 (GPU 셰이더 시뮬레이션)
	// HLSL의 world[0][0], world[1][0], world[2][0] (열 0의 xyz 성분)는 GLM 열벡터 world[0].xyz와 일치함
	vec3 ComputeShaderNormal(const mat4& world, const vec3& inputNormal)
	{
		vec3 axisX = vec3(world[0]);
		vec3 axisY = vec3(world[1]);
		vec3 axisZ = vec3(world[2]);

		vec3 normalWorld = inputNormal.x * glm::cross(axisY, axisZ) +
						   inputNormal.y * glm::cross(axisZ, axisX) +
						   inputNormal.z * glm::cross(axisX, axisY);

		float determinantSign = glm::dot(axisX, glm::cross(axisY, axisZ)) < 0.0f ? -1.0f : 1.0f;
		return glm::normalize(normalWorld * determinantSign);
	}

	// 표준 수학적 기준: M^-T * n (역행렬의 전치에 의한 법선 변환)
	vec3 ComputeMathematicalNormal(const mat4& world, const vec3& inputNormal)
	{
		mat3 normalMatrix = glm::transpose(glm::inverse(mat3(world)));
		return glm::normalize(normalMatrix * inputNormal);
	}
}

void CheckNormalTransformationMath()
{
	std::cout << "[Test 2] Non-Uniform & Negative Scale Normal Transformation Math Verification" << std::endl;

	std::vector<mat4> testMatrices;

	// Case A: 균일 스케일 + 회전
	mat4 matA = glm::rotate(mat4(1.0f), glm::radians(45.0f), vec3(0.0f, 1.0f, 0.0f));
	matA = glm::scale(matA, vec3(2.0f, 2.0f, 2.0f));
	testMatrices.push_back(matA);

	// Case B: 비균일 스케일 (X=1, Y=5, Z=0.2) + 다축 복합 회전
	mat4 matB = glm::rotate(mat4(1.0f), glm::radians(30.0f), vec3(1.0f, 0.0f, 0.0f));
	matB = glm::rotate(matB, glm::radians(60.0f), vec3(0.0f, 1.0f, 0.0f));
	matB = glm::scale(matB, vec3(1.0f, 5.0f, 0.2f));
	testMatrices.push_back(matB);

	// Case C: 단일 축 음수 스케일 (거울 반전: X=-1, Y=2, Z=1)
	mat4 matC = glm::rotate(mat4(1.0f), glm::radians(25.0f), vec3(0.0f, 0.0f, 1.0f));
	matC = glm::scale(matC, vec3(-1.0f, 2.0f, 1.0f));
	testMatrices.push_back(matC);

	// Case D: 3축 모두 음수 스케일 (X=-2, Y=-3, Z=-1) + 회전
	mat4 matD = glm::rotate(mat4(1.0f), glm::radians(75.0f), vec3(0.577f, 0.577f, 0.577f));
	matD = glm::scale(matD, vec3(-2.0f, -3.0f, -1.0f));
	testMatrices.push_back(matD);

	const std::vector<vec3> testNormals = {
		vec3(1.0f, 0.0f, 0.0f),
		vec3(0.0f, 1.0f, 0.0f),
		vec3(0.0f, 0.0f, 1.0f),
		glm::normalize(vec3(1.0f, 1.0f, 1.0f)),
		glm::normalize(vec3(-0.3f, 0.8f, -0.5f))
	};

	for (size_t m = 0; m < testMatrices.size(); ++m)
	{
		for (size_t n = 0; n < testNormals.size(); ++n)
		{
			vec3 shaderResult = ComputeShaderNormal(testMatrices[m], testNormals[n]);
			vec3 mathResult = ComputeMathematicalNormal(testMatrices[m], testNormals[n]);

			float alignment = glm::dot(shaderResult, mathResult);
			assert(alignment > 0.9999f && "Shader normal transformation deviates from mathematical inverse-transpose!");
		}
	}

	std::cout << "  -> All 20 normal test cases matched M^-T with dot product > 0.9999." << std::endl;
}

// ==============================================================================
// [Test 3] Geometry vs Shadow Pass Batching Simulation
// ==============================================================================
namespace
{
	struct MockMaterial
	{
		std::string name;
		ECullMode cullMode = ECullMode::Back;
		EFrontFace frontFace = EFrontFace::CounterClockwise;
	};

	struct MockMesh
	{
		std::string name;
	};

	struct MockDrawCommand
	{
		MockMesh* mesh = nullptr;
		MockMaterial* material = nullptr;
		mat4 worldMatrix{ 1.0f };
		uint32 pickingId = 0;
	};

	struct BatchSummary
	{
		uint32 drawCount = 0;
		std::vector<uint32> batchSizes;
	};

	BatchSummary SimulateBatching(std::vector<MockDrawCommand> commands, bool isShadowPass)
	{
		// 1. 패스별 정렬
		if (isShadowPass)
		{
			std::sort(commands.begin(), commands.end(), [](const MockDrawCommand& a, const MockDrawCommand& b)
				{
					if (a.mesh != b.mesh)
						return a.mesh < b.mesh;
					if (a.material && b.material)
					{
						if (a.material->cullMode != b.material->cullMode)
							return a.material->cullMode < b.material->cullMode;
						if (a.material->frontFace != b.material->frontFace)
							return a.material->frontFace < b.material->frontFace;
					}
					return a.material < b.material;
				});
		}
		else
		{
			std::sort(commands.begin(), commands.end(), [](const MockDrawCommand& a, const MockDrawCommand& b)
				{
					if (a.material != b.material)
						return a.material < b.material;
					return a.mesh < b.mesh;
				});
		}

		// 2. Renderer::RenderMeshes 배칭 시뮬레이션
		BatchSummary summary;
		MockMesh* batchMesh = nullptr;
		MockMaterial* batchMaterial = nullptr;
		uint32 currentBatchCount = 0;

		auto flushBatch = [&]()
		{
			if (currentBatchCount > 0)
			{
				summary.drawCount++;
				summary.batchSizes.push_back(currentBatchCount);
				currentBatchCount = 0;
			}
		};

		for (const auto& cmd : commands)
		{
			bool needsFlush = false;

			if (batchMesh)
			{
				if (cmd.mesh != batchMesh)
				{
					needsFlush = true;
				}
				else if (!isShadowPass)
				{
					if (cmd.material != batchMaterial)
						needsFlush = true;
				}
				else
				{
					if (batchMaterial && cmd.material)
					{
						if (cmd.material->cullMode != batchMaterial->cullMode ||
							cmd.material->frontFace != batchMaterial->frontFace)
						{
							needsFlush = true;
						}
					}
				}

				if (!needsFlush && currentBatchCount >= 1024)
				{
					needsFlush = true;
				}
			}

			if (needsFlush)
			{
				flushBatch();
				batchMaterial = cmd.material;
				batchMesh = cmd.mesh;
			}
			else if (!batchMesh)
			{
				batchMaterial = cmd.material;
				batchMesh = cmd.mesh;
			}

			currentBatchCount++;
		}

		flushBatch();
		return summary;
	}
}

void CheckBatchingPipeline()
{
	std::cout << "[Test 3] Geometry vs Shadow Pass Batching Simulation" << std::endl;

	MockMesh meshRock{ "Rock" };
	MockMesh meshTree{ "Tree" };

	MockMaterial matRed{ "Red", ECullMode::Back, EFrontFace::CounterClockwise };
	MockMaterial matGreen{ "Green", ECullMode::Back, EFrontFace::CounterClockwise };
	MockMaterial matBlue{ "Blue", ECullMode::Back, EFrontFace::CounterClockwise };
	MockMaterial matTwoSided{ "TwoSided", ECullMode::None, EFrontFace::CounterClockwise };

	std::vector<MockDrawCommand> sceneCommands;

	// Rock: Red 200개, Green 300개, Blue 100개, TwoSided 50개 (총 650개)
	for (int i = 0; i < 200; ++i) sceneCommands.push_back({ &meshRock, &matRed });
	for (int i = 0; i < 300; ++i) sceneCommands.push_back({ &meshRock, &matGreen });
	for (int i = 0; i < 100; ++i) sceneCommands.push_back({ &meshRock, &matBlue });
	for (int i = 0; i < 50; ++i) sceneCommands.push_back({ &meshRock, &matTwoSided });

	// Tree: Green 400개
	for (int i = 0; i < 400; ++i) sceneCommands.push_back({ &meshTree, &matGreen });

	// 1) Geometry Pass 배칭
	BatchSummary geomSummary = SimulateBatching(sceneCommands, false);
	// 지오메트리 패스는 머티리얼이 다르면 배치가 분리되므로:
	// Rock(Red:200), Rock(Green:300), Rock(Blue:100), Rock(TwoSided:50), Tree(Green:400) -> 총 5 드로우콜
	assert(geomSummary.drawCount == 5);

	// 2) Shadow Pass 배칭
	BatchSummary shadowSummary = SimulateBatching(sceneCommands, true);
	// 그림자 패스는 Rock 중 CullMode::Back인 Red(200)+Green(300)+Blue(100) = 600개가 단 1개의 배치로 병합됨!
	// Rock(CullMode::None, 50개)는 별도 배치 1개
	// Tree(Green, 400개)는 별도 배치 1개
	// 총 3 드로우콜로 압축!
	assert(shadowSummary.drawCount == 3);

	// 포인터 주소 순서에 영향을 받지 않도록 크기 집합 정렬 후 검증
	std::vector<uint32> sortedSizes = shadowSummary.batchSizes;
	std::sort(sortedSizes.begin(), sortedSizes.end());
	assert(sortedSizes[0] == 50);  // TwoSided (50개)
	assert(sortedSizes[1] == 400); // Tree (400개)
	assert(sortedSizes[2] == 600); // 3개 머티리얼(Red 200 + Green 300 + Blue 100 = 600개)이 단일 배치로 병합됨!

	// 3) 1024 한도 초과 시 분할 플러시 검증 (2500개 단일 메시/머티리얼)
	std::vector<MockDrawCommand> massiveCommands(2500, { &meshRock, &matRed });
	BatchSummary massiveSummary = SimulateBatching(massiveCommands, true);
	assert(massiveSummary.drawCount == 3);
	assert(massiveSummary.batchSizes[0] == 1024);
	assert(massiveSummary.batchSizes[1] == 1024);
	assert(massiveSummary.batchSizes[2] == 452);

	std::cout << "  -> Geometry Pass draw calls: " << geomSummary.drawCount << " -> Shadow Pass draw calls: "
			  << shadowSummary.drawCount << " (600 instances merged into 1 draw call)." << std::endl;
	std::cout << "  -> 2500 instances safely partitioned into batches of "
			  << massiveSummary.batchSizes[0] << ", " << massiveSummary.batchSizes[1]
			  << ", and " << massiveSummary.batchSizes[2] << "." << std::endl;
}

// ==============================================================================
// [Test 4] Multi-Pass Independent LOD & Hysteresis Margin Oscillation Prevention
// ==============================================================================
void CheckLODEvaluationAndHysteresis()
{
	std::cout << "[Test 4] Multi-Pass Independent LOD & Hysteresis Margin Verification" << std::endl;

	const float worldRadius = 2.0f;
	const float projY = 1.0f;
	const float lod1ScreenCoverage = 0.5f;
	const float lod2ScreenCoverage = 0.2f;
	const float hysteresisMargin = 0.05f;

	auto evaluateLOD = [&](float distance, int32 currentLOD) -> int32
	{
		const float coverage = glm::clamp((worldRadius * projY) / distance, 0.0f, 1.0f);
		float lod2Thresh = lod2ScreenCoverage;
		if (currentLOD == 2) lod2Thresh += hysteresisMargin;

		float lod1Thresh = lod1ScreenCoverage;
		if (currentLOD == 1) lod1Thresh += hysteresisMargin;

		if (coverage <= lod2Thresh) return 2;
		if (coverage <= lod1Thresh) return 1;
		return 0;
	};

	// 1) 원거리 접근 테스트:
	// 거리 2.5 -> coverage = 2.0 / 2.5 = 0.8 -> LOD 0
	// 거리 5.0 -> coverage = 2.0 / 5.0 = 0.4 -> LOD 1
	// 거리 12.0 -> coverage = 2.0 / 12.0 = 0.166 -> LOD 2
	assert(evaluateLOD(2.5f, 0) == 0);
	assert(evaluateLOD(5.0f, 0) == 1);
	assert(evaluateLOD(12.0f, 0) == 2);

	// 2) 히스테리시스 전환 안정성 (Anti-Oscillation) 검증:
	// 현재 LOD 1 상태에서 Coverage가 경계값(0.50) 부근에서 미세 진동할 때:
	// 거리 4.08 -> coverage = 2.0 / 4.08 = 0.4902 (< 0.50)
	// 거리 3.92 -> coverage = 2.0 / 3.92 = 0.5102 (> 0.50)
	// 마진(0.05)이 없을 경우 0.49와 0.51 사이에서 LOD 0과 1이 매 프레임 깜빡임.
	// 마진이 적용된 LOD 1 상태에서는:
	// lod1Thresh는 (coverage > lod1Thresh일 때) 0.50 + 0.05 = 0.55가 됨!
	// 따라서 coverage 0.5102는 0.55보다 작으므로 LOD 1 유지! (깜빡임 완전 억제)
	assert(evaluateLOD(3.92f, 1) == 1);
	assert(evaluateLOD(4.08f, 1) == 1);

	// 확실히 가까워져 coverage가 0.55를 초과해야만 비로소 LOD 0으로 복귀:
	// 거리 3.5 -> coverage = 2.0 / 3.5 = 0.5714 (> 0.55) -> LOD 0 복귀!
	assert(evaluateLOD(3.5f, 1) == 0);

	std::cout << "  -> LOD distance coverage and hysteresis margin anti-thrashing verified successfully." << std::endl;
}

int main()
{
	std::cout << "=== Running MeshInstancingCheck ===" << std::endl;

	CheckMemoryLayoutAndAlignment();
	CheckNormalTransformationMath();
	CheckBatchingPipeline();
	CheckLODEvaluationAndHysteresis();

	std::cout << "=== MeshInstancingCheck PASSED ===" << std::endl;
	return 0;
}
