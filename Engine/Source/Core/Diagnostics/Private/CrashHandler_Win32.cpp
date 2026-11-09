#include "CrashHandler.h"
#include "Logger.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>
#include <chrono>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <vector>

#pragma comment(lib, "dbghelp.lib")

BEGIN(Engine)

namespace
{
	std::filesystem::path s_DumpDirectory = L"Logs/Crashes";
	std::filesystem::path s_LastDumpPath;
	std::filesystem::path s_LastReportPath;

	std::string s_ActiveScenePath = "None";
	std::unordered_map<std::string, std::string> s_CustomContext;
	std::mutex s_ContextMutex;

	CrashHandler::EmergencyCallback s_EmergencyCallback = nullptr;

	LPTOP_LEVEL_EXCEPTION_FILTER s_PreviousFilter = nullptr;
	std::terminate_handler s_PreviousTerminate = nullptr;
	_purecall_handler s_PreviousPurecall = nullptr;
	_invalid_parameter_handler s_PreviousInvalidParam = nullptr;

	std::atomic<bool> s_IsHandlingCrash{ false };

	std::string GetTimestampString()
	{
		const auto now = std::chrono::system_clock::now();
		const auto time = std::chrono::system_clock::to_time_t(now);
		std::tm tm{};
		::localtime_s(&tm, &time);

		char buf[64];
		std::snprintf(buf, sizeof(buf), "%04d%02d%02d_%02d%02d%02d",
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec);
		return std::string(buf);
	}

	const char* GetExceptionCodeString(DWORD code)
	{
		switch (code)
		{
		case EXCEPTION_ACCESS_VIOLATION: return "EXCEPTION_ACCESS_VIOLATION (0xC0000005)";
		case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "EXCEPTION_ARRAY_BOUNDS_EXCEEDED (0xC000008C)";
		case EXCEPTION_BREAKPOINT: return "EXCEPTION_BREAKPOINT (0x80000003)";
		case EXCEPTION_DATATYPE_MISALIGNMENT: return "EXCEPTION_DATATYPE_MISALIGNMENT (0x80000002)";
		case EXCEPTION_FLT_DENORMAL_OPERAND: return "EXCEPTION_FLT_DENORMAL_OPERAND (0xC000008D)";
		case EXCEPTION_FLT_DIVIDE_BY_ZERO: return "EXCEPTION_FLT_DIVIDE_BY_ZERO (0xC000008E)";
		case EXCEPTION_FLT_INEXACT_RESULT: return "EXCEPTION_FLT_INEXACT_RESULT (0xC000008F)";
		case EXCEPTION_FLT_INVALID_OPERATION: return "EXCEPTION_FLT_INVALID_OPERATION (0xC0000090)";
		case EXCEPTION_FLT_OVERFLOW: return "EXCEPTION_FLT_OVERFLOW (0xC0000091)";
		case EXCEPTION_FLT_STACK_CHECK: return "EXCEPTION_FLT_STACK_CHECK (0xC0000092)";
		case EXCEPTION_FLT_UNDERFLOW: return "EXCEPTION_FLT_UNDERFLOW (0xC0000093)";
		case EXCEPTION_ILLEGAL_INSTRUCTION: return "EXCEPTION_ILLEGAL_INSTRUCTION (0xC000001D)";
		case EXCEPTION_IN_PAGE_ERROR: return "EXCEPTION_IN_PAGE_ERROR (0xC0000006)";
		case EXCEPTION_INT_DIVIDE_BY_ZERO: return "EXCEPTION_INT_DIVIDE_BY_ZERO (0xC0000094)";
		case EXCEPTION_INT_OVERFLOW: return "EXCEPTION_INT_OVERFLOW (0xC0000095)";
		case EXCEPTION_INVALID_DISPOSITION: return "EXCEPTION_INVALID_DISPOSITION (0xC0000026)";
		case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "EXCEPTION_NONCONTINUABLE_EXCEPTION (0xC0000025)";
		case EXCEPTION_PRIV_INSTRUCTION: return "EXCEPTION_PRIV_INSTRUCTION (0xC0000096)";
		case EXCEPTION_SINGLE_STEP: return "EXCEPTION_SINGLE_STEP (0x80000004)";
		case EXCEPTION_STACK_OVERFLOW: return "EXCEPTION_STACK_OVERFLOW (0xC00000FD)";
		default: return "UNKNOWN_EXCEPTION";
		}
	}

