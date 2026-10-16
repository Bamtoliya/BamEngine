#include "Resource.h"
#include <reflection_archive/ReflectionArchiveAdapter.h>

archive::ArchiveResult Resource::WriteArchive(archive::IArchive& ar) const
{
	return archive::ArchiveResult{ archive::ArchiveErrorCode::UnsupportedType, "Resource.WriteArchive" };
}

archive::ArchiveResult Resource::ReadArchive(const archive::IArchive& ar)
{
	return archive::ArchiveResult{ archive::ArchiveErrorCode::UnsupportedType, "Resource.ReadArchive" };
}