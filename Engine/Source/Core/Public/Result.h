#pragma once

#include "Types.h"

namespace Engine
{
	enum class EResult : uint8
	{
		Success = 0,
		Fail,
		InvalidArgument,
		NotImplemented,
		OutOfMemory,
		FileNotFound,
		AlreadyInitialized,
		AccessDenied
	};

	inline bool IsSuccess(EResult result)
	{
		return result == EResult::Success;
	}

	inline bool IsFailure(EResult result)
	{
		return result != EResult::Success;
	}

	struct ActionResult
	{
		EResult Result;
		std::string Message;
		ActionResult(EResult result = EResult::Success, const std::string& message = "")
			: Result(result), Message(message)
		{
		}
	};
}