#pragma once

namespace jh
{
	struct AllocedPageInfo
	{
		void* m_pAddr;
		size_t m_granularitySize;
	};
	class PageAllocator
	{
	public:
		PageAllocator();
		~PageAllocator();

		PageAllocator(const PageAllocator& other) = delete;
		PageAllocator(PageAllocator&& other) = delete;

		PageAllocator& operator=(const PageAllocator& other) = delete;
		PageAllocator& operator=(PageAllocator&& other) = delete;

		void* AllocPage(size_t blockSize);
		bool ReleasePage(void* p);

		size_t GetTotalAllocSize() const { return m_totalAllocSize; }
	private:
		SRWLOCK							m_lock;

		std::vector<AllocedPageInfo>	m_allocedInfoList;
		size_t							m_totalAllocSize;

	};
}

