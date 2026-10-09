#pragma once

#include "Engine_Includes.h"
#include "Archive.h"

BEGIN(Engine)
class ISerializable
{
public:
	virtual ~ISerializable() = default;

	virtual void Serialize(class Archive& ar) {}
	virtual void Deserialize(class Archive& ar) {}
};
END