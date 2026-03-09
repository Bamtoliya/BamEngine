#pragma once
#include "ImporterInterface.h"
BEGIN(Editor)
class AudioImporter : public ImporterInterface
{
#pragma region Constructor&Destructor
private:
    AudioImporter() = default;
    virtual ~AudioImporter() = default;
    virtual EResult Initialize(void* arg = nullptr) { return EResult::Success; }
public:
	static AudioImporter* Create() { return new AudioImporter(); }
    virtual void Free() override { Base::Free(); }
#pragma endregion

#pragma region Member Variables
private:
#pragma endregion
};
END
