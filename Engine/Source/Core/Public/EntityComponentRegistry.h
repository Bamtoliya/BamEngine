#pragma once

#include "Engine_API.h"

#include <entt/entity/registry.hpp>

#include <span>
#include <string>
#include <type_traits>
#include <typeindex>
#include <utility>
#include <vector>

namespace Engine
{
	struct EntityComponentEntry
	{
		using AddFunc = bool (*)(entt::registry&, entt::entity);

		std::type_index Type{ typeid(void) };
		std::string Name;
		std::string Category;
		bool (*Has)(const entt::registry&, entt::entity) = nullptr;
		AddFunc Add = nullptr;

		[[nodiscard]]
		bool TryAdd(entt::registry& world, entt::entity entity) const
		{
			if (!world.valid(entity) || Has == nullptr || Add == nullptr || Has(world, entity))
				return false;

			if (!Add(world, entity))
				return false;

			return world.valid(entity) && Has(world, entity);
		}
	};

	class EntityComponentRegistry
	{
	public:
		template<typename T>
		[[nodiscard]]
		bool Register(std::string name, std::string category, EntityComponentEntry::AddFunc add = nullptr)
		{
			static_assert(std::is_same_v<T, std::remove_cvref_t<T>>);
			static_assert(std::is_class_v<T> && std::is_default_constructible_v<T>);

			if (name.empty() || category.empty())
				return false;

			const std::type_index type{ typeid(T) };

			for (const auto& entry : m_Entries)
			{
				if (entry.Type == type || (entry.Category == category && entry.Name == name))
					return false;
			}

			EntityComponentEntry entry;
			entry.Type = type;
			entry.Name = std::move(name);
			entry.Category = std::move(category);

			entry.Has = [](const entt::registry& world, entt::entity entity)
				{
					return world.valid(entity) && world.all_of<T>(entity);
				};

			entry.Add = add;

			if (entry.Add == nullptr)
			{
				entry.Add = [](entt::registry& world, entt::entity entity)
					{
						world.emplace<T>(entity);
						return true;
					};
			}

			m_Entries.push_back(std::move(entry));
			return true;
		}

		[[nodiscard]]
		std::span<const EntityComponentEntry> GetEntries() const
		{
			return { m_Entries.data(), m_Entries.size() };
		}

	private:
		std::vector<EntityComponentEntry> m_Entries;
	};

	[[nodiscard]]
	ENGINE_API const EntityComponentRegistry& GetEntityComponentRegistry();
}