#include "Module/ModuleInterface.h"
#include "Logger.h"
#include "SystemManager.h"
#include "System.h"
#include "CoreComponents.h"
#include "GraphExecutor.h"
#include "SceneManager.h"
#include "TransformSystem.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include "GraphFile.h"
#include "GraphReflection.h"
#include "ReflectionRegistry.h"
#include <filesystem>
#include <utility>
#include "Components/Graph/GraphBindingComponent.h"
#include <chrono>
#include <cstdint>
#include <type_traits>
#include <array>

class SpinSystem final : public Engine::ISystem
{
public:
    static SpinSystem* Create() { return new SpinSystem(); }

    void OnUpdate(entt::registry& globalRegistry, const std::vector<Engine::Scene*> activeScenes, f32 dt) override
    {
        // 초당 회전 각도 (도 단위)
        constexpr f32 rotateSpeed = 60.0f;

        // 실제 씬에 배치된 엔티티들을 순회
        for (Engine::Scene* scene : activeScenes)
        {
            if (!scene) continue;
            auto& sceneRegistry = scene->GetRegistry();
            auto view = sceneRegistry.view<Engine::TransformComponent, Engine::NameComponent>();

            for (auto entity : view)
            {
                const auto& nameComp = view.get<Engine::NameComponent>(entity);
                if (nameComp.name.find(L"Sphere") != std::wstring::npos || nameComp.name.find(L"Cube") != std::wstring::npos)
                {
                    auto& transform = view.get<Engine::TransformComponent>(entity);
                    glm::vec3 euler = transform.eulerRotation;
                    euler.y += rotateSpeed * dt;
                    transform.SetEulerRotation(euler);
                }
            }
        }
    }
};
namespace
{
	using namespace Engine;

	class GraphMovementSystem final : public ISystem
	{
	public:
		static GraphMovementSystem* Create()
		{
			try
			{
				auto system = std::make_unique<GraphMovementSystem>();
				system->InitializeRegistry();
				return system.release();
			}
			catch (const std::exception& error)
			{
				BAM_LOG(Error, "GameModule", "GraphMovement", "Initialization failed: {}", error.what());
				return nullptr;
			}
		}

		void OnUpdate(entt::registry&, const std::vector<Scene*> activeScenes, f32 dt) override
		{
			RefreshScene();
			if (!m_Scene || m_Scene->IsPaused() || !std::isfinite(dt) || dt < 0.0f) return;
			if (std::find(activeScenes.begin(), activeScenes.end(), m_Scene.get()) == activeScenes.end()) return;

			auto& world = m_Scene->GetRegistry();
			m_Scene->UpdateHierarchyFlags();

			std::erase_if(m_Instances, [&](const auto& entry) { return !IsTarget(world, entry.first); });
			PollGraphChanges(world);

			for (const auto entity : world.view<TransformComponent, GraphBindingComponent>())
			{
				if (!IsTarget(world, entity)) continue;

				if (const auto* flags = world.try_get<FlagComponent>(entity))
				{
					if (!flags->IsActiveInHierarchy() || HasFlag(flags->effectiveFlags, EEntityFlag::Paused) ||
						HasFlag(flags->effectiveFlags, EEntityFlag::Dead)) continue;
				}

				const auto& binding = world.get<GraphBindingComponent>(entity);
				auto& instance = m_Instances[entity];

				if (instance.GraphFile != binding.graphFile)
				{
					instance = Instance{};
					instance.GraphFile = binding.graphFile;
				}

				if (instance.Failed) continue;

				try
				{
					auto& executor = instance.Executor;
					if (executor.GetState() == EGraphExecutionState::Idle)
						LoadInstanceGraph(instance);

					constexpr std::size_t budget = 1024;
					Require(executor.Begin(instance.UpdateEntry, dt, budget));
					executor.Resume();
					executor.Tick(budget + 1);

					if (executor.GetState() == EGraphExecutionState::Failed)
						Require(executor.GetDiagnostic());

					if (executor.GetState() != EGraphExecutionState::Completed)
						throw std::runtime_error("Movement event did not complete.");

					const auto displacement = ReadDisplacement(executor.GetResult());

					auto candidate = world.get<TransformComponent>(entity);
					const auto position = candidate.position + displacement;
					if (!candidate.SetLocalPosition(position) ||
						!TransformSystem::TryCommitTransform(world, entity, candidate))
						throw std::runtime_error("Movement transform was rejected.");
				}
				catch (const std::exception& error)
				{
					instance.Failed = true;
					BAM_LOG(Error, "GameModule", "GraphMovement",
						"Entity {} stopped, graph '{}': {}",
						entt::to_integral(entity), WStrToStr(instance.GraphFile), error.what());
				}
			}
		}

