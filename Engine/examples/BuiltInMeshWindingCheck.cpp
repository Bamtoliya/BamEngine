#include "ResourceManager.h"
#include "BuiltInPrimitiveMeshes.h"
#include "Mesh.h"
#include "MeshWinding.h"
#include "Vertex.h"
#include "TerrainUtility.h"
#include <cstring>
#include <iostream>

namespace
{
	using namespace Engine;

	// LH(+Z 전방)에서 앞면 = 화면상 시계 방향(CW)일 때 성립: dot(cross(v1 - v0, v2 - v0), N) > 0
	struct WindingResult { uint32 Clockwise = 0; uint32 CounterClockwise = 0; uint32 Degenerate = 0; };

	template<typename T> T ReadAt(const vector<uint8>& raw, size_t offset) { T value{}; std::memcpy(&value, raw.data() + offset, sizeof(T)); return value; }

	bool Inspect(const Mesh& mesh, WindingResult& out)
	{
		const auto& posRaw = mesh.GetRawStreamData(EMeshStream::Position);
		const auto& matRaw = mesh.GetRawStreamData(EMeshStream::Material);
		const auto& idxRaw = mesh.GetRawIndexData();
		const uint32 posStride = mesh.GetStreamStride(EMeshStream::Position);
		const uint32 matStride = mesh.GetStreamStride(EMeshStream::Material);
		const uint32 idxStride = mesh.GetIndexStride();
		if (posRaw.empty() || matRaw.empty() || idxRaw.empty() || matStride != sizeof(VertexMaterial) || mesh.GetIndexCount() % 3 != 0)
			return false;

		const auto index = [&](uint32 i) -> uint32 { return idxStride == sizeof(uint16) ? ReadAt<uint16>(idxRaw, i * sizeof(uint16)) : ReadAt<uint32>(idxRaw, i * sizeof(uint32)); };
		const auto position = [&](uint32 v) { return ReadAt<vec3>(posRaw, static_cast<size_t>(v) * posStride); };
		const auto normal = [&](uint32 v) { return ReadAt<VertexMaterial>(matRaw, static_cast<size_t>(v) * matStride).normal; };

		for (uint32 i = 0; i < mesh.GetIndexCount(); i += 3)
		{
			const uint32 a = index(i), b = index(i + 1), c = index(i + 2);
			const vec3 faceCross = glm::cross(position(b) - position(a), position(c) - position(a));
			if (glm::length(faceCross) < 1e-6f)
			{
				++out.Degenerate;
				continue;
			}

			if (glm::dot(faceCross, normal(a) + normal(b) + normal(c)) > 0.0f)
				++out.Clockwise;
			else
				++out.CounterClockwise;
		}
		return true;
	}
}

int main()
{
	if (!ResourceManager::Create())
	{
		std::cerr << "[FAIL] ResourceManager initialization failed.\n";
		return 1;
	}
	struct ResourceScope { ~ResourceScope() { ResourceManager::Destroy(); } } resourceScope;

	auto& rm = ResourceManager::Get();
	RegisterBuiltInPrimitiveMeshes(rm);
	rm.AddResource<Mesh>(L"WindingCheck.TerrainGrid", TerrainUtility::CreateGridMesh(4.0f, 4.0f, 4));

	struct Target { const wchar_t* Key; const char* Name; };
	const Target targets[] = { { L"QuadMesh", "Quad" }, { L"CubeMesh", "Cube" }, { L"SphereMesh", "Sphere" }, { L"PlaneMesh", "Plane" },
		{ L"CylinderMesh", "Cylinder" }, { L"CapsuleMesh", "Capsule" }, { L"WindingCheck.TerrainGrid", "TerrainGrid" } };

	int failures = 0;
	for (const auto& target : targets)
	{
		Mesh* mesh = rm.GetResourceHandle<Mesh>(target.Key).Get();
		WindingResult result;
		if (!mesh || !Inspect(*mesh, result))
		{
			std::cerr << "[FAIL] " << target.Name << ": mesh or CPU raw data unavailable\n";
			++failures;
			continue;
		}

		const bool ok = result.CounterClockwise == 0 && result.Clockwise > 0;
		(ok ? std::cout : std::cerr) << (ok ? "[ OK ] " : "[FAIL] ") << target.Name << " CW=" << result.Clockwise << " CCW=" << result.CounterClockwise << " Degenerate=" << result.Degenerate << '\n';
		if (!ok) ++failures;
	}

	// 축을 홀수 번 뒤집으면 앞면 방향을 바꾸고, 짝수 번이면 그대로 둔다.
	const auto expect = [&failures](bool condition, const char* message)
		{
			if (condition) return;
			std::cerr << "[FAIL] " << message << '\n';
			++failures;
		};
	const auto scale = [](float x, float y, float z) { return glm::scale(mat4(1.0f), vec3(x, y, z)); };
	const mat4 moved = glm::translate(mat4(1.0f), vec3(3.0f, -2.0f, 5.0f)) * glm::rotate(mat4(1.0f), 1.1f, glm::normalize(vec3(1.0f, 2.0f, 3.0f)));
	expect(!IsMirroredTransform(mat4(1.0f)) && !IsMirroredTransform(moved) && !IsMirroredTransform(moved * scale(2.0f, 0.5f, 3.0f)), "plain transforms must keep the winding");
	expect(IsMirroredTransform(scale(-1.0f, 1.0f, 1.0f)) && IsMirroredTransform(moved * scale(1.0f, -2.0f, 1.0f)) && IsMirroredTransform(scale(-1.0f, -1.0f, -1.0f)), "an odd number of flips must reverse the winding");
	expect(!IsMirroredTransform(moved * scale(-1.0f, -1.0f, 1.0f)) && !IsMirroredTransform(scale(0.0f, 1.0f, 1.0f)), "an even number of flips or a flat transform must keep the winding");
	expect(ResolveFrontFace(EFrontFace::Clockwise, false) == EFrontFace::Clockwise && ResolveFrontFace(EFrontFace::Clockwise, true) == EFrontFace::CounterClockwise &&
		ResolveFrontFace(EFrontFace::CounterClockwise, true) == EFrontFace::Clockwise, "mirrored draws must use the opposite front face");
	return failures == 0 ? 0 : 1;
}