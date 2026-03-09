#pragma once

#include "Editor_Includes.h"

BEGIN(Editor)
class ExporterInterface : public Base
{
protected:
	ExporterInterface() = default;
	virtual ~ExporterInterface() = default;
	virtual EResult Initialize(void* arg = nullptr) { return EResult::Success; }
public:
	virtual void Free() override { Base::Free(); }

public:
	virtual EResult Export(const filesystem::path& sourcePath, const filesystem::path& destDir) PURE;

#pragma region Member Variable
private:

};
END