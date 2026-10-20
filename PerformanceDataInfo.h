#pragma once
#include <Pdh.h>

namespace jh
{
	class PerformanceDataInfo
	{
	public:

		enum PERFOMANCE_DATA_ENUM : DWORD
		{
			PROCESS_CPU_USAGE = 0,										// 프로세스 cpu 사용률
			PROCESS_CPU_USER_USAGE,										// 프로세스 유저모드 cpu 사용률
			PROCESS_CPU_KERNEL_USAGE,									// 프로세스 커널모드 cpu 사용률

			PROCESS_PRIVATE_BYTES,										// 프로세스가 사용중인 commit 메모리 ( RAM + DISK, 공유 메모리 포함 X)
			PROCESS_WORKING_SET,										// 프로세스가 사용중인 워킹셋 사용량 ( RAM에 있는 commit 메모리, 공유 메모리 포함 O )

			PROCESS_PAGED_POOL_BYTES,									// 프로세스 페이징 풀 사용량
			PROCESS_NON_PAGED_POOL_BYTES,								// 프로세스 논페이징 풀 

			SYSTEM_PAGED_POOL_BYTES,									// 시스템 전체 페이징 풀 사용량
			SYSTEM_NON_PAGED_POOL_BYTES,								// 시스템 전체 논페이징 풀 사용량

			SYSTEM_CPU_USAGE_TOTAL,										// 시스템 전체 cpu 사용률 

			// 인스턴스 한 개만 측정하는 데이터의 수
			NUM_OF_SINGLE_DATA,

			////  +----------------------------							// 네트워크 카운터 (필요 시 마지막부터 추가)     ----------------------------+
			NETWORK_BYTES_TOTAL_PER_SEC = NUM_OF_SINGLE_DATA,			// 초당 송신 + 수신 바이트											 |
			NETWORK_BYTES_SEND_PER_SEC,									// 초당 송신 바이트													 |
			NETWORK_BYTES_RECV_PER_SEC,									// 초당 수신 바이트													 |
			//	  +----------------------------------------------------------------------------------------------------------------------------------+		
			NUM_OF_DATA,
		};

		enum PERFORMANCE_ARGUMENT_TYPE : DWORD
		{
			NONE = 0,
			PROCESS_NAME,
			NETWORK_INSTANCE_NAME,
			NUM_OF_ARG_TYPE,
		};

		// PDH_FMT_COUNTERVALUE 에서 따왔음.
		union PerformanceUnionData
		{
			LONG        longValue;
			double      doubleValue;
			LONGLONG    largeValue;
		};
		PerformanceDataInfo();
		~PerformanceDataInfo();

		void UpdateQuery();

		LONGLONG GetLargeData(DWORD performanceDataEnum);
		double GetDoubleData(DWORD performanceDataEnum);
		LONG GetLongData(DWORD performanceDataEnum);

		void Print();


		double GetProcessUserPercentage(); // 프로세스 유저모드 cpu 사용률 ( 프로세스 기준 사용량, user + kernel의 합을 100%로 보는 상황 )
		double GetProcessKernelPercentage();  // 프로세스 커널모드 cpu 사용률 ( 프로세스 기준 사용량, user + kernel의 합을 100%로 보는 상황 )
	private:
		void Init();

		// 패스와 번호를 만들어 카운터 등록을 하는 기본 함수.
		// 1. 어떤 데이터에 대한 카운터를 등록할 것인지 / 카운터 등록을 할 때 필요한 이름의 수 / 카운터 등록 시 필요한 패스 버퍼 / 필요한 이름 (argtype == NONE 일 시는 nullptr 전달)
		bool AddCounter(WCHAR(&path)[PDH_MAX_COUNTER_PATH], PDH_HCOUNTER* pCounter);
		bool FillPathBuffer(DWORD performanceDataEnum, DWORD argType, WCHAR(&path)[PDH_MAX_COUNTER_PATH], const WCHAR* instanceName);
		bool AddNetworkCounter();

		// 현재 실행중인 프로세스의 이름을 얻어온다.
		bool GetCurrentProcessName(WCHAR(&buffer)[MAX_PATH]);
		bool GetNetworkInterfaceName(WCHAR(&buffer)[PDH_MAX_INSTANCE_NAME]);

		// 일반 
		bool GetFormattedCounterValue(DWORD dataEnum, PDH_FMT_COUNTERVALUE* pCounterValue);
		// 네트워크 전용
		bool GetFormattedCounterValue(PDH_HCOUNTER hCounter, DWORD format, PDH_FMT_COUNTERVALUE* pCounterValue);

		struct CounterRegInfo
		{
			const WCHAR* kCounterPath;
			const DWORD m_argType;  // NONE (0) 인 경우는 매개변수가 필요 없는 것으로 간주한다.
			const DWORD m_dwFormat;
		};
		static const CounterRegInfo kCounterRegInfo[NUM_OF_DATA];

		PDH_HQUERY m_hQuery;

		// 네트워크 랜카드는 여러개 존재할 수 있으므로 네트워크 카운터는 따로 정리함
		PDH_HCOUNTER m_hCounters[NUM_OF_SINGLE_DATA];

		struct NetworkCounter
		{
		private:

		public:
			PDH_HCOUNTER m_hCounters[NUM_OF_DATA - NETWORK_BYTES_TOTAL_PER_SEC];
		};

		// 만약 랜카드가 4개라면 4개의 랜카드에 대한 total / send / recv 항목 수집
		std::vector<NetworkCounter> m_networkCounters;

		PerformanceUnionData m_data[NUM_OF_DATA];


	};
}


