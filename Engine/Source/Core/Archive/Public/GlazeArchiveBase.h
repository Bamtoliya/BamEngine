#pragma once

#include "Archive.h"
#include <glaze/json.hpp>

BEGIN(Engine)
class ENGINE_API GlazeArchiveBase : public Archive
{
public:
	explicit GlazeArchiveBase(EArchiveMode mode) : Archive(mode) { m_Root = glz::json_t::object_t{}; m_ScopeStack.push(&m_Root); }
	virtual ~GlazeArchiveBase() = default;
public:
	bool PushScope(string_view key) override;
	void PopScope() override;
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
public:
	void ProcessEnum(string_view key, void* enumPtr, size_t size) override;
public:
	size_t BeginArray(string_view key) override;
	void EndArray() override;
	void BeginArrayElement() override;
	void EndArrayElement() override;
	size_t BeginMap(string_view key) override;
	void EndMap() override;
	void BeginMapElement(string& outKey) override;
	void EndMapElement() override;

protected:
	glz::json_t m_Root;

	std::stack<size_t> m_ArrayIndexStack;

	std::stack<glz::json_t*> m_ScopeStack;

	std::stack<glz::json_t::object_t::iterator> m_MapIteratorStack;
};
END