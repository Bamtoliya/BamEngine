#pragma once

#include "Editor_Includes.h"

BEGIN(Editor)
class Importer : public Base
{
protected:
	Importer() = default;
	virtual ~Importer() = default;
	virtual EResult Initialize(void* arg = nullptr) { return EResult::Success; }
public:
	virtual void Free() override { Base::Free(); }

public:
	virtual EResult Import(const filesystem::path& filePath) PURE;

#pragma region Member Variable
private:

#pragma endregion
};
END