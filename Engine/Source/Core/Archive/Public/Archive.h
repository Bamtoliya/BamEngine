#pragma once

#include "Engine_Includes.h"

BEGIN(Engine)
enum class EArchiveMode : uint8_t { Read, Write };
class Archive
{
public:
	explicit Archive(EArchiveMode mode) : m_Mode(mode) {}
	virtual ~Archive() = default;
public:
	bool IsReading() const { return m_Mode == EArchiveMode::Read; }
	bool IsWriting() const { return m_Mode == EArchiveMode::Write; }
	EArchiveMode GetMode() const { return m_Mode; }
public:
	virtual void Process(string_view key, void* value) PURE;

protected:
	EArchiveMode m_Mode;
};
END