	std::vector<std::string> CaptureCallstack(CONTEXT* pContext)
	{
		std::vector<std::string> callstack;
		const HANDLE hProcess = GetCurrentProcess();
		const HANDLE hThread = GetCurrentThread();

		SymInitialize(hProcess, nullptr, TRUE);
		SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);

		CONTEXT ctx;
		if (pContext)
		{
			ctx = *pContext;
		}
		else
		{
			RtlCaptureContext(&ctx);
		}

		STACKFRAME64 stackFrame{};
		DWORD machineType = IMAGE_FILE_MACHINE_AMD64;
		stackFrame.AddrPC.Offset = ctx.Rip;
		stackFrame.AddrPC.Mode = AddrModeFlat;
		stackFrame.AddrFrame.Offset = ctx.Rbp;
		stackFrame.AddrFrame.Mode = AddrModeFlat;
		stackFrame.AddrStack.Offset = ctx.Rsp;
		stackFrame.AddrStack.Mode = AddrModeFlat;

		for (int frame = 0; frame < 32; ++frame)
		{
			if (!StackWalk64(machineType, hProcess, hThread, &stackFrame, &ctx,
				nullptr, SymFunctionTableAccess64, SymGetModuleBase64, nullptr))
			{
				break;
			}

			if (stackFrame.AddrPC.Offset == 0)
				break;

			alignas(SYMBOL_INFO) char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)]{};
			PSYMBOL_INFO pSymbol = reinterpret_cast<PSYMBOL_INFO>(buffer);
			pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
			pSymbol->MaxNameLen = MAX_SYM_NAME;

			DWORD64 displacement = 0;
			std::string symbolDesc;

			if (SymFromAddr(hProcess, stackFrame.AddrPC.Offset, &displacement, pSymbol))
			{
				IMAGEHLP_LINE64 lineInfo{};
				lineInfo.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
				DWORD lineDisplacement = 0;

				if (SymGetLineFromAddr64(hProcess, stackFrame.AddrPC.Offset, &lineDisplacement, &lineInfo))
				{
					char lineBuf[512];
					std::snprintf(lineBuf, sizeof(lineBuf), "%s() [%s:%lu]",
						pSymbol->Name, lineInfo.FileName, lineInfo.LineNumber);
					symbolDesc = lineBuf;
				}
				else
				{
					char symBuf[256];
					std::snprintf(symBuf, sizeof(symBuf), "%s + 0x%llX", pSymbol->Name, displacement);
					symbolDesc = symBuf;
				}
			}
			else
			{
				char addrBuf[64];
				std::snprintf(addrBuf, sizeof(addrBuf), "0x%016llX", stackFrame.AddrPC.Offset);
				symbolDesc = addrBuf;
			}

			callstack.push_back(symbolDesc);
		}

		SymCleanup(hProcess);
		return callstack;
	}

	void GenerateCrashReport(const std::filesystem::path& reportPath,
		DWORD exceptionCode, PVOID exceptionAddress, CONTEXT* pContext)
	{
		std::error_code ec;
		std::filesystem::create_directories(reportPath.parent_path(), ec);

		std::ofstream reportFile(reportPath);
		if (!reportFile.is_open())
			return;

		const auto callstack = CaptureCallstack(pContext);

		MEMORYSTATUSEX memInfo{};
		memInfo.dwLength = sizeof(MEMORYSTATUSEX);
		GlobalMemoryStatusEx(&memInfo);

		reportFile << "{\n";
		reportFile << "  \"CrashReportVersion\": \"1.0\",\n";
		reportFile << "  \"Timestamp\": \"" << GetTimestampString() << "\",\n";
#if defined(BAM_EDITOR) && BAM_EDITOR
		reportFile << "  \"Target\": \"Editor\",\n";
#else
		reportFile << "  \"Target\": \"Client\",\n";
#endif
		reportFile << "  \"Exception\": {\n";
		reportFile << "    \"Code\": \"0x" << std::hex << exceptionCode << std::dec << "\",\n";
		reportFile << "    \"Name\": \"" << GetExceptionCodeString(exceptionCode) << "\",\n";
		reportFile << "    \"Address\": \"0x" << std::hex << reinterpret_cast<uintptr_t>(exceptionAddress) << std::dec << "\"\n";
		reportFile << "  },\n";

		reportFile << "  \"Environment\": {\n";
		reportFile << "    \"TotalPhysicalMemoryMB\": " << (memInfo.ullTotalPhys / (1024 * 1024)) << ",\n";
		reportFile << "    \"AvailPhysicalMemoryMB\": " << (memInfo.ullAvailPhys / (1024 * 1024)) << "\n";
		reportFile << "  },\n";

		reportFile << "  \"Context\": {\n";
		reportFile << "    \"ActiveScene\": \"" << s_ActiveScenePath << "\"";
		{
			std::lock_guard lock(s_ContextMutex);
			for (const auto& [k, v] : s_CustomContext)
			{
				reportFile << ",\n    \"" << k << "\": \"" << v << "\"";
			}
		}
		reportFile << "\n  },\n";

		reportFile << "  \"Callstack\": [\n";
		for (size_t i = 0; i < callstack.size(); ++i)
		{
			reportFile << "    \"" << callstack[i] << "\"";
			if (i + 1 < callstack.size()) reportFile << ",";
			reportFile << "\n";
		}
		reportFile << "  ]\n";
		reportFile << "}\n";

		reportFile.flush();
	}

	LONG WINAPI BamUnhandledExceptionFilter(EXCEPTION_POINTERS* pExceptionPointers)
	{
		// 중복 크래시 및 재귀 진입 차단
		if (s_IsHandlingCrash.exchange(true))
		{
			return EXCEPTION_EXECUTE_HANDLER;
		}

		// 1. 긴급 스냅샷 및 상태 갱신 콜백 즉시 호출 (Tier 2 Recovery)
		if (s_EmergencyCallback)
		{
			s_EmergencyCallback();
		}

		// 2. 버퍼링된 로그 즉각 디스크 플러시
		Logger::Flush();

		// 3. 미니덤프 및 리포트 디렉터리 준비
		std::error_code ec;
		std::filesystem::create_directories(s_DumpDirectory, ec);

		const std::string timestamp = GetTimestampString();
		const std::filesystem::path dumpPath = s_DumpDirectory / ("BamEngine_Crash_" + timestamp + ".dmp");
		const std::filesystem::path reportPath = s_DumpDirectory / ("BamEngine_Crash_" + timestamp + ".crash.json");

		s_LastDumpPath = dumpPath;
		s_LastReportPath = reportPath;

		// 4. MiniDumpWriteDump 생성
		const HANDLE hDumpFile = CreateFileW(dumpPath.c_str(), GENERIC_WRITE, 0, nullptr,
			CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

		if (hDumpFile != INVALID_HANDLE_VALUE)
		{
			MINIDUMP_EXCEPTION_INFORMATION mei{};
			mei.ThreadId = GetCurrentThreadId();
			mei.ExceptionPointers = pExceptionPointers;
			mei.ClientPointers = FALSE;

#if defined(BAM_EDITOR) && BAM_EDITOR
			const MINIDUMP_TYPE dumpType = static_cast<MINIDUMP_TYPE>(
				MiniDumpNormal |
				MiniDumpWithDataSegs |
				MiniDumpWithUnloadedModules |
				MiniDumpWithProcessThreadData |
				MiniDumpWithThreadInfo
			);
#else
			const MINIDUMP_TYPE dumpType = static_cast<MINIDUMP_TYPE>(
				MiniDumpNormal |
				MiniDumpWithThreadInfo
			);
#endif

			MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),
				hDumpFile, dumpType, pExceptionPointers ? &mei : nullptr, nullptr, nullptr);

			CloseHandle(hDumpFile);
		}

		// 5. 구조화된 크래시 리포트 생성
		const DWORD code = pExceptionPointers && pExceptionPointers->ExceptionRecord ?
			pExceptionPointers->ExceptionRecord->ExceptionCode : 0;
		PVOID addr = pExceptionPointers && pExceptionPointers->ExceptionRecord ?
			pExceptionPointers->ExceptionRecord->ExceptionAddress : nullptr;
		CONTEXT* ctx = pExceptionPointers ? pExceptionPointers->ContextRecord : nullptr;

		GenerateCrashReport(reportPath, code, addr, ctx);

		// 6. 안내 메시지박스
		char msg[512];
		std::snprintf(msg, sizeof(msg),
			"BamEngine has encountered a fatal error and must close.\n\n"
			"Exception: %s\n"
			"Crash Dump: %s\n"
			"Crash Report: %s\n\n"
			"Your unsaved work has been saved to the recovery snapshot directory.",
			GetExceptionCodeString(code),
			dumpPath.string().c_str(),
			reportPath.string().c_str());

		MessageBoxA(nullptr, msg, "BamEngine Crash Diagnostics", MB_OK | MB_ICONERROR | MB_TASKMODAL);

		return EXCEPTION_EXECUTE_HANDLER;
	}

	void BamTerminateHandler()
	{
		BamUnhandledExceptionFilter(nullptr);
		std::abort();
	}

	void BamPurecallHandler()
	{
		BamUnhandledExceptionFilter(nullptr);
		std::abort();
	}

	void BamInvalidParameterHandler(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, uintptr_t)
	{
		BamUnhandledExceptionFilter(nullptr);
		std::abort();
	}
}

