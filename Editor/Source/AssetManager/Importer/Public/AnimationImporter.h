#pragma once
#include "ImporterInterface.h"
BEGIN(Editor)
class AnimationImporter : public ImporterInterface
{
#pragma region Constructor&Destructor
private:
    AnimationImporter() = default;
    virtual ~AnimationImporter() = default;
    virtual EResult Initialize(void* arg = nullptr) { return EResult::Success; }
public:
	static AnimationImporter* Create() { return new AnimationImporter(); }
    virtual void Free() override { ImporterInterface::Free(); }
#pragma endregion

public:
    virtual EResult Import(const filesystem::path& sourcePath, const filesystem::path& destDir) override;

#pragma region Member Variables
private:
#pragma endregion
};
END
