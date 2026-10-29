#include "pch.h"
#include "MemoryPool.h"
#include "PageAllocator.h"

jh::MemoryPool::MemoryPool(size_t allocSize) : m_kAllocSize{ allocSize }, m_llComplexFullNode{}, m_pPartialNodeHead{}, m_partialNodeCount{}, m_pPageAllocator{}, m_llComplexCounter{}
{
	InitializeSRWLock(&m_partialLock);
	InitializeSRWLock(&m_allocationLock);
}

jh::MemoryPool::~MemoryPool()
{

}

void jh::MemoryPool::RegisterPageAllocator(PageAllocator* pageAllocator)
{
	if (nullptr == m_pPageAllocator)
		m_pPageAllocator = pageAllocator;
}

void jh::MemoryPool::TryPushBlock(Node* nodeHead, size_t nodeCount)
{
	MEMORY_POOL_PROFILE_FLAG;
	nodeHead->m_blockSize = nodeCount;

	LONGLONG topComplexNode;

	LONGLONG counter = GetIncreasedCounter();
	LONGLONG newComplexNode = GetComplexNode(counter, nodeHead);
	do
	{
		topComplexNode = m_llComplexFullNode;

		nodeHead->m_llNextComplexBlock = topComplexNode;
	}

	while (InterlockedCompareExchange64(&m_llComplexFullNode, newComplexNode, topComplexNode) != topComplexNode);


	ALLOC_COUNT_CHECK(InterlockedAdd64(&m_llL2DeallocNodeCount, static_cast<LONGLONG>(nodeCount));)

}

jh::Node* jh::MemoryPool::TryPopBlock()
{
	// 1. FullNode 확인
	MEMORY_POOL_PROFILE_FLAG;
	LONGLONG topComplexNode;

	Node* topNodePointer;

	while (1)
	{
		topComplexNode = m_llComplexFullNode;

		topNodePointer = GetNodePointer(topComplexNode);

		if (nullptr == topNodePointer)
		{
			Node* newBlock = GetNewBlock();

			// Level 3에서 새로운 메모리를 할당받아서 블록들을 연결한 상태이다.
			if (nullptr != newBlock)
				return newBlock;

			// topNode가 없어서 새로운 블록을 얻으려고 진입했지만 누군가 새로운 블록을 만들어서 다시 경쟁을 하러 내려온 상황이다.
			continue;
		}

		if ((InterlockedCompareExchange64(&m_llComplexFullNode, topNodePointer->m_llNextComplexBlock, topComplexNode) == topComplexNode))
			break;
	}

	ALLOC_COUNT_CHECK(InterlockedAdd64(&m_llL2AllocNodeCount, static_cast<LONGLONG>(topNodePointer->m_blockSize));)
		return topNodePointer;
}


void jh::MemoryPool::TryPushBlockList(Node* nodeHead, Node* nodeTail)
{
	// blockSize는 각각 입력된 상태로 전달
	// [nodeHead](새로운 head) -> [nodeTail] -> [기존 head] 형태로 push

	// 이미 노드들은 원자적으로 연결된 상태로 전달된다.
	MEMORY_POOL_PROFILE_FLAG;
	LONGLONG topComplexNode;

	LONGLONG counter = GetIncreasedCounter();
	LONGLONG newComplexNode = GetComplexNode(counter, nodeHead);
	do
	{
		topComplexNode = m_llComplexFullNode;

		nodeTail->m_llNextComplexBlock = topComplexNode;
	}

	while (InterlockedCompareExchange64(&m_llComplexFullNode, newComplexNode, topComplexNode) != topComplexNode);

}

