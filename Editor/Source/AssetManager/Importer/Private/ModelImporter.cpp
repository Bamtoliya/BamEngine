#pragma once

#include "Model.h"
#include "ModelImporter.h"

BEGIN(Editor)
EResult ModelImporter::Import(const filesystem::path& sourcePath, const filesystem::path& destDir)
{
	// Implementation for importing a model
	return EResult::Success;
}
END