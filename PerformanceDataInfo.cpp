#include "pch.h"
#include "PerformanceDataInfo.h"
#include "PdhMsg.h"

#pragma comment(lib, "pdh.lib")

// 오브젝트 이름(인스턴스 이름)%카운터 이름

const jh::PerformanceDataInfo::CounterRegInfo jh::PerformanceDataInfo::kCounterRegInfo[jh::PerformanceDataInfo::NUM_OF_DATA] =
{
	// 프로세스 이름 필요
	{L"\\Process(%s)\\%% Processor Time", PROCESS_NAME, PDH_FMT_DOUBLE},					// 프로세스 cpu 사용률
	{L"\\Process(%s)\\%% User Time", PROCESS_NAME, PDH_FMT_DOUBLE},							// 프로세스 유저모드 cpu 사용률
	{L"\\Process(%s)\\%% Privileged Time", PROCESS_NAME, PDH_FMT_DOUBLE},					// 프로세스 커널모드 cpu 사용률

	{L"\\Process(%s)\\Private Bytes", PROCESS_NAME,PDH_FMT_DOUBLE},// PDH_FMT_LARGE},		// 프로세스가 사용중인 commit 메모리 ( RAM + DISK, 공유 메모리 포함 X)
	{L"\\Process(%s)\\Working Set", PROCESS_NAME, PDH_FMT_DOUBLE},//PDH_FMT_LARGE},			// 프로세스가 사용중인 워킹셋 사용량 ( RAM에 있는 commit 메모리, 공유 메모리 포함 O )

	{L"\\Process(%s)\\Pool Paged Bytes", PROCESS_NAME, PDH_FMT_DOUBLE},//PDH_FMT_LARGE},	// 프로세스 페이징 풀 사용량
	{L"\\Process(%s)\\Pool Nonpaged Bytes", PROCESS_NAME, PDH_FMT_DOUBLE},//PDH_FMT_LARGE},	// 프로세스 논페이징 풀 사용량

	{L"\\Memory\\Pool Paged Bytes", NONE, PDH_FMT_DOUBLE},//PDH_FMT_LARGE},					// 시스템 전체 페이징 풀 사용량
	{L"\\Memory\\Pool Nonpaged Bytes", NONE, PDH_FMT_DOUBLE},//PDH_FMT_LARGE},				// 시스템 전체 논페이징 풀 사용량

	{L"\\Processor(_Total)\\%% Processor Time", NONE, PDH_FMT_DOUBLE},						// 시스템 전체 cpu 사용률 

	// 네트워크 랜카드 인스턴스 이름 필요
	{L"\\Network Interface(%s)\\Bytes Total/sec", NETWORK_INSTANCE_NAME, PDH_FMT_DOUBLE},	// 초당 송신 + 수신 바이트
	{L"\\Network Interface(%s)\\Bytes Sent/sec", NETWORK_INSTANCE_NAME, PDH_FMT_DOUBLE},	// 초당 송신 바이트
	{L"\\Network Interface(%s)\\Bytes Received/sec", NETWORK_INSTANCE_NAME, PDH_FMT_DOUBLE},// 초당 수신 바이트
};

jh::PerformanceDataInfo::PerformanceDataInfo() : m_hQuery{}, m_hCounters{}, m_data{}, m_networkCounters{}
{
	Init();
}

jh::PerformanceDataInfo::~PerformanceDataInfo()
{
	if (nullptr != m_hQuery)
	{
		PdhCloseQuery(m_hQuery);
		m_hQuery = nullptr;
	}
}

double jh::PerformanceDataInfo::GetProcessUserPercentage()
{
	double total = m_data[PROCESS_CPU_USER_USAGE].doubleValue + m_data[PROCESS_CPU_KERNEL_USAGE].doubleValue;
	if (0 == total)
		return 0;

	return m_data[PROCESS_CPU_USER_USAGE].doubleValue / total * 100;
}

double jh::PerformanceDataInfo::GetProcessKernelPercentage()
{
	double total = m_data[PROCESS_CPU_USER_USAGE].doubleValue + m_data[PROCESS_CPU_KERNEL_USAGE].doubleValue;
	if (0 == total)
		return 0;

	return m_data[PROCESS_CPU_KERNEL_USAGE].doubleValue / total * 100;
}

