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

	inline const char* ResultToString(EResult result)
	{
		switch (result)
		{
		case EResult::Success:				return "Success";
		case EResult::Fail:					return "Fail";
		case EResult::InvalidArgument:		return "Invalid Argument";
		case EResult::NotImplemented:		return "Not Implemented";
		case EResult::OutOfMemory:			return "Out Of Memory";
		case EResult::FileNotFound:			return "File Not Found";
		case EResult::AlreadyInitialized:	return "Already Initialized";
		case EResult::AccessDenied:			return "Access Denied";
		default:							return "Unknown Error";
		}
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