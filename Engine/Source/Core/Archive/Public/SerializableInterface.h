#pragma once

#include "Engine_Includes.h"
#include "Archive.h"
#include "SerializationHelper.h"

BEGIN(Engine)
class SerialzableInterface
{
public:
	virtual ~SerialzableInterface() = default;

	virtual void Serialize(class Archive& ar) {}
	virtual void Deserialize(class Archive& ar) {}
};
END