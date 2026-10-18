#pragma once

#include "Base.h"
#include "Reflection/ReflectionMacro.h"
#include "Entity.h"
#include "TypeIdentityInterface.h"
#include "Reflection/ReflectionRegistry.h"
#include <utility>
#include <archive/ArchiveResult.h>

BEGIN(archive)
class IArchive;
END

BEGIN(Engine)

enum class EntityReparentResult { Success, Unchanged, InvalidEntity, InvalidHierarchy, Cycle, InvalidTransform, DepthLimit };

class EventSystem;
class UICanvas;

struct SceneCreateDesc
{
	wstring name = L"Scene";
};

enum class ESceneFlags : uint8
{
	None = 0,
	Paused = 1 << 0,
	Active = 1 << 1,
};

ENABLE_BITMASK_OPERATORS(ESceneFlags)

CLASS()
class ENGINE_API Scene : public Base, public ITypeIdentity
{
	REFLECT_BASE()
#pragma region Constructor&Destructor
protected:
	using DESC = SceneCreateDesc;
	Scene() {}
	public: virtual ~Scene() {}
	virtual EResult Initialize(void* arg = nullptr);
public:
	static Scene* Create(void* arg = nullptr);
	virtual void Free() override;
#pragma endregion

#pragma region Loop
public:
	virtual void FixedUpdate(f32 dt);
	virtual void Update(f32 dt);
	virtual void LateUpdate(f32 dt);
#pragma endregion

#pragma region Scene Management
public:
	void SetName(const wstring& name) { m_Name = name; }
	const wstring& GetName() const { return m_Name; }
#pragma endregion

#pragma region Entity Management
public:
	entt::registry& GetRegistry() { return m_LocalRegistry; }
	const entt::registry& GetRegistry() const { return m_LocalRegistry; }
	Entity& CreateEntity();
	[[nodiscard]] bool DestroyEntity(Entity& entity);
	[[nodiscard]] EntityReparentResult ReparentEntity(Entity& child, Entity* parent = nullptr);
	Entity* FindEntity(entt::entity handle);
	Entity* FindEntityById(uint64 id);
	[[nodiscard]] bool MoveSiblingOrder(entt::entity entity, int direction); // -1: 위로, +1: 아래로
	const Entity* FindEntityById(uint64 id) const;
	template<typename T, typename... Args>
	T& AddComponent(Entity& entity, Args&&... args)
	{
		return m_LocalRegistry.emplace<T>(entity.GetEntityHandle(), std::forward<Args>(args)...);
	}
	template<typename T>
	T& GetComponent(Entity& entity)
	{
		return m_LocalRegistry.get<T>(entity.GetEntityHandle());
	}
	template<typename T>
	bool HasComponent(Entity& entity)
	{
		return m_LocalRegistry.all_of<T>(entity.GetEntityHandle());
	}
	template<typename T>
	void RemoveComponent(Entity& entity)
	{
		m_LocalRegistry.remove<T>(entity.GetEntityHandle());
	}
	void ClearEntities();
	[[nodiscard]] ComponentVisitResult ForEachReflectedComponent(Entity& entity, const ComponentVisitor& visitor)
	{
		if (entity.GetScene() != this || !m_LocalRegistry.valid(entity.GetEntityHandle()))
			return ComponentVisitResult::InvalidEntity;
		if (!ReflectionRegistry::IsCreated())
			return ComponentVisitResult::NotInitialized;
		return ReflectionRegistry::Get().ForEachComponent(m_LocalRegistry, entity.GetEntityHandle(), visitor);
	}
	[[nodiscard]] ComponentVisitResult ForEachReflectedComponent(const Entity& entity,
		const ComponentVisitor& visitor) const
	{
		if (entity.GetScene() != this || !m_LocalRegistry.valid(entity.GetEntityHandle()))
			return ComponentVisitResult::InvalidEntity;
		if (!ReflectionRegistry::IsCreated())
			return ComponentVisitResult::NotInitialized;
		return ReflectionRegistry::Get().ForEachComponent(m_LocalRegistry, entity.GetEntityHandle(), visitor);
	}
#pragma endregion

#pragma region Flag Mangement
public:
	bool IsActive() const { return HasFlag(m_Flags, ESceneFlags::Active); }
	void SetActive(bool active);

	bool IsPaused() const { return HasFlag(m_Flags, ESceneFlags::Paused); }
	void SetPaused(bool paused);

	void UpdateHierarchyFlags();
#pragma endregion

#pragma region System Management
public:
	class EventSystem* GetEventSystem() const { return m_EventSystem; }
	void SetEventSystem(class EventSystem* sys) { m_EventSystem = sys; }
	class UICanvas* GetRootCanvas() const { return m_RootCanvas; }
	void SetRootCanvas(class UICanvas* canvas) { m_RootCanvas = canvas; }
#pragma endregion

#pragma region Save&Load
public:
	[[nodiscard]]
	archive::ArchiveResult WriteArchive(archive::IArchive& ar) const;
	[[nodiscard]] static archive::ArchiveResult CreateFromArchive(const archive::IArchive& ar, Scene*& scene);
	[[nodiscard]] archive::ArchiveResult WritePrefabArchive(archive::IArchive& ar, const Entity& rootEntity) const;
	[[nodiscard]] archive::ArchiveResult InstantiatePrefabArchive(const archive::IArchive& ar, Entity*& outRootEntity, Entity* parent = nullptr);
#pragma endregion

#pragma region Variable
protected:
	entt::registry m_LocalRegistry;

	unordered_map<entt::entity, Entity*> m_EntityMap;
	uint64 m_NextEntityId = 1;

	PROPERTY()
	ESceneFlags m_Flags = ESceneFlags::Active;

	PROPERTY()
	wstring m_Name = { L"Scene" };

	class EventSystem* m_EventSystem = { nullptr };
	class UICanvas* m_RootCanvas = { nullptr };
#pragma endregion
};

#
END
