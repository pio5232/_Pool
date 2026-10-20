#include "pch.h"
#include "CrashDump.h"
#include <crtdbg.h>
#include <iostream>

using namespace jh_utility;
#pragma comment(lib, "DbgHelp.lib")

#define DUMP_EXPORT


	long CrashDump::_DumpCount = 0;

	CrashDump::CrashDump()
	{
		_DumpCount = 0;

		_invalid_parameter_handler oldHandler, newHandler;
		newHandler = MyInvalidParameterHandler;

		oldHandler = _set_invalid_parameter_handler(newHandler); // invalid parameter 에러가 났을 때 내가 등록한 핸들러로 처리하도록 한다.

		_CrtSetReportMode(_CRT_WARN, 0); // WARNING 메시지 표시 중단
		_CrtSetReportMode(_CRT_ASSERT, 0); // ASSERT 메시지 표시 중단
		_CrtSetReportMode(_CRT_ERROR, 0); // ERROR 메시지 표시 중단

		_CrtSetReportHook(_custom_Report_hook);

		_set_purecall_handler(MyPurecallHandler); // purecall error가 났을 때 내가 등록한 핸들러로 처리하도록 한다.

		SetHandlerDump();
	}

	void CrashDump::Crash()
	{
		DebugBreak();
	}

	LONG WINAPI CrashDump::MyExceptionFilter(__in PEXCEPTION_POINTERS pExceptionPointer)
	{
		int iWorkingMemory = 0;
		SYSTEMTIME stNowTime;

		long DumpCount = InterlockedIncrement(&_DumpCount);

		WCHAR filename[MAX_PATH];

		GetLocalTime(&stNowTime);

		// 우리는 덤프 카운트가 1인 것만 확인하도록 한다.
		wsprintf(filename, L"Dump_%d%02d%02d_%02d_%02d_%02d_%d.dmp", stNowTime.wYear, stNowTime.wMonth, stNowTime.wDay, stNowTime.wHour, stNowTime.wMinute, stNowTime.wSecond, DumpCount);

		wprintf(L"Crash Error Handling...\n");

		HANDLE hDumpFile = CreateFile(filename, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hDumpFile != INVALID_HANDLE_VALUE)
		{
			_MINIDUMP_EXCEPTION_INFORMATION MinidumpExceptionInformation;

			MinidumpExceptionInformation.ThreadId = GetCurrentThreadId();
			MinidumpExceptionInformation.ExceptionPointers = pExceptionPointer;
			MinidumpExceptionInformation.ClientPointers = true;

			MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hDumpFile, MiniDumpWithFullMemory, &MinidumpExceptionInformation, NULL, NULL);

			CloseHandle(hDumpFile);

			wprintf(L"CrashDump Save Finish !");
		}
		return EXCEPTION_EXECUTE_HANDLER;
	}

	void CrashDump::SetHandlerDump()
	{
		SetUnhandledExceptionFilter(MyExceptionFilter);
	}

	void CrashDump::MyInvalidParameterHandler(const wchar_t* expression, const wchar_t* function, const wchar_t* file, unsigned int line, uintptr_t pReserved)
	{
		Crash();
	}

	int CrashDump::_custom_Report_hook(int ireposttype, char* message, int* returnvalue)
	{
		Crash();
		return true;
	}

	void CrashDump::MyPurecallHandler()
	{
		Crash();
	}