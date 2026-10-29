#pragma once

// 프로파일링 플래그
//#define JH_MEM_PROFILE_FLAG

// alloc / free 계산용 플래그 
//#define JH_MEM_ALLOC_CHECK_FLAG


// 메모리 추가/제거 카운팅을 위한 정보. 디버깅할때만 플래그를 이용해 탐지하도록 한다.
#ifdef JH_MEM_ALLOC_CHECK_FLAG
#define ALLOC_COUNT_CHECK(x) \
	do {	\
	{		\
	x		\
	}		\
	} while(0);
#else 
#define ALLOC_COUNT_CHECK(x)
#endif

#ifdef JH_MEM_PROFILE_FLAG
#define MEMORY_POOL_PROFILE_FLAG PRO_START_AUTO_FUNC
#else
#define MEMORY_POOL_PROFILE_FLAG 
#endif


namespace jh
{

	/// <summary>
	/// m_pNextNode : Node들의 연결을 나타낸다.
	/// 
	/// +---------------------- NodeStack ----------------------+
	/// |	[Node]	->	[Node]	->	[Node]	->	[Node]		|
	/// |	NodeStack 내의 연결은 m_pNextNode를 통해 이루어진다.|
	/// +---------------------- NodeStack ----------------------+ 
	///			|
	///			|	
	///			|
	/// +---------------------- NodeStack ----------------------+
	/// |	[Node]	->	[Node]	->	[Node]	->	[Node]		|
	/// |	NodeStack 내의 연결은 m_pNextNode를 통해 이루어진다.|
	/// +---------------------- NodeStack ----------------------+ 

	/// NodeStack 간의 연결은 m_pNextBlock을 통해 이루어진다.
	/// m_pNextBlock은 Blcok 내의 첫 Node만 활용하도록 한다.
	/// 또한 m_llNextComplexBlock : LEVEL 2에서 관리되어야 한다.
	/// 
	/// [LEVEL 1] MemoryAllocator를 통해서 사용될 때 Node의 구조
	/// [	[size_t]					[Data]				[ ]						]
	/// 
	/// [LEVEL 2] memoryPool을 통해서 사용될 때 Node의 구조
	/// [	[m_llNextComplexBlock]		[m_blockSize]		[m_pNextNode]			]
	///  
	/// 현재 Block과 Node의 차이
	/// 
	/// 기본 : Node*로 관리되는 연결리스트 형태
	/// Block : 각 연결리스트를 의미 
	/// 
	/// </summary>
	struct Node
	{
		LONGLONG m_llNextComplexBlock; // L2 에서 Block의 할당/해제에 사용된다. 앞에 counter가 붙는다.
		size_t m_blockSize; // L2 에서 Block의 할당/해제에 사용된다
		Node* m_pNextNode;
	};

	// 64 / 128 / 256 / 512 / 1024 / 2048 / 4096
	// MemoryPool의 개수. 사이즈별로 존재한다.
	constexpr size_t kPoolCount = 7;
	constexpr size_t kPoolSizeArr[kPoolCount]		= { {64}, { 128 }, { 256 }, { 512 }, { 1024 }, { 2048 }, { 4096 } };

	// Level 3에서 얻어오는 사이즈를 정하기 위한 수
	// MemoryPool Size * kMaxBlockCount 와 kAllocationGranularity를 통해 할당받을 사이즈를 정의한다.
	constexpr size_t kAllocationGranularity			= 4096 * 16;
	constexpr size_t kNodeCountToCreate				= 4096;

	// MemoryPool에서 관리되는 1블럭당 연결된 Node의 수
	constexpr size_t kNodeCountPerBlock				= 512;

	constexpr size_t kBlockCountToCreate			= kNodeCountToCreate / kNodeCountPerBlock;

	// Pool에서 관리할 수 있는 최대 사이즈. 이 사이즈를 넘어가면 new / delete를 통해 할당/해제를 진행한다.
	constexpr size_t kMaxAllocSize					= 4096;
	constexpr size_t kPageSize						= 4096;


	// [카운터] [포인터] 된 값의 마스킹을 위한 값
	constexpr ULONGLONG kPointerMask				= 0x00007fffffffffff;
	constexpr int kCounterShift						= 47;

	inline consteval std::array<int, kMaxAllocSize + 1> CreatePoolTable()
	{
		std::array<int, kMaxAllocSize + 1> poolTable{};

		int targetSize = 0;
		int size = 1;
		int tableIndex = 0;

		for (targetSize = 64; targetSize <= kMaxAllocSize; targetSize *= 2)
		{
			while (size <= targetSize)
			{
				poolTable[size] = tableIndex;
				size++;
			}

			tableIndex++;
		}
		return poolTable;
	}

	class PageAllocator; // LEVEL 3
	class MemoryPool; // LEVEL 2
	class MemoryAllocator; // LEVEL 1


	inline constexpr std::array<int, kMaxAllocSize + 1> poolTable = CreatePoolTable(); // Size 요청에 따른 인덱스 탐색을 위한 맵핑 테이블
}