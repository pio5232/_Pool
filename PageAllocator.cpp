#include "pch.h"
#include "PageAllocator.h"

jh::PageAllocator::PageAllocator() : m_totalAllocSize{}
{
	InitializeSRWLock(&m_lock);

	m_allocedInfoList.reserve(100);
}

jh::PageAllocator::~PageAllocator()
{
	SRWLockGuard lockGuard(&m_lock);
	for (auto [addr, granularitySize] : m_allocedInfoList)
	{
		if (false == ReleasePage(addr))
			continue;

		m_totalAllocSize -= kAllocationGranularity * granularitySize;
	}

	m_allocedInfoList.clear();
}

void* jh::PageAllocator::AllocPage(size_t granularitySize)
{
	void* p = VirtualAlloc(nullptr, kAllocationGranularity * granularitySize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

	if (nullptr == p)
		jh_utility::CrashDump::Crash();

	SRWLockGuard lockGuard(&m_lock);
	m_allocedInfoList.emplace_back(p, granularitySize);

	m_totalAllocSize += kAllocationGranularity * granularitySize;

	return p;
}

bool jh::PageAllocator::ReleasePage(void* p)
{
	if (false == VirtualFree(p, 0, MEM_RELEASE))
	{
		DWORD gle = GetLastError();
		printf("Dealloc Failed. GLE : [%u]\n", gle);

		return false;
	}

	return true;
}
