#pragma once
#include <Windows.h>

#include <map>

#define MAX_SAMPLE_COUNT 50
#define SAMPLE_NAME_LEN 64

#define PROF_WIDE2(x) L##x
#define PROF_WIDE1(x) PROF_WIDE2(x)
#define PROF_WFUNC PROF_WIDE1(__FUNCTION__)

#ifdef _DEBUG
#define PRO_START_AUTO_FUNC jh_utility::AutoProfiler ra(PROF_WFUNC)
#else
#define PRO_START_AUTO_FUNC jh_utility::AutoProfiler ra(PROF_WFUNC)
#endif

#define PRO_START(x) jh_utility::Profiler::GetInstance().Start(L##x)
#define PRO_END(x) jh_utility::Profiler::GetInstance().Stop(L##x)

#define PRO_RESET jh_utility::Profiler::GetInstance().DataReset()
#define PRO_SAVE(FILE_NAME) jh_utility::Profiler::GetInstance().ProfileDataOutText(L##FILE_NAME) 
#define ARRAY_SIZE(ARR) (sizeof(ARR) / sizeof(ARR[0]))

#define MILLI_SCALE(qpfrequency) ((double)1'000 / (double)qpfrequency)
#define MICRO_SCALE(qpfrequency) ((double)1'000'000 / (double)qpfrequency)

namespace jh_utility
{
	struct ProfileSample
	{
		ProfileSample();
		void Initialize();

		WCHAR			m_wszSampleName[SAMPLE_NAME_LEN]; // 태그 이름

		LARGE_INTEGER	m_llStartTime; // 시작 시간

		ULONGLONG		m_ullTotalTime; // 전체 사용 시간
		ULONGLONG		m_ullMinTime[2]; // 최소 사용 시간
		ULONGLONG		m_ullMaxTime[2]; // 최대 사용 시간.
		ULONGLONG		m_ullCallCount; // 호출 횟수 

		DWORD			m_dwThreadId;

		bool			m_bUseFlag; // 프로파일의 사용 여부
	};

	struct ThreadProfileData
	{
	public:
		ThreadProfileData();
		~ThreadProfileData();

		// Sample 등록은 최대 50개
		void Start(const WCHAR* tag);
		void Stop(const WCHAR* tag);

		void Reset();

		ProfileSample m_samples[MAX_SAMPLE_COUNT];
	};


	/*-----------------------
			Profiler
	-----------------------*/

	class Profiler
	{
	public:

		static Profiler& GetInstance()
		{
			static Profiler instance;

			return instance;
		}

		void Start(const WCHAR* funcName);
		void Stop(const WCHAR* funcName);

		void ProfileDataOutText(const WCHAR* fileName);
		void DataReset();

	private:

		Profiler();
		~Profiler();

		Profiler(const Profiler&) = delete;
		Profiler& operator=(const Profiler&) = delete;

		LARGE_INTEGER						m_llQueryPerformanceFrequency;
		SRWLOCK								m_lock;

		// 전체 종료를 위한 동적할당 포인터 모음
		std::map<DWORD, ThreadProfileData*> m_profilerMap;

		// 완전히 동기화하지 않고 그 순간의 데이터를 출력한다.
		// 오차가 있을 수 있다.

	};

	class AutoProfiler
	{
	public:
		AutoProfiler(const WCHAR* functionName) : currentFunctionName{ functionName }
		{
			jh_utility::Profiler::GetInstance().Start(currentFunctionName);
		}
		~AutoProfiler() {

			jh_utility::Profiler::GetInstance().Stop(currentFunctionName);
			currentFunctionName = nullptr;
		}

	private:
		const WCHAR* currentFunctionName = nullptr;
	};
}
extern thread_local jh_utility::ThreadProfileData* tls_pProfiler;