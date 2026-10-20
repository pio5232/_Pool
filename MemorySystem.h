#pragma once
#include "Memory.h"
namespace jh
{
	class MemoryPool;
	class PageAllocator;
	class MemoryAllocator;

	/// 메모리 관리 시스템.
	/// 유저는 이 클래스에 메모리 할당을 요청한다.
	class MemorySystem
	{
	private:

	public:
		MemorySystem();
		~MemorySystem();

		MemorySystem(const MemorySystem& other) = delete;
		MemorySystem(MemorySystem&& other) = delete;

		MemorySystem& operator=(const MemorySystem& other) = delete;
		MemorySystem& operator=(MemorySystem&& other) = delete;

		void* Alloc(size_t reqSize);
		void Free(void* ptr);

		void PrintMemoryUsage();
	private:
		MemoryAllocator* GetMemoryAllocator();

		/// <summary>
		/// LEVEL 1. 대부분의 경우 독립적으로 메모리를 할당 / 해제한다.
		/// </summary>
		/// 

		/// <summary>
		/// LEVEL 2. memoryAllocator가 소유한 노드가 부족해지면 m_pool에게서 얻어온다.
		/// </summary>
		MemoryPool* m_poolArr[kPoolCount];

		/// <summary>
		/// LEVEL 3. 최종적인 메모리 할당기. 
		/// VirtualAlloc을 통해 할당한 메모리를 전달한다.
		/// </summary>
		PageAllocator* m_pageAllocator;

#ifdef JH_MEM_ALLOC_CHECK_FLAG
		LONGLONG m_llTestAllocCounter = 0;
		LONGLONG m_llTestDeallocCounter = 0;
#endif
	};
}
