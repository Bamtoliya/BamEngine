#pragma once

#include "GlazeArchiveBase.h"

BEGIN(Engine)
class ENGINE_API JsonArchive : public GlazeArchiveBase
{
public:
	explicit JsonArchive(EArchiveMode mode) : GlazeArchiveBase(mode) {}
	virtual ~JsonArchive() = default;
public:
	virtual bool SaveToFile(string_view filePath) override;
	virtual bool LoadFromFile(string_view filePath) override;
};
END