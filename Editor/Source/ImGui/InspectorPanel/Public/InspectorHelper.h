#pragma once

#include "Editor_Includes.h"
#include "ReflectionMetadataKeys.h"
#include "SerializationHelper.h"

#include <reflection/runtime/Registry.h>

#include <array>
#include <cctype>
#include <cfloat>
#include <cstdlib>
#include <optional>
#include <vector>
#include <limits>

BEGIN(Editor)

using Engine::MetaNameHash;
using Engine::MetaTooltipHash;
using Engine::MetaCategoryHash;
using Engine::MetaRangeHash;
using Engine::MetaColorHash;
using Engine::MetaEditableHash;
using Engine::MetaReadOnlyHash;
using Engine::MetaFilePathHash;
using Engine::MetaDirectoryHash;
using Engine::MetaEditConditionHash;
using Engine::MetaDefaultHash;
using Engine::MetaOnChangedHash;
using Engine::MetaNoSerializeHash;

struct ParsedMetaRange
{
    f32 Min = -FLT_MAX;
    f32 Max = FLT_MAX;
    f32 Speed = 1.0f;
};

struct ParsedMetaColor 
{
    bool Enabled = true;
    vec4 RGBA = vec4(1.0f);
};

struct ParsedMetaEditCondition
{
    string ConditionVariableName;
    string MaskLiteral;
    bool HasMaskLiteral = false;
    bool bExactMatch = false;
    bool bInvert = false;
};

struct ParsedMetaOnChanged
{
    string FunctionName;
    vector<string> ArgumentPropertyNames;
};

inline string_view TrimView(string_view text)
{
    size_t begin = 0;
    while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin])))
    {
        ++begin;
    }

    size_t end = text.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])))
    {
        --end;
    }

    return text.substr(begin, end - begin);
}

inline string TrimCopy(string_view text)
{
    return string(TrimView(text));
}

inline string UnquoteString(string_view text)
{
    text = TrimView(text);
    if (text.size() >= 2)
    {
        const char first = text.front();
        const char last = text.back();
        if ((first == '"' && last == '"') || (first == '\'' && last == '\''))
        {
            text.remove_prefix(1);
            text.remove_suffix(1);
        }
    }
    return string(text);
}

inline vector<string> SplitMetadataArgs(string_view text)
{
    vector<string> result;
    string current;

    int parenDepth = 0;
    int braceDepth = 0;
    int bracketDepth = 0;
    int angleDepth = 0;
    bool inQuote = false;
    char quoteChar = '\0';

    for (size_t i = 0; i < text.size(); ++i)
    {
        const char ch = text[i];

        if (inQuote)
        {
            current.push_back(ch);
            if (ch == quoteChar && (i == 0 || text[i - 1] != '\\'))
            {
                inQuote = false;
            }
            continue;
        }

        if (ch == '"' || ch == '\'')
        {
            inQuote = true;
            quoteChar = ch;
            current.push_back(ch);
            continue;
        }

        switch (ch)
        {
        case '(': ++parenDepth; break;
        case ')': --parenDepth; break;
        case '{': ++braceDepth; break;
        case '}': --braceDepth; break;
        case '[': ++bracketDepth; break;
        case ']': --bracketDepth; break;
        case '<': ++angleDepth; break;
        case '>': --angleDepth; break;
        default: break;
        }

        if (ch == ',' && parenDepth == 0 && braceDepth == 0 && bracketDepth == 0 && angleDepth == 0)
        {
            result.push_back(TrimCopy(current));
            current.clear();
            continue;
        }

        current.push_back(ch);
    }

    if (!current.empty())
    {
        result.push_back(TrimCopy(current));
    }

    return result;
}

inline bool ParseBoolLiteral(string_view text, bool& outValue)
{
    text = TrimView(text);
    string lowered;
    lowered.reserve(text.size());

    for (char ch : text)
    {
        lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }

    if (lowered == "true" || lowered == "1")
    {
        outValue = true;
        return true;
    }

    if (lowered == "false" || lowered == "0")
    {
        outValue = false;
        return true;
    }

    return false;
}