void CrashHandler::Initialize(const std::filesystem::path& crashDumpDirectory)
{
	s_DumpDirectory = crashDumpDirectory;
	std::error_code ec;
	std::filesystem::create_directories(s_DumpDirectory, ec);

	s_PreviousFilter = SetUnhandledExceptionFilter(&BamUnhandledExceptionFilter);
	s_PreviousTerminate = std::set_terminate(&BamTerminateHandler);
	s_PreviousPurecall = _set_purecall_handler(&BamPurecallHandler);
	s_PreviousInvalidParam = _set_invalid_parameter_handler(&BamInvalidParameterHandler);
}

void CrashHandler::Shutdown()
{
	if (s_PreviousFilter) SetUnhandledExceptionFilter(s_PreviousFilter);
	if (s_PreviousTerminate) std::set_terminate(s_PreviousTerminate);
	if (s_PreviousPurecall) _set_purecall_handler(s_PreviousPurecall);
	if (s_PreviousInvalidParam) _set_invalid_parameter_handler(s_PreviousInvalidParam);
}

void CrashHandler::SetActiveScenePath(const std::string& scenePath)
{
	s_ActiveScenePath = scenePath;
}

void CrashHandler::SetCustomContext(const std::string& key, const std::string& value)
{
	std::lock_guard lock(s_ContextMutex);
	s_CustomContext[key] = value;
}

void CrashHandler::RegisterEmergencyCallback(EmergencyCallback callback)
{
	s_EmergencyCallback = callback;
}

const std::filesystem::path& CrashHandler::GetCrashDumpDirectory()
{
	return s_DumpDirectory;
}

const std::filesystem::path& CrashHandler::GetLastDumpPath()
{
	return s_LastDumpPath;
}

const std::filesystem::path& CrashHandler::GetLastReportPath()
{
	return s_LastReportPath;
}

void CrashHandler::TriggerCrashForTesting()
{
	volatile int* ptr = nullptr;
	*ptr = 42;
}

END
