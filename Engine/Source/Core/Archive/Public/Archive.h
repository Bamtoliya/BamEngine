#pragma once

#include "Engine_Includes.h"

BEGIN(Engine)
enum class EArchiveMode : uint8_t { Read, Write };
class ENGINE_API Archive abstract
{
public:
	explicit Archive(EArchiveMode mode) : m_Mode(mode) {}
	virtual ~Archive() = default;
public:
	bool IsReading() const { return m_Mode == EArchiveMode::Read; }
	bool IsWriting() const { return m_Mode == EArchiveMode::Write; }
	EArchiveMode GetMode() const { return m_Mode; }
public:
	virtual bool PushScope(string_view key) PURE;
	virtual void PopScope() PURE;
public:
	template<typename T>
	void Process(string_view key, T& type);
	virtual void Process(string_view key, bool& v) PURE;
	virtual void Process(string_view key, int8& v) PURE;
	virtual void Process(string_view key, int16& v) PURE;
	virtual void Process(string_view key, int32& v) PURE;
	virtual void Process(string_view key, int64& v) PURE;
	virtual void Process(string_view key, uint8& v) PURE;
	virtual void Process(string_view key, uint32& v) PURE;
	virtual void Process(string_view key, uint16& v) PURE;
	virtual void Process(string_view key, uint64& v) PURE;
	virtual void Process(string_view key, f32& v) PURE;
	virtual void Process(string_view key, f64& v) PURE;
	virtual void Process(string_view key, string& v) PURE;
	virtual void Process(string_view key, wstring& v) PURE;
public:
	virtual void Process(string_view key, glm::vec2& v) PURE;
	virtual void Process(string_view key, glm::vec3& v) PURE;
	virtual void Process(string_view key, glm::vec4& v) PURE;
	virtual void Process(string_view key, glm::quat& v) PURE;
	virtual void Process(string_view key, glm::mat3& v) PURE;
	virtual void Process(string_view key, glm::mat4& v) PURE;
public:
	virtual void ProcessRaw(string_view key, const void* data, size_t size) PURE;
public:
	virtual void ProcessEnum(string_view key, void* enumPtr, size_t size) PURE;
public:
	virtual size_t BeginArray(string_view key) PURE; // 로드 시 크기 반환
	virtual void   EndArray() PURE;
	virtual void   BeginArrayElement() PURE;
	virtual void   EndArrayElement() PURE;
public:
	virtual size_t BeginMap(string_view key) PURE;
	virtual void   EndMap() PURE;
	virtual void   BeginMapElement(string& outKey) PURE;
	virtual void   EndMapElement() PURE;

public:
	virtual bool SaveToFile(string_view filePath) { return false; }
	virtual bool LoadFromFile(string_view filePath) { return false; }

	virtual bool SaveToFile(wstring_view filePath) { return SaveToFile(WStrToStr(filePath.data())); }
	virtual bool LoadFromFile(wstring_view filePath) { return LoadFromFile(WStrToStr(filePath.data())); }


protected:
	EArchiveMode m_Mode;
};
END