inline bool ParseFloatLiteral(string_view text, f32& outValue)
{
    const string token = TrimCopy(text);
    if (token.empty())
    {
        return false;
    }

    string normalized;
    normalized.reserve(token.size());
    for (char ch : token)
    {
        if (std::isspace(static_cast<unsigned char>(ch)))
        {
            continue;
        }
        normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }

    if (normalized == "intmax" ||
        normalized == "+intmax" ||
        normalized == "int_max" ||
        normalized == "+int_max" ||
        normalized == "std::numeric_limits<int>::max()" ||
        normalized == "+std::numeric_limits<int>::max()")
    {
        outValue = static_cast<f32>(std::numeric_limits<int>::max());
        return true;
    }

    if (normalized == "intmin" ||
        normalized == "int_min" ||
        normalized == "std::numeric_limits<int>::min()" ||
        normalized == "std::numeric_limits<int>::lowest()")
    {
        outValue = static_cast<f32>(std::numeric_limits<int>::lowest());
        return true;
    }

    if (normalized == "-intmax" ||
        normalized == "-int_max" ||
        normalized == "-std::numeric_limits<int>::max()")
    {
        outValue = static_cast<f32>(-std::numeric_limits<int>::max());
        return true;
    }

    char* endPtr = nullptr;
    const float parsed = std::strtof(token.c_str(), &endPtr);
    if (endPtr == token.c_str())
    {
        return false;
    }

    while (*endPtr != '\0' && std::isspace(static_cast<unsigned char>(*endPtr)))
    {
        ++endPtr;
    }

    if (*endPtr == 'f' || *endPtr == 'F')
    {
        ++endPtr;
    }

    while (*endPtr != '\0' && std::isspace(static_cast<unsigned char>(*endPtr)))
    {
        ++endPtr;
    }

    if (*endPtr != '\0')
    {
        return false;
    }

    outValue = parsed;
    return true;
}

inline bool ParseUnsignedLiteral(string_view text, uint64& outValue)
{
    const string token = TrimCopy(text);
    if (token.empty())
    {
        return false;
    }

    char* endPtr = nullptr;
    const unsigned long long parsed = std::strtoull(token.c_str(), &endPtr, 0);
    if (endPtr == token.c_str())
    {
        return false;
    }

    while (*endPtr != '\0' && std::isspace(static_cast<unsigned char>(*endPtr)))
    {
        ++endPtr;
    }

    if (*endPtr != '\0')
    {
        return false;
    }

    outValue = static_cast<uint64>(parsed);
    return true;
}

inline const Engine::MetadataEntry* FindMetadata(std::span<const Engine::MetadataEntry> metadata, uint64 keyHash)
{
    return Engine::MetadataEntry::Find(metadata, keyHash);
}

inline string GetMetadataString(std::span<const Engine::MetadataEntry> metadata, uint64 keyHash, const string& fallback = "")
{
    const auto* entry = FindMetadata(metadata, keyHash);
    if (!entry)
    {
        return fallback;
    }

    if (const auto* value = std::get_if<string_view>(&entry->Value))
    {
        return string(*value);
    }

    if (const auto* value = std::get_if<wstring_view>(&entry->Value))
    {
        return Engine::WStrToStr(wstring(value->begin(), value->end()));
    }

    return fallback;
}

inline string GetMetadataName(std::span<const Engine::MetadataEntry> metadata, const string& fallback = "")
{
    return GetMetadataString(metadata, MetaNameHash, fallback);
}

inline string GetMetadataTooltip(std::span<const Engine::MetadataEntry> metadata, const string& fallback = "")
{
    return GetMetadataString(metadata, MetaTooltipHash, fallback);
}

inline string GetMetadataCategory(std::span<const Engine::MetadataEntry> metadata, const string& fallback = "")
{
    return GetMetadataString(metadata, MetaCategoryHash, fallback);
}

inline string GetMetadataFilePath(std::span<const Engine::MetadataEntry> metadata, const string& fallback = "")
{
    return GetMetadataString(metadata, MetaFilePathHash, fallback);
}

inline string GetMetadataDirectory(std::span<const Engine::MetadataEntry> metadata, const string& fallback = "")
{
    return GetMetadataString(metadata, MetaDirectoryHash, fallback);
}

inline bool GetMetadataBool(std::span<const Engine::MetadataEntry> metadata, uint64 keyHash, bool fallback = false)
{
    const auto* entry = FindMetadata(metadata, keyHash);
    if (!entry)
    {
        return fallback;
    }

    if (const auto* value = std::get_if<bool>(&entry->Value))
    {
        return *value;
    }

    return fallback;
}

