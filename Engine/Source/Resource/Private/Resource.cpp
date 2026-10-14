#pragma once

#include "Resource.h"

archive::ArchiveResult Resource::WriteArchive(archive::IArchive&) const
{
	return { archive::ArchiveErrorCode::UnsupportedType, "Resource.WriteArchive" };
}

archive::ArchiveResult Resource::ReadArchive(const archive::IArchive&)
{
	return { archive::ArchiveErrorCode::UnsupportedType, "Resource.ReadArchive" };
}