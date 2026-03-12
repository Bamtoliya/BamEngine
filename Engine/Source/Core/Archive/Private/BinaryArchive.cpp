#pragma once
#include "BinaryArchive.h"

bool BinaryArchive::PushScope(string_view key)
{
    return false;
}

void BinaryArchive::PopScope()
{
}

void BinaryArchive::Process(string_view key, bool& v)
{
}

void BinaryArchive::Process(string_view key, int32& v)
{
}

void BinaryArchive::Process(string_view key, uint32& v)
{
}

void BinaryArchive::Process(string_view key, uint64& v)
{
}

void BinaryArchive::Process(string_view key, f32& v)
{
}

void BinaryArchive::Process(string_view key, f64& v)
{
}

void BinaryArchive::Process(string_view key, string& v)
{
}

void BinaryArchive::Process(string_view key, wstring& v)
{
}

void BinaryArchive::Process(string_view key, glm::vec2& v)
{
}

void BinaryArchive::Process(string_view key, glm::vec3& v)
{
}

void BinaryArchive::Process(string_view key, glm::vec4& v)
{
}

void BinaryArchive::Process(string_view key, glm::quat& v)
{
}

void BinaryArchive::Process(string_view key, glm::mat4& v)
{
}

void BinaryArchive::ProcessEnum(string_view key, void* enumPtr, size_t size)
{
}

size_t BinaryArchive::BeginArray(string_view key)
{
    return size_t();
}

void BinaryArchive::EndArray()
{
}

void BinaryArchive::BeginArrayElement()
{
}

void BinaryArchive::EndArrayElement()
{
}

size_t BinaryArchive::BeginMap(string_view key)
{
    return size_t();
}

void BinaryArchive::EndMap()
{
}

void BinaryArchive::BeginMapElement(string& outKey)
{
}

void BinaryArchive::EndMapElement()
{
}

#pragma region Save&Load
bool BinaryArchive::SaveToFile(string_view filePath)
{
    return false;
}

bool BinaryArchive::LoadFromFile(string_view filePath)
{
    return false;
}
#pragma endregion