void jh::PerformanceDataInfo::Init()
{
	// 1. 쿼리 생성
	PDH_STATUS pdhStatus;

	pdhStatus = PdhOpenQuery(NULL, NULL, &m_hQuery);

	if (ERROR_SUCCESS != pdhStatus)
	{
		wprintf(L"\nPdhOpenQuery failed.. status : 0x%x\n", pdhStatus);
		m_hQuery = nullptr;
		return;
	}

	// 매개변수로 활용할 데이터 처리
	WCHAR processNameBuffer[MAX_PATH];
	GetCurrentProcessName(processNameBuffer);

	// 카운터 경로를 입력할 패스
	WCHAR fullCounterPathBuffer[PDH_MAX_COUNTER_PATH];

	// 2. 카운터 추가  (네트워크 관련을 제외한 <- 랜카드가 여러개일 수 있기 때문에 여러개의 정보를 합산해서 처리)
	// 쿼리, 카운터 경로 , 사용자 추가할 값, 핸들
	// 네트워크 관련 카운터 등록은 따로 처리.
	for (DWORD i = 0; i < NUM_OF_SINGLE_DATA; i++)
	{
		int writeBytes = -1;
		DWORD argType = kCounterRegInfo[i].m_argType;

		if (false == FillPathBuffer(i, argType, fullCounterPathBuffer, processNameBuffer))
		{
			wprintf(L"\Init fillPathBuffer() failed...  index : %u\n", i);
			return;
		}
		if (false == AddCounter(fullCounterPathBuffer, &m_hCounters[i]))
		{
			wprintf(L"Init AddCounter() Failed.. index : %u\n", i);
			return;
		}
	}

	// 2-2 네트워크 카운터 등록
	if (false == AddNetworkCounter())
	{
		wprintf(L"Init AddNetworkCounter() failed.. exit...\n");
		return;
	}

	// 3. 이전 샘플 계산 위해 한번 실행
	PdhCollectQueryData(m_hQuery);

}

void jh::PerformanceDataInfo::UpdateQuery()
{
	PDH_STATUS pdhStatus = PdhCollectQueryData(m_hQuery);

	if (ERROR_SUCCESS != pdhStatus)
	{
		wprintf(L"UpdateQuery CollectQueryData failed... status 0x%x.\n", pdhStatus);

		return;
	}

	for (DWORD i = 0; i < NUM_OF_SINGLE_DATA; i++)
	{
		PDH_FMT_COUNTERVALUE pdhFmtCounterValue{};

		if (false == GetFormattedCounterValue(i, &pdhFmtCounterValue))
			continue;

		switch (kCounterRegInfo[i].m_dwFormat)
		{
		case PDH_FMT_LONG: m_data[i].longValue = pdhFmtCounterValue.longValue; break;
		case PDH_FMT_DOUBLE: 	m_data[i].doubleValue = pdhFmtCounterValue.doubleValue; break;
		case PDH_FMT_LARGE: m_data[i].largeValue = pdhFmtCounterValue.largeValue; break;
		default:break;
		}
	}

	// 네트워크 카운팅
	for (DWORD i = NETWORK_BYTES_TOTAL_PER_SEC; i < NUM_OF_DATA; i++)
	{
		// 초기화
		m_data[i].largeValue = 0;
	}

	// 각 네트워크 인스턴스마다의 total / send / recv에 대해
	for (NetworkCounter& netCounter : m_networkCounters)
	{
		// 나중에 추가되더라도
		PDH_FMT_COUNTERVALUE pdhFmtCounterValue[NUM_OF_DATA - NETWORK_BYTES_TOTAL_PER_SEC]{};

		// 개수만큼을 자동으로
		for (DWORD i = NETWORK_BYTES_TOTAL_PER_SEC; i < NUM_OF_DATA; i++)
		{
			const DWORD counterValueIdx = i - NETWORK_BYTES_TOTAL_PER_SEC;

			// 값을 구해서 
			if (false == GetFormattedCounterValue(netCounter.m_hCounters[counterValueIdx], kCounterRegInfo[i].m_dwFormat, &pdhFmtCounterValue[counterValueIdx]))
				continue;

			// 모니터링 데이터에 추가하도록 총량을 계산한다.
			m_data[i].doubleValue += pdhFmtCounterValue[counterValueIdx].doubleValue;
		}
	}
}