inline std::optional<ParsedMetaRange> GetMetadataRange(std::span<const Engine::MetadataEntry> metadata)
{
    const string literal = GetMetadataString(metadata, MetaRangeHash);
    if (literal.empty())
    {
        return std::nullopt;
    }

    const vector<string> args = SplitMetadataArgs(literal);
    if (args.size() < 2)
    {
        return std::nullopt;
    }

    ParsedMetaRange range;
    if (!ParseFloatLiteral(args[0], range.Min))
    {
        return std::nullopt;
    }

    if (!ParseFloatLiteral(args[1], range.Max))
    {
        return std::nullopt;
    }

    if (args.size() >= 3)
    {
        f32 speed = 0.0f;
        if (ParseFloatLiteral(args[2], speed))
        {
            range.Speed = speed;
        }
    }

    return range;
}

inline std::optional<ParsedMetaColor> GetMetadataColor(std::span<const Engine::MetadataEntry> metadata)
{
    const string literal = GetMetadataString(metadata, MetaColorHash);
    if (literal.empty())
        return std::nullopt;

    const vector<string> args = SplitMetadataArgs(literal);
    ParsedMetaColor result;
    f32 r = 1.f, g = 1.f, b = 1.f, a = 1.f;
    if (args.size() >= 1) ParseFloatLiteral(args[0], r);
    if (args.size() >= 2) ParseFloatLiteral(args[1], g);
    if (args.size() >= 3) ParseFloatLiteral(args[2], b);
    if (args.size() >= 4) ParseFloatLiteral(args[3], a);
    result.RGBA = vec4(r, g, b, a);
    return result;
}

inline bool HasMetadataColor(std::span<const Engine::MetadataEntry> metadata)
{
    return FindMetadata(metadata, MetaColorHash) != nullptr;
}

inline std::optional<ParsedMetaEditCondition> GetMetadataEditCondition(std::span<const Engine::MetadataEntry> metadata)
{
    const string literal = GetMetadataString(metadata, MetaEditConditionHash);
    if (literal.empty())
    {
        return std::nullopt;
    }

    const vector<string> args = SplitMetadataArgs(literal);
    if (args.empty())
    {
        return std::nullopt;
    }

    ParsedMetaEditCondition result;
    result.ConditionVariableName = UnquoteString(args[0]);
    if (result.ConditionVariableName.empty())
    {
        return std::nullopt;
    }

    if (!result.ConditionVariableName.empty() && result.ConditionVariableName.front() == '!')
    {
        result.bInvert = true;
        result.ConditionVariableName.erase(result.ConditionVariableName.begin());
    }

    if (args.size() >= 2)
    {
        result.MaskLiteral = TrimCopy(args[1]);
        result.HasMaskLiteral = !result.MaskLiteral.empty();
    }

    if (args.size() >= 3)
    {
        bool exactMatch = false;
        if (ParseBoolLiteral(args[2], exactMatch))
        {
            result.bExactMatch = exactMatch;
        }
    }

    return result;
}

inline std::optional<ParsedMetaOnChanged> GetMetadataOnChanged(std::span<const Engine::MetadataEntry> metadata)
{
    const string literal = GetMetadataString(metadata, MetaOnChangedHash);
    if (literal.empty())
    {
        return std::nullopt;
    }

    const vector<string> args = SplitMetadataArgs(literal);
    if (args.empty())
    {
        return std::nullopt;
    }

    ParsedMetaOnChanged result;
    result.FunctionName = UnquoteString(args[0]);
    if (result.FunctionName.empty())
    {
        return std::nullopt;
    }

    for (size_t i = 1; i < args.size(); ++i)
    {
        const string propertyName = UnquoteString(args[i]);
        if (!propertyName.empty())
        {
            result.ArgumentPropertyNames.push_back(propertyName);
        }
    }

    return result;
}

inline bool GetMetadataReadOnly(std::span<const Engine::MetadataEntry> metadata)
{
    return GetMetadataBool(metadata, MetaReadOnlyHash, false);
}

inline bool GetMetadataNoSerialize(std::span<const Engine::MetadataEntry> metadata)
{
    return GetMetadataBool(metadata, MetaNoSerializeHash, false);
}

