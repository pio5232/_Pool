#pragma once

#include "Memory.h"

namespace jh
{
	class PageAllocator;
	class MemoryPool
	{
	public:
		MemoryPool(size_t allocSize);
		~MemoryPool();

		void RegisterPageAllocator(PageAllocator* pageAllocator);

		// 낱개의 덩어리 단위로 추가 (일반적인 상황)
		void TryPushBlock(Node* nodeHead, size_t nodeCount);
		Node* TryPopBlock();

		// 여러개의 덩어리 단위로 추가 (메모리 추가 할당 시)
		void TryPushBlockList(Node* nodeHead, Node* nodeTail);

		// 낱개 단위로 반환 (스레드 종료 시)
		void TryPushNode(Node* node);

		Node* GetNewBlock();

	private:
		LONGLONG GetIncreasedCounter()
		{
			return InterlockedIncrement64(&m_llComplexCounter);
		}

		// Node* -> [Counter][Node*] 형태로 만든다
		LONGLONG GetComplexNode(LONGLONG counter, Node* node) const
		{
			return  (reinterpret_cast<LONGLONG>(node) | (counter << kCounterShift));
		}

		// [Counter ][Node*] -> Node* 형태로 만든다.
		Node* GetNodePointer(LONGLONG complexNode) const
		{
			return  reinterpret_cast<Node*>((complexNode & kPointerMask));
		}

	private:
		const size_t m_allocSize;
		PageAllocator* m_pPageAllocator;

		// MemoryAllocator 존재하는 스레드 종료 시 남아있는 잉여 노드들의 저장을 위해 락 사용
		SRWLOCK m_partialLock;
		SRWLOCK m_allocationLock;
		Node* m_pPartialNodeHead; // Block에 대한 연결
		size_t m_partialNodeCount;

		alignas(64) LONGLONG m_llComplexCounter;
		alignas(64) LONGLONG m_llComplexFullNode; // Block에 대한 연결


	public:
#ifdef JH_MEM_ALLOC_CHECK_FLAG
		alignas(64) LONGLONG m_llL2TotalNode = 0;
		alignas(64) LONGLONG m_llL2AllocNodeCount = 0;
		alignas(64) LONGLONG m_llL2DeallocNodeCount = 0;
#endif

	};
}