LONGLONG jh::PerformanceDataInfo::GetLargeData(DWORD performanceDataEnum)
{
	if (PDH_FMT_LARGE != kCounterRegInfo[performanceDataEnum].m_dwFormat)
		return 0;

	return m_data[performanceDataEnum].largeValue;
}

double jh::PerformanceDataInfo::GetDoubleData(DWORD performanceDataEnum)
{
	if (PDH_FMT_DOUBLE != kCounterRegInfo[performanceDataEnum].m_dwFormat)
		return 0;

	return m_data[performanceDataEnum].doubleValue;
}

LONG jh::PerformanceDataInfo::GetLongData(DWORD performanceDataEnum)
{
	if (PDH_FMT_LONG != kCounterRegInfo[performanceDataEnum].m_dwFormat)
		return 0;

	return m_data[performanceDataEnum].longValue;
}

void jh::PerformanceDataInfo::Print()
{
	UpdateQuery();

	wprintf(L"+--------------------------------------------------------------------------------------------------------------+\n");
	wprintf(L"|%-40ls : %17.3lf%50ls|\n", L" SYSTEM_CPU_USAGE_TOTAL", GetDoubleData(jh::PerformanceDataInfo::SYSTEM_CPU_USAGE_TOTAL),L" ");
	wprintf(L"|%-110ls|\n", L" ");

	wprintf(L"|%-40ls : %17.3lf%%%49ls|\n", L" PROCESS_CPU_USAGE", GetDoubleData(jh::PerformanceDataInfo::PROCESS_CPU_USAGE), L" ");
	wprintf(L"|%-40ls : %17.3lf%%%49ls|\n", L" PROCESS_CPU_USER_USAGE", GetDoubleData(jh::PerformanceDataInfo::PROCESS_CPU_USER_USAGE), L" ");
	wprintf(L"|%-40ls : %17.3lf%%%49ls|\n", L" PROCESS_CPU_KERNEL_USAGE", GetDoubleData(jh::PerformanceDataInfo::PROCESS_CPU_KERNEL_USAGE), L" ");
	wprintf(L"|%-110ls|\n", L" ");

	wprintf(L"|%-40ls : %17.3lfMB%48ls|\n", L" PROCESS_PRIVATE_BYTES", GetDoubleData(jh::PerformanceDataInfo::PROCESS_PRIVATE_BYTES) / (1024*1024), L" "); // GetLargeData
	wprintf(L"|%-40ls : %17.3lfMB%48ls|\n", L" PROCESS_WORKING_SET", GetDoubleData(jh::PerformanceDataInfo::PROCESS_WORKING_SET) / (1024 * 1024), L" "); // GetLargeData
	wprintf(L"|%-110ls|\n", L" ");
	
	wprintf(L"|%-40ls : %17.3lfMB%48ls|\n", L" PROCESS_PAGED_POOL_BYTES", GetDoubleData(jh::PerformanceDataInfo::PROCESS_PAGED_POOL_BYTES) / (1024 * 1024), L" "); // GetLargeData
	wprintf(L"|%-40ls : %17.3lfMB%48ls|\n", L" PROCESS_NON_PAGED_POOL_BYTES", GetDoubleData(jh::PerformanceDataInfo::PROCESS_NON_PAGED_POOL_BYTES) / (1024 * 1024), L" "); // GetLargeData
	wprintf(L"|%-110ls|\n", L" ");
	
	wprintf(L"|%-40ls : %17.3lfMB%48ls|\n", L" SYSTEM_PAGED_POOL_BYTES", GetDoubleData(jh::PerformanceDataInfo::SYSTEM_PAGED_POOL_BYTES) / (1024 * 1024), L" "); // GetLargeData
	wprintf(L"|%-40ls : %17.3lfMB%48ls|\n", L" SYSTEM_NON_PAGED_POOL_BYTES", GetDoubleData(jh::PerformanceDataInfo::SYSTEM_NON_PAGED_POOL_BYTES) / (1024 * 1024), L" "); // GetLargeData
	wprintf(L"|%-110ls|\n", L" ");

	wprintf(L"|%-40ls : %17.3lf%%%49ls|\n", L" PROCESS USER / TOTAL PERCENTAGE", GetProcessUserPercentage(), L" ");
	wprintf(L"|%-40ls : %17.3lf%%%49ls|\n", L" PROCESS KERNEL / TOTAL PERCENTAGE", GetProcessKernelPercentage(), L" ");
	wprintf(L"+--------------------------------------------------------------------------------------------------------------+\n");

}

