#pragma once

#include "Archive.h"
#include "ReflectionTypes.h"

BEGIN(Engine)

enum class EBinTag : uint8
{
	Value = 1,
	Object = 2,   // PushScope / PopScope
	Array = 3,    // BeginArray / EndArray
	Map = 4       // BeginMap / EndMap
};
struct BinFileHeader
{
	uint32 Magic = 0x42414D42; // 'BAMB' (BamEngine Binary)
	uint16 Major = 1;
	uint16 Minor = 0;
};
struct BinNodeHeader
{
	uint64 KeyHash;
	uint8  Tag;
	uint8  ValueType;
	uint16 Reserved = 0;
	uint32 PayloadSize;
};
struct NodeView {
	BinNodeHeader Header;
	size_t PayloadOffset;
};

class ENGINE_API BinaryArchive : public Archive
{
private:
	struct ScopeWriteState
	{
		size_t HeaderOffset;
		bool HasHeader;
		bool IsSequential;
		size_t CountOffset; // For arrays
		size_t ElementCount; // For arrays
	};

	struct ScopeReadState
	{
		size_t Begin = 0;
		size_t End = 0;
		bool IsSequential = false;
		std::unordered_map<uint64, NodeView> Index;
	};
	
	
public:
	explicit BinaryArchive(EArchiveMode mode) : Archive(mode), m_Cursor(0)
	{
		if (IsWriting())
		{
			m_WriteScopeStack.push({ 0, false, false, 0, 0 }); // Root scope
			BinFileHeader header;
			WriteRaw(&header, sizeof(header));
		}
	}
	virtual ~BinaryArchive() = default;
public:
	bool PushScope(string_view key) override;
	void PopScope() override;
public:
	template<typename T>
	void ProcessPrimitive(string_view key, Engine::EPropertyType type, T& value);
public:
	void Process(string_view key, bool& v) override;
	void Process(string_view key, int8& v) override;
	void Process(string_view key, int16& v) override;
	void Process(string_view key, int32& v) override;
	void Process(string_view key, int64& v) override;
	void Process(string_view key, uint8& v) override;
	void Process(string_view key, uint16& v) override;
	void Process(string_view key, uint32& v) override;
	void Process(string_view key, uint64& v) override;
	void Process(string_view key, f32& v) override;
	void Process(string_view key, f64& v) override;
	void Process(string_view key, string& v) override;
	void Process(string_view key, wstring& v) override;
	void Process(string_view key, glm::vec2& v) override;
	void Process(string_view key, glm::vec3& v) override;
	void Process(string_view key, glm::vec4& v) override;
	void Process(string_view key, glm::quat& v) override;
	void Process(string_view key, glm::mat3& v) override;
	void Process(string_view key, glm::mat4& v) override;
	void ProcessEnum(string_view key, void* enumPtr, size_t size) override;
public:
	void ProcessRaw(string_view key, const void* data, size_t size) override;
public:
	size_t BeginArray(string_view key) override;
	void EndArray() override;
	void BeginArrayElement() override;
	void EndArrayElement() override;

	size_t BeginMap(string_view key) override;
	void EndMap() override;
	void BeginMapElement(string& outKey) override;
	void EndMapElement() override;
public:
	virtual bool SaveToFile(string_view filePath) override;
	virtual bool LoadFromFile(string_view filePath)  override;
private:
	bool IsCurrentSequential() const;
	void WriteRaw(const void* data, size_t size);
	void ReadRaw(void* data, size_t size);
	void WriteNodeHeader(string_view key, EBinTag tag, uint8 valueType, uint32 payloadSize);
	void ParseScopeDirectory(size_t endOffset, size_t startCursor = 0);


private:
	vector<uint8> m_Buffer;
	size_t m_Cursor;

	stack<ScopeWriteState> m_WriteScopeStack;
	stack<ScopeReadState> m_ReadScopeStack;
};
END

template<typename T>
inline void BinaryArchive::ProcessPrimitive(string_view key, Engine::EPropertyType type, T& v)
{
	if (IsWriting())
	{
		if (!key.empty() && !IsCurrentSequential()) {
			WriteNodeHeader(key, EBinTag::Value, static_cast<uint8>(type), sizeof(T));
		}
		WriteRaw(&v, sizeof(T));
	}
	else
	{
		if (!key.empty() && !IsCurrentSequential()) {
			if (m_ReadScopeStack.empty()) return;
			uint64 keyHash = RunTimeHash(key);
			auto it = m_ReadScopeStack.top().Index.find(keyHash);

			if (it != m_ReadScopeStack.top().Index.end()) {
				// 저장된 타입과 요구하는 타입이 같은지 검증(ValueType) 할 수도 있습니다.
				if (it->second.PayloadOffset + sizeof(T) <= m_Buffer.size()) {
					std::memcpy(&v, m_Buffer.data() + it->second.PayloadOffset, sizeof(T));
				}
			}
		}
		else {
			if (m_Cursor + sizeof(T) <= m_Buffer.size()) {
				std::memcpy(&v, m_Buffer.data() + m_Cursor, sizeof(T));
				m_Cursor += sizeof(T);
			}
		}
	}
}
