#pragma once

#include "GlazeArchiveBase.h"

BEGIN(Engine)
class BeveArchive : public GlazeArchiveBase
{
public:
	explicit BeveArchive(EArchiveMode mode) : GlazeArchiveBase(mode) {}
	virtual ~BeveArchive() = default;
public:
	virtual bool SaveToFile(string_view filePath) override;
	virtual bool LoadFromFile(string_view filePath) override;
};
END