bool jh::PerformanceDataInfo::AddCounter(WCHAR(&path)[PDH_MAX_COUNTER_PATH], PDH_HCOUNTER* pCounter)
{
	PDH_STATUS pdhStatus = PdhAddCounter(m_hQuery, path, NULL, pCounter);

	// 쿼리 등록 실패
	if (ERROR_SUCCESS != pdhStatus)
	{
		wprintf(L"\AddCounter() failed... status 0x%x \n", pdhStatus);

		return false;
	}
	return true;
}

bool jh::PerformanceDataInfo::FillPathBuffer(DWORD performanceDataEnum, DWORD argType, WCHAR(&path)[PDH_MAX_COUNTER_PATH], const WCHAR* instanceName)
{
	int writeBytes = -1;

	// 카운터에 이름이 필요 없는 경우
	if (NONE == argType)
	{
		writeBytes = swprintf_s(path, kCounterRegInfo[performanceDataEnum].kCounterPath);
	}
	// 이름이 필요한 경우
	else
	{
		writeBytes = swprintf_s(path, kCounterRegInfo[performanceDataEnum].kCounterPath, instanceName);
	}

	return -1 != writeBytes;

	// 버퍼 쓰기 실패
	//if (-1 == writeBytes)
	//	return false;

	//return true;
}

bool jh::PerformanceDataInfo::AddNetworkCounter()
{
	WCHAR networkInstanceList[PDH_MAX_INSTANCE_NAME]; // 만약에 랜카드가 많아져서 이 문자열로 다 담지 못하면 크기를 더 늘리도록 한다.
	GetNetworkInterfaceName(networkInstanceList);

	WCHAR* networkInstance = networkInstanceList;

	WCHAR pathBuffer[PDH_MAX_COUNTER_PATH];

	// 인터페이스 리스트를 출력
	// networkInterfaceName 함수로 얻어온 이름은 (1번이름)\0(2번이름)\0(3번이름)\0\0 의 형태로 오기 때문에 이 형태로 파싱
	int idx = 0;
	while (L'\0' != *networkInstance)
	{
		NetworkCounter netCounter{};

		// File 생성 실패 또는 카운터 등록 실패 (카운터 등록 성공 : pdhStatus == 0)
		// total
		for (DWORD i = NETWORK_BYTES_TOTAL_PER_SEC; i < NUM_OF_DATA; i++)
		{
			if (false == FillPathBuffer(i, NETWORK_INSTANCE_NAME, pathBuffer, networkInstance) ||
				false == AddCounter(pathBuffer, &netCounter.m_hCounters[i - NETWORK_BYTES_TOTAL_PER_SEC]))
			{
				wprintf(L"\AddNetworkCounter() [%u] failed... instance : %s \n", i, networkInstance);
				return false;
			}
		}

		//if (false == FillPathBuffer(NETWORK_BYTES_TOTAL_PER_SEC, NETWORK_INSTANCE_NAME, pathBuffer, networkInstance) ||
		//	false == AddCounter(pathBuffer, &netCounter.m_hNetowrkTotalCounter))
		//{
		//	wprintf(L"\AddNetworkCounter() [NETWORK_BYTES_TOTAL_PER_SEC] failed... instance : %s \n", networkInstance);
		//	return false;
		//}

		//// send
		//if (false == FillPathBuffer(NETWORK_BYTES_SEND_PER_SEC, NETWORK_INSTANCE_NAME, pathBuffer, networkInstance) ||
		//	false == AddCounter(pathBuffer, &netCounter.m_hNetworkSendCounter))
		//{
		//	wprintf(L"\AddNetworkCounter() [NETWORK_BYTES_SEND_PER_SEC] failed... instance : %s \n", networkInstance);
		//	return false;
		//}

		//// recv
		//if (false == FillPathBuffer(NETWORK_BYTES_RECV_PER_SEC, NETWORK_INSTANCE_NAME, pathBuffer, networkInstance) ||
		//	false == AddCounter(pathBuffer, &netCounter.m_hNetworkRecvCounter))
		//{
		//	wprintf(L"\AddNetworkCounter() [NETWORK_BYTES_RECV_PER_SEC] failed... instance : %s \n", networkInstance);
		//	return false;
		//}

		m_networkCounters.push_back(netCounter);

		networkInstance += wcslen(networkInstance) + 1; // 
	}

	return true;
}

