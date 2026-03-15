#pragma once

#include "Editor_Includes.h"
#include "Archives.h"

BEGIN(Editor)
class ImporterInterface : public Base
{
protected:
	ImporterInterface() = default;
	virtual ~ImporterInterface() = default;
	virtual EResult Initialize(void* arg = nullptr) { return EResult::Success; }
public:
	virtual void Free() override { Base::Free(); }

public:
	virtual EResult Import(const filesystem::path & sourcePath, const filesystem::path & destDir) PURE;

#pragma region Member Variable
private:

#pragma endregion
};
END