inline bool GetMetadataEditable(std::span<const Engine::MetadataEntry> metadata)
{
    if (GetMetadataBool(metadata, MetaReadOnlyHash, false))
    {
        return true;
    }

    return GetMetadataBool(metadata, MetaEditableHash, false);
}

inline string NormalizeReflectedTypeName(string_view rawTypeName)
{
    string name = TrimCopy(rawTypeName);

    while (name.rfind("class ", 0) == 0)
    {
        name.erase(0, 6);
    }

    while (name.rfind("struct ", 0) == 0)
    {
        name.erase(0, 7);
    }

    while (name.rfind("const ", 0) == 0)
    {
        name.erase(0, 6);
    }

    while (name.rfind("volatile ", 0) == 0)
    {
        name.erase(0, 9);
    }

    while (!name.empty())
    {
        const char last = name.back();
        if (last == '*' || last == '&' || std::isspace(static_cast<unsigned char>(last)))
        {
            name.pop_back();
            continue;
        }
        break;
    }

    return name;
}

inline bool IsQualifiedTypeName(string_view typeName)
{
    return typeName.find("::") != string_view::npos;
}

inline string_view GetShortTypeName(string_view qualifiedName)
{
    const size_t pos = qualifiedName.rfind("::");
    return (pos == string_view::npos) ? qualifiedName : qualifiedName.substr(pos + 2);
}

inline string GetTypeNamespace(string_view qualifiedName)
{
    const size_t pos = qualifiedName.rfind("::");
    if (pos == string_view::npos)
    {
        return {};
    }

    return string(qualifiedName.substr(0, pos));
}

inline const Engine::EnumInfo* ResolveEnumInfo(string_view rawTypeName, const Engine::TypeInfo* ownerTypeInfo = nullptr)
{
    auto& registry = reflection::Registry::Get();

    const string normalized = NormalizeReflectedTypeName(rawTypeName);
    if (normalized.empty())
    {
        return nullptr;
    }

    if (const Engine::EnumInfo* enumInfo = registry.GetEnumByQualifiedName(normalized))
    {
        return enumInfo;
    }

    if (!IsQualifiedTypeName(normalized))
    {
        if (ownerTypeInfo && !ownerTypeInfo->QualifiedName.empty())
        {
            const string ownerNamespace = GetTypeNamespace(ownerTypeInfo->QualifiedName);
            if (!ownerNamespace.empty())
            {
                const string candidate = ownerNamespace + "::" + normalized;
                if (const Engine::EnumInfo* enumInfo = registry.GetEnumByQualifiedName(candidate))
                {
                    return enumInfo;
                }
            }
        }

        const string engineCandidate = "Engine::" + normalized;
        if (const Engine::EnumInfo* enumInfo = registry.GetEnumByQualifiedName(engineCandidate))
        {
            return enumInfo;
        }
    }

    return nullptr;
}

inline bool IsSentinelEnumEntry(string_view entryName)
{
    const string_view shortName = GetShortTypeName(entryName);
    return
        shortName == "Count" ||
        shortName == "MAX" ||
        shortName == "Max" ||
        shortName == "NUM" ||
        shortName == "Num";
}

inline bool ShouldDisplayEnumEntry(const Engine::EnumEntry& entry)
{
    return !IsSentinelEnumEntry(entry.Name);
}

inline bool ShouldDisplayBitFlagEntry(const Engine::EnumEntry& entry)
{
    return !IsSentinelEnumEntry(entry.Name);
}

inline string GetEnumPreviewName(uint64 value, const Engine::EnumInfo* enumInfo)
{
    if (!enumInfo)
    {
        return std::to_string(value);
    }

    for (const auto& entry : enumInfo->Entries)
    {
        if (!ShouldDisplayEnumEntry(entry))
        {
            continue;
        }

        if (entry.Value == value)
        {
            return string(entry.Name);
        }
    }

    return std::to_string(value);
}

