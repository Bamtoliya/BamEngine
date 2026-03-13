#pragma once
#include "BinaryArchive.h"

bool BinaryArchive::PushScope(string_view key)
{
	if (IsWriting())
	{
		size_t headerOffset = m_Buffer.size();
		if (!key.empty() && !IsCurrentSequential())
		{
			WriteNodeHeader(key, EBinTag::Object, static_cast<uint8>(Engine::EPropertyType::Object), 0); // Placeholder header, will be updated later
		}
		m_WriteScopeStack.push({ headerOffset, false, false, 0, 0 });
	}
	else
	{
		if (!key.empty() && !IsCurrentSequential())
		{
			uint64 keyHash = RunTimeHash(key);
			auto& currentScope = m_ReadScopeStack.top();
			auto it = currentScope.Index.find(keyHash);

			if (it != currentScope.Index.end() && it->second.Header.Tag == static_cast<uint8>(EBinTag::Object))
			{
				ParseScopeDirectory(it->second.PayloadOffset + it->second.Header.PayloadSize, it->second.PayloadOffset);
				return true;
			}
			return false;
		}
		else
		{
			// Sequential scope, just push a new scope with the current cursor position
			uint32 payloadSize = 0;
			ReadRaw(&payloadSize, sizeof(uint32));
			ParseScopeDirectory(m_Cursor + payloadSize, m_Cursor);
			return true;
		}
	}
    return false;
}

void BinaryArchive::PopScope()
{
	if (IsWriting())
	{
		ScopeWriteState currentScope = m_WriteScopeStack.top();
		m_WriteScopeStack.pop();

		if (!m_WriteScopeStack.top().IsSequential && m_Buffer.size() > currentScope.HeaderOffset)
		{
			uint32 payloadSize = static_cast<uint32>(m_Buffer.size() - currentScope.HeaderOffset - sizeof(BinNodeHeader));
			size_t payloadSizeOffset = currentScope.HeaderOffset + offsetof(BinNodeHeader, PayloadSize);
			std::memcpy(m_Buffer.data() + payloadSizeOffset, &payloadSize, sizeof(uint32));
		}
	}
	else
	{
		if (!m_ReadScopeStack.empty())
		{
			m_ReadScopeStack.pop();
		}
	}
}

void BinaryArchive::Process(string_view key, bool& v)
{
	ProcessPrimitive(key, Engine::EPropertyType::Bool, v);
}

void BinaryArchive::Process(string_view key, int8& v)
{
	ProcessPrimitive(key, Engine::EPropertyType::Int8, v);
}

void BinaryArchive::Process(string_view key, int16& v)
{
	ProcessPrimitive(key, Engine::EPropertyType::Int16, v);
}

void BinaryArchive::Process(string_view key, int32& v)
{
	ProcessPrimitive(key, Engine::EPropertyType::Int32, v);
}

void BinaryArchive::Process(string_view key, int64& v)
{
	ProcessPrimitive(key, Engine::EPropertyType::Int64, v);
}

void BinaryArchive::Process(string_view key, uint8& v)
{
	ProcessPrimitive(key, Engine::EPropertyType::UInt8, v);
}

void BinaryArchive::Process(string_view key, uint16& v)
{
	ProcessPrimitive(key, Engine::EPropertyType::UInt16, v);
}

void BinaryArchive::Process(string_view key, uint32& v)
{
	ProcessPrimitive(key, Engine::EPropertyType::UInt32, v);
}

void BinaryArchive::Process(string_view key, uint64& v)
{
	ProcessPrimitive(key, Engine::EPropertyType::UInt64, v);
}

void BinaryArchive::Process(string_view key, f32& v)
{
	ProcessPrimitive(key, Engine::EPropertyType::F32, v);
}

void BinaryArchive::Process(string_view key, f64& v)
{
	ProcessPrimitive(key, Engine::EPropertyType::F64, v);
}

void BinaryArchive::Process(string_view key, string& v)
{
	if (IsWriting())
	{
		uint32 strLen = static_cast<uint32>(v.size());
		uint32 totalPayloadSize = sizeof(uint32) + strLen; // 문자열 길이(4바이트) + 실제 문자열 버퍼

		if (!key.empty() && !IsCurrentSequential()) {
			WriteNodeHeader(key, EBinTag::Value, static_cast<uint8>(EPropertyType::String), totalPayloadSize);
		}
		WriteRaw(&strLen, sizeof(uint32));
		if (strLen > 0) WriteRaw(v.data(), strLen);
	}
	else
	{
		if (!key.empty() && !IsCurrentSequential()) {
			if (m_ReadScopeStack.empty()) return;
			uint64 keyHash = RunTimeHash(key);
			auto it = m_ReadScopeStack.top().Index.find(keyHash);

			if (it != m_ReadScopeStack.top().Index.end()) {
				size_t offset = it->second.PayloadOffset;
				uint32 strLen = 0;
				std::memcpy(&strLen, m_Buffer.data() + offset, sizeof(uint32));
				offset += sizeof(uint32);

				if (strLen > 0 && offset + strLen <= m_Buffer.size()) {
					v.assign(reinterpret_cast<const char*>(m_Buffer.data() + offset), strLen);
				}
				else { v.clear(); }
			}
		}
		else {
			uint32 strLen = 0;
			ReadRaw(&strLen, sizeof(uint32));
			if (strLen > 0 && m_Cursor + strLen <= m_Buffer.size()) {
				v.assign(reinterpret_cast<const char*>(m_Buffer.data() + m_Cursor), strLen);
				m_Cursor += strLen;
			}
			else { v.clear(); }
		}
	}
}

