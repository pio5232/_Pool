#pragma once

#include <Windows.h>
#include <DbgHelp.h>

namespace jh_utility
{
	class CrashDump 
	{
	public:
		CrashDump();
		static void Crash();
		static LONG WINAPI MyExceptionFilter(__in PEXCEPTION_POINTERS pExceptionPointer);
		static void SetHandlerDump();
		static void MyInvalidParameterHandler(const wchar_t* expression, const wchar_t* function, const wchar_t* file, unsigned int line, uintptr_t pReserved);
		static int _custom_Report_hook(int ireposttype, char* message, int* returnvalue);
		static void MyPurecallHandler();
	private:
		static long _DumpCount;
	};
}
// #endif
