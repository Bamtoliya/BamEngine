#pragma once

#include "GraphValue.h"
#include <reflection_archive/PropertyArchiveCodecs.h>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace archive_graph
{
	namespace detail
	{
		template<typename T>
		constexpr const char* ValueTag()
		{
			if constexpr (std::is_same_v<T, std::monostate>) return "None";
			else if constexpr (std::is_same_v<T, bool>) return "Bool";
			else if constexpr (std::is_same_v<T, std::int64_t>) return "Int";
			else if constexpr (std::is_same_v<T, float>) return "Float";
			else if constexpr (std::is_same_v<T, std::array<float, 2>>) return "Float2";
			else if constexpr (std::is_same_v<T, std::array<float, 3>>) return "Float3";
			else
			{
				static_assert(std::is_same_v<T, std::array<float, 4>>);
				return "Float4";
			}
		}

		template<typename T>
		archive::ArchiveResult ReadAlternative(const archive::IArchive& object, Engine::GraphValue& candidate)
		{
			T decoded{};
			archive::ArchiveResult result;

			if constexpr (std::is_same_v<T, std::monostate>)
			{
				bool hasValue = false;
				result = object.HasField("Value", hasValue);
				if (result && hasValue)
					return { archive::ArchiveErrorCode::InvalidValue, "Value" };
			}
			else if constexpr (archive::ArchiveScalar<T>)
			{
				result = object.Read("Value", decoded);
			}
			else
			{
				result = object.ReadArray("Value", decoded);
			}

			if (!result) return result;

			candidate = decoded;
			if (!Engine::IsFiniteGraphValue(candidate))
				return { archive::ArchiveErrorCode::InvalidValue, "Value" };

			return {};
		}
	}

	[[nodiscard]] inline archive::ArchiveResult WriteValue(
		archive::IArchive& ar, std::string_view key, const Engine::GraphValue& value)
	{
		if (!Engine::IsFiniteGraphValue(value))
			return { archive::ArchiveErrorCode::InvalidValue, std::string(key) };

		return ar.WriteObject(key, value,
			[](archive::IArchive& object, const Engine::GraphValue& source) -> archive::ArchiveResult
			{
				return std::visit([&](const auto& item) -> archive::ArchiveResult
					{
						using T = std::decay_t<decltype(item)>;
						const auto result = object.Write("Type", std::string(detail::ValueTag<T>()));
						if (!result) return result;

						if constexpr (std::is_same_v<T, std::monostate>)
							return {};
						else if constexpr (archive::ArchiveScalar<T>)
							return object.Write("Value", item);
						else
							return object.WriteArray("Value", item);
					}, source);
			});
	}

	[[nodiscard]] inline archive::ArchiveResult ReadValue(
		const archive::IArchive& ar, std::string_view key, Engine::GraphValue& value)
	{
		return ar.ReadObject(key, value,
			[](archive::IArchive& object, Engine::GraphValue& candidate) -> archive::ArchiveResult
			{
				std::string type;
				const auto result = object.Read("Type", type);
				if (!result) return result;

				if (type == "None") return detail::ReadAlternative<std::monostate>(object, candidate);
				if (type == "Bool") return detail::ReadAlternative<bool>(object, candidate);
				if (type == "Int") return detail::ReadAlternative<std::int64_t>(object, candidate);
				if (type == "Float") return detail::ReadAlternative<float>(object, candidate);
				if (type == "Float2") return detail::ReadAlternative<std::array<float, 2>>(object, candidate);
				if (type == "Float3") return detail::ReadAlternative<std::array<float, 3>>(object, candidate);
				if (type == "Float4") return detail::ReadAlternative<std::array<float, 4>>(object, candidate);

				return { archive::ArchiveErrorCode::InvalidValue, "Type" };
			});
	}

	[[nodiscard]] inline bool RegisterValueCodec(archive_reflection::PropertyCodecs& codecs)
	{
		return codecs.Register<Engine::GraphValue>(WriteValue, ReadValue);
	}
}