// 이 함수는 MemoryAllocator가 소멸될때만 호출되어야한다.
// 호출 빈도 수는 매우 적음.
void jh::MemoryPool::TryPushNode(Node* node)
{
	MEMORY_POOL_PROFILE_FLAG;
	//PRO_START_AUTO_FUNC;
	Node* separatedNode;

	{
		SRWLockGuard lockGuard(&m_partialLock);

		// head로 등록
		node->m_pNextNode = m_pPartialNodeHead;
		m_pPartialNodeHead = node;
		m_partialNodeCount++;

		ALLOC_COUNT_CHECK(InterlockedIncrement64(&m_llL2DeallocNodeCount);)

			if (kNodeCountPerBlock > m_partialNodeCount)
				return;

		// 한 덩어리로 묶을 수 있는 크기가 된다면 다시 묶어서 FullNode List에 넣어준다.
		Node* curNode = m_pPartialNodeHead;
		separatedNode = m_pPartialNodeHead;

		for (int i = 0; i < (kNodeCountPerBlock - 1); i++)
		{
			curNode = curNode->m_pNextNode;
		}

		// 노드 분리 후 head 재설정
		m_pPartialNodeHead = curNode->m_pNextNode;
		curNode->m_pNextNode = nullptr;

		m_partialNodeCount -= kNodeCountPerBlock;
	}

	// 해당 개수만큼을 LEVEL 2에 반환한다.
	TryPushBlock(separatedNode, kNodeCountPerBlock);

	ALLOC_COUNT_CHECK
	(
		LONGLONG r = kNodeCountPerBlock;

	InterlockedAdd64(&m_llL2DeallocNodeCount, -r);
	)


}

jh::Node* jh::MemoryPool::GetNewBlock()
{
	MEMORY_POOL_PROFILE_FLAG;

	size_t granularitySize = kNodeCountToCreate / (jh::kAllocationGranularity / m_kAllocSize);

	//	블록 크기								64,		128,	256,	512,	1024,	2048,	4096
	//	allocationGranurity 당 노드 수			1024,	512,	256,	128,	64,		32,		16

	SRWLockGuard lockGuard(&m_allocationLock);
	{
		LONGLONG topComplexNode = m_llComplexFullNode;

		// 진입 시에 이미 다른 스레드가 lock을 통해 페이지 할당받고 자른 게 아닌 경우
		if (nullptr == GetNodePointer(topComplexNode))
		{
			// 페이지 할당
			void* allocatedPoitner = m_pPageAllocator->AllocPage(granularitySize);

			// page를 할당받고
			// m_allocSize만큼씩을 쪼개서
			size_t numOfBlock = 0;

			// [kBlockCountToCraete-2](head) -> [...] -> [0] (tail)
			Node* head = nullptr;
			Node* tail = nullptr;

			// lastBlock : 연결하지 않고 반환해서 사용할 마지막 블럭.
			Node* lastBlock = nullptr;

			// 블럭 단위로 연결
			for (; numOfBlock < kBlockCountToCreate; numOfBlock++)
			{
				size_t addr = reinterpret_cast<size_t>(allocatedPoitner) + (numOfBlock * m_kAllocSize * kNodeCountPerBlock);

				// N번째 블락의 시작점은 [시작점 + (풀 할당 크기 * 블럭당 노드 수)] 이다.
				Node* blockBaseNode = reinterpret_cast<Node*>(addr);

				if (numOfBlock > 0 && numOfBlock < (kBlockCountToCreate - 1))
				{
					LONGLONG counter = GetIncreasedCounter();
					LONGLONG complexNode = GetComplexNode(counter, lastBlock);

					blockBaseNode->m_llNextComplexBlock = complexNode;
				}

				blockBaseNode->m_blockSize = kNodeCountPerBlock;

				// Node단위로 연결
				for (size_t numOfNode = 0; numOfNode < kNodeCountPerBlock - 1; numOfNode++)
				{
					Node* curNode = reinterpret_cast<Node*>(addr + numOfNode * m_kAllocSize);
					Node* nextNode = reinterpret_cast<Node*>(addr + (numOfNode + 1) * m_kAllocSize);

					curNode->m_pNextNode = nextNode;
				}
				// 블럭의 마지막 노드 값 설정
				Node* lastNodeInBlock = reinterpret_cast<Node*>(addr + (kNodeCountPerBlock - 1) * m_kAllocSize);
				lastNodeInBlock->m_pNextNode = nullptr;

				lastBlock = blockBaseNode;

				if (0 == numOfBlock)
					tail = lastBlock;

				if (kBlockCountToCreate - 1 != numOfBlock)
					head = lastBlock;
			}

			if (nullptr != head && nullptr != tail)
				TryPushBlockList(head, tail);

			if (nullptr != lastBlock)
				lastBlock->m_llNextComplexBlock = 0;

			ALLOC_COUNT_CHECK
			(
				InterlockedAdd64(&m_llL2TotalNode, kNodeCountToCreate);
			InterlockedAdd64(&m_llL2AllocNodeCount, static_cast<LONGLONG>(lastBlock->m_blockSize));
			)
				return lastBlock;
		}
		else
		{
			return nullptr;
		}

	}
}