inline string GetBitFlagPreviewName(uint64 value, const Engine::EnumInfo* enumInfo)
{
    if (!enumInfo)
    {
        return std::to_string(value);
    }

    if (value == 0)
    {
        for (const auto& entry : enumInfo->Entries)
        {
            if (!ShouldDisplayBitFlagEntry(entry))
            {
                continue;
            }

            if (entry.Value == 0)
            {
                return string(entry.Name);
            }
        }

        return "None";
    }

    string result;
    for (const auto& entry : enumInfo->Entries)
    {
        if (!ShouldDisplayBitFlagEntry(entry))
        {
            continue;
        }

        if (entry.Value == 0)
        {
            continue;
        }

        if ((value & entry.Value) == entry.Value)
        {
            if (!result.empty())
            {
                result += " | ";
            }
            result += entry.Name;
        }
    }

    return result.empty() ? std::to_string(value) : result;
}

inline const Engine::PropertyInfo* FindPropertyByName(const Engine::TypeInfo& typeInfo, string_view propertyName)
{
    const Engine::TypeInfo* current = &typeInfo;

    while (current != nullptr)
    {
        for (const auto& prop : current->Properties)
        {
            if (prop.Name == propertyName)
            {
                return &prop;
            }
        }

        if (current->ParentQualifiedName.empty())
        {
            break;
        }

        current = reflection::Registry::Get().GetTypeByQualifiedName(current->ParentQualifiedName);
    }

    return nullptr;
}

inline bool TryResolveEnumLiteral(string_view token, const Engine::EnumInfo& enumInfo, uint64& outValue)
{
    token = TrimView(token);

    if (ParseUnsignedLiteral(token, outValue))
    {
        return true;
    }

    const string_view tokenShort = GetShortTypeName(token);

    for (const auto& entry : enumInfo.Entries)
    {
        const string_view entryShort = GetShortTypeName(entry.Name);
        if (entry.Name == token || entryShort == token || entryShort == tokenShort)
        {
            outValue = entry.Value;
            return true;
        }
    }

    return false;
}

inline bool IsOnChangedFunctionMatch(const Engine::TypeInfo& typeInfo, const Engine::FunctionInfo& functionInfo, const ParsedMetaOnChanged& onChanged)
{
    if (functionInfo.Parameters.size() != onChanged.ArgumentPropertyNames.size())
    {
        return false;
    }

    for (size_t i = 0; i < onChanged.ArgumentPropertyNames.size(); ++i)
    {
        const Engine::PropertyInfo* propertyInfo = FindPropertyByName(typeInfo, onChanged.ArgumentPropertyNames[i]);
        if (!propertyInfo)
        {
            return false;
        }

        const Engine::VariableInfo& parameterInfo = functionInfo.Parameters[i];
        if (parameterInfo.Type != propertyInfo->TypeInfo.Type)
        {
            return false;
        }

        const string parameterTypeName = NormalizeReflectedTypeName(parameterInfo.Name);
        const string propertyTypeName = NormalizeReflectedTypeName(propertyInfo->TypeInfo.Name);

        if (!parameterTypeName.empty() && !propertyTypeName.empty() && parameterTypeName != propertyTypeName)
        {
            return false;
        }
    }

    return true;
}

inline const Engine::FunctionInfo* FindOnChangedFunction(const Engine::TypeInfo& typeInfo, const ParsedMetaOnChanged& onChanged)
{
    const Engine::TypeInfo* current = &typeInfo;

    while (current != nullptr)
    {
        for (const auto& func : current->Functions)
        {
            if (func.Name != onChanged.FunctionName)
            {
                continue;
            }

            if (IsOnChangedFunctionMatch(typeInfo, func, onChanged))
            {
                return &func;
            }
        }

        if (current->ParentQualifiedName.empty())
        {
            break;
        }

        current = reflection::Registry::Get().GetTypeByQualifiedName(current->ParentQualifiedName);
    }

    return nullptr;
}

inline int64 ReadInteger(void* data, size_t size)
{
    switch (size)
    {
    case 1: return *reinterpret_cast<int8_t*>(data);
    case 2: return *reinterpret_cast<int16_t*>(data);
    case 4: return *reinterpret_cast<int32_t*>(data);
    case 8: return *reinterpret_cast<int64_t*>(data);
    default: return 0;
    }
}

inline uint64 ReadUnsignedInteger(const void* data, size_t size)
{
    switch (size)
    {
    case 1: return *reinterpret_cast<const uint8_t*>(data);
    case 2: return *reinterpret_cast<const uint16_t*>(data);
    case 4: return *reinterpret_cast<const uint32_t*>(data);
    case 8: return *reinterpret_cast<const uint64_t*>(data);
    default: return 0;
    }
}

