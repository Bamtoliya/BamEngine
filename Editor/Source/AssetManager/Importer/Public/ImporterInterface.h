#pragma once

#include "Editor_Includes.h"
#include "ImportJob.h"

#pragma push_macro("new")
#undef new
#include <memory>
#pragma pop_macro("new")

BEGIN(Editor)
class IImporter : public Base
{
protected:
	IImporter() = default;
	virtual ~IImporter() = default;
	virtual EResult Initialize(void* arg = nullptr) { return EResult::Success; }
public:
	virtual void Free() override { Base::Free(); }

public:
    virtual EResult Import(const filesystem::path& sourcePath, const filesystem::path& destDir = {}, void* arg = nullptr) BAM_PURE;
    virtual EResult CreateJob(const filesystem::path& sourcePath, const filesystem::path& destDir, void* arg, std::unique_ptr<ImportJob>& outJob) { return EResult::NotImplemented; }

#pragma region Member Variable
private:

#pragma endregion
};
END