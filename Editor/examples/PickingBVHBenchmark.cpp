#include "Collision.h"
#include "Mesh.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <memory>
#include <vector>

int main()
{
    using Clock = std::chrono::steady_clock;
    constexpr Engine::uint32 Grid = 64;
    constexpr int Repeats = 4;

    std::vector<glm::vec3> positions;
    positions.reserve(Grid * Grid * 6);
    for (Engine::uint32 y = 0; y < Grid; ++y)
    {
        for (Engine::uint32 x = 0; x < Grid; ++x)
        {
            const float left = static_cast<float>(x) - Grid * 0.5f;
            const float bottom = static_cast<float>(y) - Grid * 0.5f;
            const glm::vec3 a(left, bottom, 0.0f), b(left + 1.0f, bottom, 0.0f);
            const glm::vec3 c(left + 1.0f, bottom + 1.0f, 0.0f), d(left, bottom + 1.0f, 0.0f);
            positions.insert(positions.end(), { a, b, c, a, c, d });
        }
    }

    Engine::MeshCreateDesc desc{};
    desc.Streams[static_cast<Engine::uint32>(Engine::EMeshStream::Position)] = {
        positions.data(), static_cast<Engine::uint32>(positions.size()), sizeof(glm::vec3)
    };

    const auto destroy = [](Engine::Mesh* value)
        {
            if (value)
            {
                value->Free();
                delete value;
            }
        };
    std::unique_ptr<Engine::Mesh, decltype(destroy)> mesh(Engine::Mesh::Create(&desc), destroy);
    if (!mesh)
    {
        std::cerr << "Mesh creation failed.\n";
        return 1;
    }

    const auto buildStart = Clock::now();
    const auto& cache = mesh->GetRaycastBVH();
    const double buildMs = std::chrono::duration<double, std::milli>(Clock::now() - buildStart).count();
    if (cache.Triangles.size() != positions.size() / 3 || cache.Nodes.empty())
    {
        std::cerr << "BVH construction failed.\n";
        return 1;
    }

    std::vector<Engine::Ray> rays;
    for (Engine::uint32 i = 0; i < 256; ++i)
    {
        float x = static_cast<float>((i * 17) % Grid) + 0.25f - Grid * 0.5f;
        const float y = static_cast<float>(((i / Grid) * 13 + i * 29) % Grid) + 0.375f - Grid * 0.5f;
        if (i % 4 == 0) x += Grid * 2.0f;
        rays.emplace_back(glm::vec3(x, y, -5.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    }

    const glm::mat4 identity(1.0f);
    const Engine::AABB bounds(mesh->GetMin(), mesh->GetMax());

    const auto linear = [&](const Engine::Ray& ray) -> float
        {
            Engine::HitResult hit{};
            if (!Engine::Collision::Raycast(ray, bounds, identity, hit, 20.0f)) return -1.0f;

            bool found = false;
            float closest = 20.0f;
            for (std::size_t i = 0; i < positions.size(); i += 3)
            {
                if (Engine::Collision::Raycast(ray, positions[i], positions[i + 1], positions[i + 2], hit, closest))
                {
                    found = true;
                    closest = hit.distance;
                }
            }
            return found ? closest : -1.0f;
        };

    const auto bvh = [&](const Engine::Ray& ray) -> float
        {
            Engine::HitResult hit{};
            return Engine::Collision::Raycast(ray, *mesh, identity, hit, 20.0f) ? hit.distance : -1.0f;
        };

    for (const auto& ray : rays)
    {
        const float expected = linear(ray);
        const float actual = bvh(ray);
        if (!std::isfinite(expected) || !std::isfinite(actual) || std::abs(expected - actual) > 0.001f)
        {
            std::cerr << "Linear and BVH results differ.\n";
            return 1;
        }
    }

    const auto measure = [&](const auto& query, double& checksum) -> double
        {
            std::array<double, 5> samples{};
            for (auto& sample : samples)
            {
                const auto start = Clock::now();
                for (int repeat = 0; repeat < Repeats; ++repeat)
                    for (const auto& ray : rays) checksum += query(ray);
                sample = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
            }
            std::sort(samples.begin(), samples.end());
            return samples[samples.size() / 2];
        };

    double linearChecksum = 0.0, bvhChecksum = 0.0;
    const double linearMs = measure(linear, linearChecksum);
    const double bvhMs = measure(bvh, bvhChecksum);

    std::cout << std::fixed << std::setprecision(3)
        << "Triangles: " << cache.Triangles.size() << '\n'
        << "Queries per sample: " << rays.size() * Repeats << '\n'
        << "BVH build ms: " << buildMs << '\n'
        << "Linear median ms: " << linearMs << '\n'
        << "BVH median ms: " << bvhMs << '\n'
        << "Checksums: " << linearChecksum << ", " << bvhChecksum << '\n';
    return 0;
}