inline void WriteInteger(void* data, size_t size, int64 value)
{
    switch (size)
    {
    case 1: *reinterpret_cast<int8_t*>(data) = static_cast<int8_t>(value); break;
    case 2: *reinterpret_cast<int16_t*>(data) = static_cast<int16_t>(value); break;
    case 4: *reinterpret_cast<int32_t*>(data) = static_cast<int32_t>(value); break;
    case 8: *reinterpret_cast<int64_t*>(data) = static_cast<int64_t>(value); break;
    default: break;
    }
}

inline void WriteUnsignedInteger(void* data, size_t size, uint64 value)
{
    switch (size)
    {
    case 1: *reinterpret_cast<uint8_t*>(data) = static_cast<uint8_t>(value); break;
    case 2: *reinterpret_cast<uint16_t*>(data) = static_cast<uint16_t>(value); break;
    case 4: *reinterpret_cast<uint32_t*>(data) = static_cast<uint32_t>(value); break;
    case 8: *reinterpret_cast<uint64_t*>(data) = static_cast<uint64_t>(value); break;
    default: break;
    }
}

inline bool InvokeOnChanged(void* instance, const Engine::TypeInfo& typeInfo, std::span<const Engine::MetadataEntry> metadata)
{
    const auto onChanged = GetMetadataOnChanged(metadata);
    if (!onChanged.has_value())
    {
        return false;
    }

    const Engine::FunctionInfo* functionInfo = FindOnChangedFunction(typeInfo, *onChanged);
    if (!functionInfo)
    {
        return false;
    }

    vector<void*> arguments;
    arguments.reserve(onChanged->ArgumentPropertyNames.size());

    for (const string& propertyName : onChanged->ArgumentPropertyNames)
    {
        const Engine::PropertyInfo* propertyInfo = FindPropertyByName(typeInfo, propertyName);
        if (!propertyInfo)
        {
            return false;
        }

        void* argumentPtr = reinterpret_cast<uint8*>(instance) + propertyInfo->Offset;
        arguments.push_back(argumentPtr);
    }

    functionInfo->Invoke(instance, arguments.empty() ? nullptr : arguments.data());
    return true;
}

inline bool CheckEditCondition(void* instance, const TypeInfo& typeInfo, std::span<const Engine::MetadataEntry> metadata)
{
    const auto editCondition = GetMetadataEditCondition(metadata);
    if (!editCondition.has_value())
    {
        return true;
    }

    if (editCondition->ConditionVariableName.empty())
    {
        return true;
    }

    const Engine::PropertyInfo* conditionProp = FindPropertyByName(typeInfo, editCondition->ConditionVariableName);
    if (!conditionProp)
    {
        return true;
    }

    void* valuePtr = reinterpret_cast<uint8*>(instance) + conditionProp->Offset;
    bool conditionResult = false;

    if (conditionProp->TypeInfo.Type == EPropertyType::Enum || conditionProp->TypeInfo.Type == EPropertyType::BitFlag)
    {
        const uint64 currentValue = ReadUnsignedInteger(valuePtr, conditionProp->Size);
        uint64 mask = 0;

        if (editCondition->HasMaskLiteral)
        {
            const Engine::EnumInfo* enumInfo = reflection::Registry::Get().GetEnumByQualifiedName(conditionProp->TypeInfo.Name);
            if (enumInfo)
            {
                if (!TryResolveEnumLiteral(editCondition->MaskLiteral, *enumInfo, mask))
                {
                    return true;
                }
            }
            else if (!ParseUnsignedLiteral(editCondition->MaskLiteral, mask))
            {
                return true;
            }
        }

        if (editCondition->bExactMatch)
        {
            conditionResult = (currentValue == mask);
        }
        else if (!editCondition->HasMaskLiteral || mask == 0)
        {
            conditionResult = (currentValue != 0);
        }
        else
        {
            conditionResult = ((currentValue & mask) == mask);
        }
    }
    else
    {
        conditionResult = *reinterpret_cast<bool*>(valuePtr);
    }

    return editCondition->bInvert ? !conditionResult : conditionResult;
}

END