		void OnLateUpdate(entt::registry&, const std::vector<Scene*>, f32) override
		{
			RefreshScene();
		}

	private:
		struct FileStamp
		{
			std::filesystem::file_time_type WriteTime{};
			std::uintmax_t Size = 0;

			bool operator==(const FileStamp&) const = default;
		};

		struct GraphSource
		{
			Graph Data{ EGraphDomain::Logic };
			GraphNodeId UpdateEntry = 0;
			FileStamp Stamp;
			std::string ReloadError;
		};

		struct Instance
		{
			GraphExecutor Executor;
			std::wstring GraphFile;
			std::filesystem::path SourcePath;
			GraphNodeId UpdateEntry = 0;
			bool Failed = false;
		};

		static void Require(const GraphDiagnostic& diagnostic)
		{
			if (!diagnostic.Succeeded())
				throw std::runtime_error(fmt::format("Node {}, pin {}: {}",
					diagnostic.Node, diagnostic.Pin, diagnostic.Message));
		}

		static bool IsTarget(const entt::registry& world, entt::entity entity)
		{
			if (!world.valid(entity) || !world.all_of<TransformComponent, GraphBindingComponent>(entity))
				return false;

			const auto& binding = world.get<GraphBindingComponent>(entity);
			if (!binding.enabled || binding.graphFile.empty()) return false;

			const auto* hierarchy = world.try_get<HierarchyComponent>(entity);
			return !hierarchy || hierarchy->parent == entt::null;
		}

		void InitializeRegistry()
		{
			if (!ReflectionRegistry::IsCreated())
				throw std::runtime_error("ReflectionRegistry is unavailable.");

			Require(m_Registry.RegisterBuiltins());
			Require(RegisterReflectedGraphNodes(m_Registry, ReflectionRegistry::Get().GetRegistry()));
		}

		static FileStamp ReadFileStamp(const std::filesystem::path& path)
		{
			return { std::filesystem::last_write_time(path), std::filesystem::file_size(path) };
		}

		GraphSource ReadGraphSource(const std::filesystem::path& path)
		{
			const auto stamp = ReadFileStamp(path);
			const auto& reflection = ReflectionRegistry::Get();
			const auto& types = reflection.GetRegistry();
			const auto pathText = WStrToStr(path.wstring());

			GraphSource source;
			const auto loaded = LoadGraphJson(path, source.Data, m_Registry, types, reflection.GetPropertyCodecs());
			if (!loaded.Succeeded())
			{
				throw std::runtime_error(fmt::format(
					"Graph file '{}' failed: stage={}, archive={} field='{}', "
					"node={} pin='{}' message='{}', system={} '{}'",
					pathText, static_cast<int>(loaded.Stage),
					static_cast<int>(loaded.ArchiveError.Code), loaded.ArchiveError.Field,
					loaded.GraphError.Node, loaded.GraphError.Pin, loaded.GraphError.Message,
					loaded.SystemError.value(),
					loaded.SystemError ? loaded.SystemError.message() : std::string{}));
			}

			GraphExecutor probe;
			Require(probe.Load(source.Data, m_Registry, &types));

			std::size_t resultCount = 0;
			for (const auto& node : source.Data.GetNodes())
			{
				if (node.Type == "Event.Update")
				{
					if (source.UpdateEntry != 0)
						throw std::runtime_error("Movement graph requires exactly one Event.Update node.");

					source.UpdateEntry = node.Id;
				}

				if (node.Type == "Logic.FloatResult" || node.Type == "Logic.Float3Result")
				{
					++resultCount;
				}
				else if (node.Type == "Logic.BoolResult" || node.Type == "Logic.IntResult" ||
					node.Type == "Logic.Float2Result" || node.Type == "Logic.Float4Result")
				{
					throw std::runtime_error(fmt::format(
						"Movement result node '{}' is unsupported (node {}).", node.Type, node.Id));
				}
			}

			if (source.UpdateEntry == 0)
				throw std::runtime_error("Movement graph requires exactly one Event.Update node.");

			if (resultCount != 1)
				throw std::runtime_error("Movement graph requires exactly one Float or Float3 result node.");

			if (ReadFileStamp(path) != stamp)
				throw std::runtime_error("Graph file changed while it was being loaded.");

			source.Stamp = stamp;
			return source;
		}

		const GraphSource& GetGraphSource(const std::filesystem::path& path)
		{
			if (const auto found = m_GraphSources.find(path); found != m_GraphSources.end())
				return found->second;

			auto source = ReadGraphSource(path);
			const auto entry = m_GraphSources.emplace(path, std::move(source)).first;

			BAM_LOG(Info, "GameModule", "GraphMovement",
				"Cached source '{}'", WStrToStr(path.wstring()));

			return entry->second;
		}

