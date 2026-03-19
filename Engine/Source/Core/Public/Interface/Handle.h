#pragma once

#include "Engine_API.h"
#include "Macro.h"
#include "Types.h"

BEGIN(Engine)
struct ENGINE_API Handle
{
public:
	static constexpr uint64 INVALID_ID = 0xFFFFFFFFFFFFFFFF;

	Handle() : ID(INVALID_ID) {}
	explicit Handle(uint64 id) : ID(id) {}

	Handle(uint32 index, uint32 generation)
	{
		ID = (static_cast<uint64>(generation) << 32) | index;
	}
public:
	bool IsValid() const { return ID != INVALID_ID; }
	operator bool() const { return IsValid(); }
	uint64 GetID() const { return ID; }
	uint32 GetIndex() const { return static_cast<uint32>(ID & 0xFFFFFFFF); }
	uint32 GetGeneration() const { return static_cast<uint32>((ID >> 32) & 0xFFFFFFFF); }
public:
	bool operator==(const Handle& other) const { return ID == other.ID; }
	bool operator!=(const Handle& other) const { return ID != other.ID; }
	bool operator<(const Handle& other) const { return ID < other.ID; }
protected:
	uint64 ID = INVALID_ID;
};
END

BEGIN(std)
template<>
struct hash<Engine::Handle>
{
	size_t operator()(const Engine::Handle& handle) const
	{
		return std::hash<uint64>()(handle.GetID());
	}
};
END