void BinaryArchive::Process(string_view key, wstring& v)
{
	if (IsWriting()) {
		string utf8 = Engine::WStrToStr(v);
		uint32 strLen = static_cast<uint32>(utf8.size());
		uint32 totalPayloadSize = sizeof(uint32) + strLen;

		if (!key.empty() && !IsCurrentSequential()) {
			WriteNodeHeader(key, EBinTag::Value, static_cast<uint8>(EPropertyType::Wstring), totalPayloadSize);
		}
		WriteRaw(&strLen, sizeof(uint32));
		if (strLen > 0) WriteRaw(utf8.data(), strLen);
	}
	else {
		string utf8;
		Process(key, utf8);
		v = Engine::StrToWStr(utf8);
	}
}

void BinaryArchive::Process(string_view key, glm::vec2& v)
{
	ProcessPrimitive(key, Engine::EPropertyType::Vector2, v);
}

void BinaryArchive::Process(string_view key, glm::vec3& v)
{
	ProcessPrimitive(key, Engine::EPropertyType::Vector3, v);
}

void BinaryArchive::Process(string_view key, glm::vec4& v)
{
	ProcessPrimitive(key, Engine::EPropertyType::Vector4, v);
}

void BinaryArchive::Process(string_view key, glm::quat& v)
{
	ProcessPrimitive(key, Engine::EPropertyType::Quaternion, v);
}

void BinaryArchive::Process(string_view key, glm::mat3& v)
{
	ProcessPrimitive(key, Engine::EPropertyType::Matrix3, v);
}

void BinaryArchive::Process(string_view key, glm::mat4& v)
{
	ProcessPrimitive(key, Engine::EPropertyType::Matrix4, v);
}

void BinaryArchive::ProcessEnum(string_view key, void* enumPtr, size_t size)
{
	if (IsWriting())
	{
		if(!key.empty() && !IsCurrentSequential()) {
			WriteNodeHeader(key, EBinTag::Value, static_cast<uint8>(Engine::EPropertyType::Enum), static_cast<uint32>(size));
		}
		WriteRaw(enumPtr, size);
	}
	else
	{
		if (!key.empty() && !IsCurrentSequential()) {
			if (m_ReadScopeStack.empty()) return;
			uint64 keyHash = RunTimeHash(key);
			auto it = m_ReadScopeStack.top().Index.find(keyHash);
			if (it != m_ReadScopeStack.top().Index.end() && it->second.Header.Tag == static_cast<uint8>(EBinTag::Value)) {
				const BinNodeHeader& header = it->second.Header;
				if (header.ValueType == static_cast<uint8>(Engine::EPropertyType::Enum) && header.PayloadSize == size) {
					std::memcpy(enumPtr, m_Buffer.data() + it->second.PayloadOffset, size);
				}
			}
		}
		else {
			if (m_Cursor + size <= m_Buffer.size()) {
				std::memcpy(enumPtr, m_Buffer.data() + m_Cursor, size);
				m_Cursor += size;
			}
		}
	}
}

size_t BinaryArchive::BeginArray(string_view key)
{
	if (IsWriting()) {
		bool hasHeader = !key.empty() && !IsCurrentSequential();
		size_t headerPos = m_Buffer.size();

		if (hasHeader) {
			WriteNodeHeader(key, EBinTag::Array, static_cast<uint8>(EPropertyType::Array), 0);
		}

		size_t countOffset = m_Buffer.size();
		uint32 placeholderCount = 0;
		WriteRaw(&placeholderCount, sizeof(uint32));

		m_WriteScopeStack.push({ headerPos, hasHeader, true, countOffset, 0 });
		return 0;
	}
	else {
		if (!key.empty() && !IsCurrentSequential()) {
			auto& ctx = m_ReadScopeStack.top();
			auto it = ctx.Index.find(RunTimeHash(key));
			if (it != ctx.Index.end()) {
				m_Cursor = it->second.PayloadOffset;
				uint32 count = 0; ReadRaw(&count, sizeof(uint32));

				ScopeReadState newCtx;
				newCtx.Begin = m_Cursor;
				newCtx.End = it->second.PayloadOffset + it->second.Header.PayloadSize;
				newCtx.IsSequential = true;
				m_ReadScopeStack.push(std::move(newCtx));
				return count;
			}
			return 0;
		}
		else {
			uint32 count = 0; ReadRaw(&count, sizeof(uint32));
			ScopeReadState newCtx;
			newCtx.Begin = m_Cursor;
			newCtx.End = m_Buffer.size();
			newCtx.IsSequential = true;
			m_ReadScopeStack.push(std::move(newCtx));
			return count;
		}
	}
}

