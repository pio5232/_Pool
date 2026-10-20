#include "pch.h"

#include <sstream>
#include <iomanip>
#include <utility>
#include <string>
#include <strsafe.h>
#include "Profiler.h"


using namespace jh_utility;

thread_local jh_utility::ThreadProfileData* tls_pProfiler = nullptr;

jh_utility::ProfileSample::ProfileSample()
{
	Initialize();
}

void jh_utility::ProfileSample::Initialize()
{
	memset(m_wszSampleName, NULL, sizeof(m_wszSampleName));
	m_llStartTime.QuadPart = 0;
	m_ullTotalTime = 0;
	m_ullMinTime[0] = m_ullMinTime[1] = ULLONG_MAX;
	m_ullMaxTime[0] = m_ullMaxTime[1] = 0;
	m_ullCallCount = 0;

	m_dwThreadId = ~(0);
	m_bUseFlag = false;
}

jh_utility::ThreadProfileData::ThreadProfileData() : m_samples{}
{
}

jh_utility::ThreadProfileData::~ThreadProfileData()
{
}


void jh_utility::ThreadProfileData::Start(const WCHAR* tag)
{
	for (int i = 0; i < MAX_SAMPLE_COUNT; i++)
	{
		if (false == m_samples[i].m_bUseFlag) // 첫 사용 체크
		{
			m_samples[i].m_bUseFlag = true;

			StringCchCopy(m_samples[i].m_wszSampleName, ARRAY_SIZE(m_samples[i].m_wszSampleName), tag);

			m_samples[i].m_ullTotalTime = 0;

			m_samples[i].m_ullMinTime[0] = m_samples[i].m_ullMinTime[1] = ULLONG_MAX;
			m_samples[i].m_ullMaxTime[0] = m_samples[i].m_ullMaxTime[1] = 0;

			m_samples[i].m_ullCallCount = 0;

			m_samples[i].m_dwThreadId = GetCurrentThreadId();

			QueryPerformanceCounter(&m_samples[i].m_llStartTime);

			break;
		}

		if (wcscmp(m_samples[i].m_wszSampleName, tag) == 0) // 동일한 태그인지 체크
		{
			QueryPerformanceCounter(&m_samples[i].m_llStartTime);

			break;
		}

	}
}

void jh_utility::ThreadProfileData::Stop(const WCHAR* tag)
{
	LARGE_INTEGER endTime;

	QueryPerformanceCounter(&endTime);

	for (int i = 0; i < MAX_SAMPLE_COUNT; i++)
	{
		if (false == m_samples[i].m_bUseFlag)
			break;

		if (wcscmp(m_samples[i].m_wszSampleName, tag) == 0)
		{
			// 초
			LONGLONG elapsedTime = (endTime.QuadPart - m_samples[i].m_llStartTime.QuadPart);

			if (elapsedTime < m_samples[i].m_ullMinTime[0]) // 뒤 1st 
			{
				m_samples[i].m_ullMinTime[1] = m_samples[i].m_ullMinTime[0];
				m_samples[i].m_ullMinTime[0] = elapsedTime;
			}
			else if (elapsedTime < m_samples[i].m_ullMinTime[1]) // 뒤 2st
			{
				m_samples[i].m_ullMinTime[1] = elapsedTime;
			}

			if (elapsedTime > m_samples[i].m_ullMaxTime[0]) // 앞 1st
			{
				m_samples[i].m_ullMaxTime[1] = m_samples[i].m_ullMaxTime[0];
				m_samples[i].m_ullMaxTime[0] = elapsedTime;
			}
			else if (elapsedTime > m_samples[i].m_ullMaxTime[1]) // 앞 2st
			{
				m_samples[i].m_ullMaxTime[1] = elapsedTime;
			}

			m_samples[i].m_ullTotalTime += elapsedTime;
			m_samples[i].m_ullCallCount++;

			break;
		}
	}
}

