#pragma once

#include "reflection/ReflectionCore_API.h"
#include "reflection/core/EnumInfo.h"
#include "reflection/core/QualifiedName.h"
#include "reflection/core/TypeInfo.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace reflection
{
    class REFLECTION_CORE_API Registry
    {
    public:
        static Registry& Get();

        void RegisterType(const TypeInfo& type_info);
        void RegisterEnum(const EnumInfo& enum_info);
        void RegisterFunction(const FunctionInfo& function_info);

        const TypeInfo* GetType(std::uint64_t id) const;
        const TypeInfo* GetTypeByQualifiedName(std::string_view qualified_name) const;
        const std::vector<const TypeInfo*>& GetTypesByShortName(std::string_view short_name) const;
        const TypeInfo* ResolveTypeName(std::string_view type_name) const;
        bool HasType(std::string_view qualified_name) const;

        const EnumInfo* GetEnum(std::uint64_t id) const;
        const EnumInfo* GetEnumByQualifiedName(std::string_view qualified_name) const;

        const FunctionInfo* GetFunction(std::uint64_t id) const;
        const FunctionInfo* GetFunctionByQualifiedName(std::string_view owner_qualified_name, std::string_view signature) const;

        void* CreateInstanceByQualifiedName(std::string_view qualified_name) const;
        void DestroyInstanceByQualifiedName(std::string_view qualified_name, void* instance) const;

    private:
        Registry() = default;

    private:
        std::unordered_map<std::uint64_t, const TypeInfo*> m_types_by_id;
        std::unordered_map<std::string, const TypeInfo*> m_types_by_qualified_name;
        std::unordered_map<std::string, std::vector<const TypeInfo*>> m_types_by_short_name;

        std::unordered_map<std::uint64_t, const EnumInfo*> m_enums_by_id;
        std::unordered_map<std::string, const EnumInfo*> m_enums_by_qualified_name;

        std::unordered_map<std::uint64_t, const FunctionInfo*> m_functions_by_id;
        std::unordered_map<std::string, const FunctionInfo*> m_functions_by_qualified_name;
    };
}