		void LoadInstanceGraph(Instance& instance)
		{
			const auto path = std::filesystem::canonical(std::filesystem::path(instance.GraphFile));
			const auto& source = GetGraphSource(path);
			const auto& types = ReflectionRegistry::Get().GetRegistry();

			instance.SourcePath = path;
			Require(instance.Executor.Load(source.Data, m_Registry, &types));
			instance.UpdateEntry = source.UpdateEntry;
		}

		void PollGraphChanges(entt::registry& world)
		{
			const auto now = std::chrono::steady_clock::now();
			if (now < m_NextGraphCheck) return;
			m_NextGraphCheck = now + std::chrono::seconds(1);

			const auto& types = ReflectionRegistry::Get().GetRegistry();

			for (auto& [path, source] : m_GraphSources)
			{
				try
				{
					if (ReadFileStamp(path) == source.Stamp)
					{
						source.ReloadError.clear();
						continue;
					}

					auto candidate = ReadGraphSource(path);

					// Prepare replacements without changing any live instance.
					auto prepared = m_Instances;
					for (auto& [entity, instance] : prepared)
					{
						if (instance.SourcePath != path || !IsTarget(world, entity)) continue;

						const auto& binding = world.get<GraphBindingComponent>(entity);
						if (binding.graphFile != instance.GraphFile) continue;

						Instance replacement;
						replacement.GraphFile = instance.GraphFile;
						replacement.SourcePath = path;
						replacement.UpdateEntry = candidate.UpdateEntry;
						Require(replacement.Executor.Load(candidate.Data, m_Registry, &types));

						instance = std::move(replacement);
					}

					// Commit only after every replacement has been prepared.
					static_assert(std::is_nothrow_swappable_v<GraphSource>);
					static_assert(noexcept(m_Instances.swap(prepared)));

					std::swap(source, candidate);
					m_Instances.swap(prepared);
				}
				catch (const std::exception& error)
				{
					const std::string message = error.what();
					if (source.ReloadError != message)
					{
						source.ReloadError = message;
						BAM_LOG(Warn, "GameModule", "GraphMovement",
							"Reload rejected for '{}'; keeping previous graph: {}",
							WStrToStr(path.wstring()), message);
					}
					continue;
				}

				BAM_LOG(Info, "GameModule", "GraphMovement",
					"Reloaded '{}'; bound instance states were reset.",
					WStrToStr(path.wstring()));
			}
		}

		void RefreshScene()
		{
			auto* scene = SceneManager::Get().GetCurrentScene();
			if (m_Scene.get() == scene) return;

			m_Instances.clear();
			m_GraphSources.clear();
			m_NextGraphCheck = {};
			m_Scene.reset();

			if (scene)
			{
				scene->AddRef();
				m_Scene = std::shared_ptr<Scene>(scene, [](Scene* value) { value->Release(); });
			}
		}

		static glm::vec3 ReadDisplacement(const GraphValue& result)
		{
			if (const auto* scalar = std::get_if<float>(&result))
				return glm::vec3(*scalar, 0.0f, 0.0f);

			if (const auto* vector = std::get_if<std::array<float, 3>>(&result))
				return glm::vec3((*vector)[0], (*vector)[1], (*vector)[2]);

			throw std::runtime_error("Movement result must be Float or Float3.");
		}

		GraphRegistry m_Registry;
		std::shared_ptr<Scene> m_Scene;
		std::unordered_map<entt::entity, Instance> m_Instances;
		std::unordered_map<std::filesystem::path, GraphSource> m_GraphSources;
		std::chrono::steady_clock::time_point m_NextGraphCheck{};
	};
}

class GameModule final : public Engine::IModule
{
public:
    void OnLoad() override
    {
        BAM_LOG(Info, "GameModule", "Lifecycle", "[GameModule] Registering SpinSystem...");
        Engine::SystemManager::Get().AddSystem<SpinSystem>();
		if (!Engine::SystemManager::Get().AddSystem<GraphMovementSystem>())
			BAM_LOG(Error, "GameModule", "GraphMovement", "System registration failed.");
    }

    void OnUnload() override
    {
        BAM_LOG(Info, "GameModule", "Lifecycle", "[GameModule] Unregistering SpinSystem...");
		Engine::SystemManager::Get().RemoveSystem<GraphMovementSystem>();
        Engine::SystemManager::Get().RemoveSystem<SpinSystem>();
    }

    void OnPreHotReload() override
    {
        BAM_LOG(Info, "GameModule", "Lifecycle", "[GameModule] PreHotReload triggered.");
    }

    void OnPostHotReload() override
    {
        BAM_LOG(Info, "GameModule", "Lifecycle", "[GameModule] PostHotReload finished.");
    }
};

IMPLEMENT_MODULE(GameModule)