void BinaryArchive::EndArray()
{
	PopScope();
}

void BinaryArchive::BeginArrayElement()
{
}

void BinaryArchive::EndArrayElement()
{
	if (IsWriting() && !m_WriteScopeStack.empty()) {
		m_WriteScopeStack.top().ElementCount++;
	}
}

size_t BinaryArchive::BeginMap(string_view key)
{
	if (IsWriting()) {
		bool hasHeader = !key.empty() && !IsCurrentSequential();
		size_t headerPos = m_Buffer.size();
		if (hasHeader) {
			WriteNodeHeader(key, EBinTag::Map, static_cast<uint8>(EPropertyType::Map), 0);
		}
		size_t countOffset = m_Buffer.size();
		uint32 placeholderCount = 0;
		WriteRaw(&placeholderCount, sizeof(uint32));

		m_WriteScopeStack.push({ headerPos, hasHeader, true, countOffset, 0 });
		return 0;
	}
	else {
		return BeginArray(key); // 읽기 로직은 Array와 동일
	}
}

void BinaryArchive::EndMap()
{
	PopScope();
}

void BinaryArchive::BeginMapElement(string& outKey)
{
}

void BinaryArchive::EndMapElement()
{
	if (IsWriting() && !m_WriteScopeStack.empty()) {
		m_WriteScopeStack.top().ElementCount++;
	}
}

#pragma region Save&Load
bool BinaryArchive::SaveToFile(string_view filePath)
{
	if (IsReading()) return false;

	std::ofstream outFile(filePath.data(), std::ios::binary);
    if (!outFile || !outFile.is_open()) return false;
    outFile.write(reinterpret_cast<const char*>(m_Buffer.data()), m_Buffer.size());
    return true;
}

bool BinaryArchive::LoadFromFile(string_view filePath)
{
    if (IsWriting()) return false;
	std::ifstream inFile(filePath.data(), std::ios::binary);
	if (!inFile || !inFile.is_open()) return false;

    size_t size = (size_t)inFile.tellg();
	inFile.seekg(0);
	m_Buffer.resize(size);
	inFile.read(reinterpret_cast<char*>(m_Buffer.data()), size);
	m_Cursor = 0;

	if (size < sizeof(BinFileHeader)) return false;
	BinFileHeader header;
	ReadRaw(&header, sizeof(header));

	if (header.Magic != 0x42414D42) return false; // 'BAMB'

	ParseScopeDirectory(size);
    return true;
}

#pragma endregion
#pragma region Helpers
bool BinaryArchive::IsCurrentSequential() const
{
	if(IsWriting()) return m_WriteScopeStack.empty() ? false : m_WriteScopeStack.top().IsSequential;
	else return m_ReadScopeStack.empty() ? false : m_ReadScopeStack.top().IsSequential;
}
void BinaryArchive::WriteRaw(const void* data, size_t size)
{
	if (size == 0) return;
	const uint8_t* ptr = static_cast<const uint8_t*>(data);
	m_Buffer.insert(m_Buffer.end(), ptr, ptr + size);
}
void BinaryArchive::ReadRaw(void* data, size_t size)
{
	if (m_Cursor + size <= m_Buffer.size())
	{
		std::memcpy(data, m_Buffer.data() + m_Cursor, size);
		m_Cursor += size;
	}
}

void BinaryArchive::WriteNodeHeader(string_view key, EBinTag tag, uint8 valueType, uint32 payloadSize)
{
	BinNodeHeader header;
	header.KeyHash = RunTimeHash(key);
	header.Tag = static_cast<uint8>(tag);
	header.ValueType = valueType;
	header.PayloadSize = payloadSize;
	WriteRaw(&header, sizeof(header));
}

void BinaryArchive::ParseScopeDirectory(size_t endOffset, size_t startCursor)
{
	if (startCursor > 0) m_Cursor = startCursor;

	ScopeReadState newScope;
	newScope.Begin = m_Cursor;
	newScope.End = endOffset;
	newScope.IsSequential = false;

	while (m_Cursor < endOffset && m_Cursor + sizeof(BinNodeHeader) <= m_Buffer.size())
	{
		BinNodeHeader header;
		ReadRaw(&header, sizeof(BinNodeHeader));

		NodeView view;
		view.Header = header;
		view.PayloadOffset = m_Cursor;
		newScope.Index[header.KeyHash] = view;

		m_Cursor += header.PayloadSize;
	}
	m_ReadScopeStack.push(std::move(newScope));
}

#pragma endregion