void jh_utility::ThreadProfileData::Reset()
{
	for (int i = 0; i < MAX_SAMPLE_COUNT; i++)
	{
		m_samples[i].Initialize();
	}
}

/// +-------------------+
///	|	Profiler	|
///	+-------------------+
/// 
void jh_utility::Profiler::Start(const WCHAR* funcName)
{
	if (nullptr == tls_pProfiler)
	{
		tls_pProfiler = new ThreadProfileData();

		DWORD currentThreadId = GetCurrentThreadId();
		// 스레드가 재사용된 경우 기존에 등록된 스레드ID의 동적할당 데이터 해제
		{
			SRWLockGuard lockGuard(&m_lock);

			auto it = m_profilerMap.find(currentThreadId);
			if (it != m_profilerMap.end())
			{
				delete it->second;
			}

			// 없었던 경우는 등록, 있었던 경우는 변경
			m_profilerMap[currentThreadId] = tls_pProfiler;
		}
	}

	tls_pProfiler->Start(funcName);
}

void jh_utility::Profiler::Stop(const WCHAR* funcName)
{
	if (nullptr == tls_pProfiler)
		CrashDump::Crash();

	tls_pProfiler->Stop(funcName);
}

jh_utility::Profiler::Profiler()
{
	InitializeSRWLock(&m_lock);

	_wsetlocale(LC_ALL, L"korean");

	QueryPerformanceFrequency(&m_llQueryPerformanceFrequency);
}
jh_utility::Profiler::~Profiler()
{
	for (auto& pair : m_profilerMap)
	{
		delete pair.second;
	}

	m_profilerMap.clear();

}

void jh_utility::Profiler::ProfileDataOutText(const WCHAR* fileName)
{
	// THREAD NAME AVERAGE MIN MAX CALL
	SRWLockGuard lockGuard(&m_lock);

	FILE* file = nullptr;

	errno_t wfOpenError = _wfopen_s(&file, fileName, L"w, ccs=UNICODE");

	if (0 != wfOpenError || nullptr == file)
	{
		return;
	}

	// 우측정렬 
	// THREAD \t | NAME \t | ... 의 형태로 생성
	fwprintf_s(file, L"%15s | %70s | %17s | %17s | %17s | %15s |\n", L"THREADID", L"FUNC", L"AVERAGE", L"MIN", L"MAX", L"CALL");;
	fwprintf_s(file, L"-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n\n");

	for (auto& pair : m_profilerMap)
	{
		for (int i = 0; i < ARRAY_SIZE(pair.second->m_samples); i++)
		{
			ProfileSample& sample = pair.second->m_samples[i];

			if (false == sample.m_bUseFlag)
				break;

			double minTime = sample.m_ullCallCount <= 2 ? (double)sample.m_ullMinTime[0] : (double)sample.m_ullMinTime[1];
			double maxTime = sample.m_ullCallCount <= 2 ? (double)sample.m_ullMaxTime[0] : (double)sample.m_ullMaxTime[1];
			// min, max는 2번째꺼 출력.
			fwprintf_s(file, L"%15u | %70s | %15.6lf㎲ | %15.6lf㎲ | %15.6lf㎲ | %15u |\n", sample.m_dwThreadId, sample.m_wszSampleName,
				(double)sample.m_ullTotalTime / sample.m_ullCallCount * MICRO_SCALE(m_llQueryPerformanceFrequency.QuadPart), // AVERAGE
				minTime * MICRO_SCALE(m_llQueryPerformanceFrequency.QuadPart), // MIN
				maxTime * MICRO_SCALE(m_llQueryPerformanceFrequency.QuadPart), // MAX
				sample.m_ullCallCount); // m_ullCallCount
		}
		fwprintf_s(file, L"\n");
	}

	fwprintf_s(file, L"-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");



	fclose(file);
	return;
}

void jh_utility::Profiler::DataReset()
{
	SRWLockGuard lockGuard(&m_lock);

	for (auto& pair : m_profilerMap)
	{
		pair.second->Reset();
	}
}