bool jh::PerformanceDataInfo::GetCurrentProcessName(WCHAR(&buffer)[MAX_PATH])
{
	DWORD writeBytes = MAX_PATH;

	const HANDLE processHandle = GetCurrentProcess();

	// 현재 프로세스의 이름을 얻는 함수 (절대 경로, 확장자 포함) 
	// PDH 프로세스 이름 넣으려면 이름만 얻어야함.
	if (0 == QueryFullProcessImageName(processHandle, 0, buffer, &writeBytes))
	{
		DWORD gle = GetLastError();

		wprintf(L"GetCurrentProcessName() failed, GetLastError : %u\n", gle);

		return false;
	}

	// 경로 이름 분할 함수
	WCHAR fileName[MAX_PATH]{};
https://learn.microsoft.com/ko-kr/cpp/c-runtime-library/reference/splitpath-s-wsplitpath-s?view=msvc-170 참고
	errno_t err = _wsplitpath_s(buffer, nullptr, 0, nullptr, 0, fileName, MAX_PATH, nullptr, 0);

	// 0 이 아니면 오류코드
	if (0 != err)
	{
		wprintf(L"GetCurrentProcessName _wsplitpath_s() failed, errorCode : %d\n", err);

		return false;
	}
	wcscpy_s(buffer, fileName);

	return true;
}

bool jh::PerformanceDataInfo::GetNetworkInterfaceName(WCHAR(&buffer)[PDH_MAX_INSTANCE_NAME])
{
	WCHAR dummyBuffer[PDH_MAX_INSTANCE_NAME]{};
	DWORD counterBufferSize = PDH_MAX_INSTANCE_NAME;

	DWORD instanceBufferSize = PDH_MAX_INSTANCE_NAME;

	// 랜카드 이름만 받도록한다.
	// 매개변수는 https://learn.microsoft.com/ko-kr/windows/win32/api/pdh/nf-pdh-pdhenumobjectitemsa 참고
	PDH_STATUS pdhStatus = PdhEnumObjectItems(nullptr, nullptr, L"Network Interface", dummyBuffer, &counterBufferSize, buffer, &instanceBufferSize, PERF_DETAIL_WIZARD, 0);

	// pdhStatus == PDH_MORE_DATA가 나와도 고정 크기 배열이니 재할당하지 않고 종료.
	if (ERROR_SUCCESS != pdhStatus)
	{
		if (PDH_MORE_DATA == pdhStatus)
			wprintf(L"GetNetworkInterfaceName() failed.. PDH_MORE_DATA, need size : %u\n", instanceBufferSize);
		else
			wprintf(L"GetNetworkInterfaceName() failed.. stattus : 0x%x, need size : %u\n", pdhStatus, instanceBufferSize);

		return false;
	}

	return true;
}

bool jh::PerformanceDataInfo::GetFormattedCounterValue(DWORD dataEnum, PDH_FMT_COUNTERVALUE* pCounterValue)
{
	if (dataEnum >= NETWORK_BYTES_TOTAL_PER_SEC)
	{
		wprintf(L"Go  GetFormattedCounterValue(PDH_HCOUNTER hCounter, DWORD format, PDH_FMT_COUNTERVALUE* pCounterValue)\n");
		return false;
	}
	// 3번째 인자는 카운터의 유형을 반환. 필요하지 않다.
	PDH_STATUS pdhStatus = PdhGetFormattedCounterValue(m_hCounters[dataEnum], kCounterRegInfo[dataEnum].m_dwFormat, NULL, pCounterValue);

	if (ERROR_SUCCESS != pdhStatus)
	{
		wprintf(L"GetFormattedCounterValue() failed.. stattus : 0x%x,\n", pdhStatus);
		return false;
	}
	return true;
}

bool jh::PerformanceDataInfo::GetFormattedCounterValue(PDH_HCOUNTER hCounter, DWORD format, PDH_FMT_COUNTERVALUE* pCounterValue)
{
	PDH_STATUS pdhStatus = PdhGetFormattedCounterValue(hCounter, format, NULL, pCounterValue);

	if (ERROR_SUCCESS != pdhStatus)
	{
		wprintf(L"GetFormattedCounterValue() failed.. stattus : 0x%x,\n", pdhStatus);
		return false;
	}
	return true;
}

