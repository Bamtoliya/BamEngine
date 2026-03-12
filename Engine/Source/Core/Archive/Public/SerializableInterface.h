#pragma once

#include "Engine_Includes.h"
#include "Archive.h"
#include "SerializationHelper.h"

BEGIN(Engine)
class SerializableInterface
{
public:
	virtual ~SerializableInterface() = default;

	virtual void Serialize(class Archive& ar) {}
	virtual void Deserialize(class Archive& ar) {}
};
END