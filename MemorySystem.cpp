#include "pch.h"
#include "MemorySystem.h"
#include "MemoryAllocator.h"
#include "PageAllocator.h"
#include "MemoryPool.h"
#include "MemoryHeader.h"


jh::MemorySystem::MemorySystem()
{
    m_pageAllocator = new PageAllocator();

    for (int i = 0; i < kPoolCount; i++)
    {
        m_poolArr[i] = new MemoryPool(kPoolSizeArr[i]);

        m_poolArr[i]->RegisterPageAllocator(m_pageAllocator);
    }

}

jh::MemorySystem::~MemorySystem()
{
#ifdef JH_MEM_ALLOC_CHECK_FLAG

    printf("================================== ~MemorySystem ==================================\n");
    printf("Test Alloc Counter : %lld\n", m_llTestAllocCounter);
    printf("Test Dealloc Counter : %lld\n", m_llTestDeallocCounter);

    LONGLONG poolAllocCountSum = 0;
    LONGLONG poolDeallocCountSum = 0;
    LONGLONG poolTotal = 0;

    for (int i = 0; i < kPoolCount; i++)
    {
        LONGLONG poolAllocCountSum2 = InterlockedExchange64(&m_poolArr[i]->m_llL2AllocNodeCount, 0);
        LONGLONG poolDeallocCountSum2 = InterlockedExchange64(&m_poolArr[i]->m_llL2DeallocNodeCount, 0);

        //printf("[%d] poolAllocCountSum2 : [%lld]\n", i, poolAllocCountSum2);
        //printf("[%d] poolDeallocCountSum2 : [%lld]\n\n", i, poolDeallocCountSum2);

        printf("[%d] %30s : [%lld]\n", i, "poolAllocCountSum2", poolAllocCountSum2);
        printf("[%d] %30s : [%lld]\n\n", i, "poolDeallocCountSum2", poolDeallocCountSum2);

        poolAllocCountSum += poolAllocCountSum2;
        poolDeallocCountSum += poolDeallocCountSum2;

        poolTotal += InterlockedExchange64(&m_poolArr[i]->m_llL2TotalNode, 0);

        delete m_poolArr[i];
        m_poolArr[i] = nullptr;
    }

    delete m_pageAllocator;
    m_pageAllocator = nullptr;

    //printf("Test L2 Alloced Sum : %lld\n", poolAllocCountSum);
    //printf("Test L2 Dealloced Sum : %lld\n", poolDeallocCountSum);
    //printf("Test L2 Created Sum : %lld\n", poolTotal);

    printf("%40s : %lld\n", "Test L2 Alloced Sum", poolAllocCountSum);
    printf("%40s : %lld\n", "Test L2 Dealloced Sum", poolDeallocCountSum);
    printf("%40s : %lld\n", "Test L2 Created Sum", poolTotal);

    printf("---------------------------------------------------------\n\n");

#else
    for (int i = 0; i < kPoolCount; i++)
    {
        delete m_poolArr[i];
        m_poolArr[i] = nullptr;
    }

    delete m_pageAllocator;
    m_pageAllocator = nullptr;
#endif
}

void* jh::MemorySystem::Alloc(size_t reqSize)
{
    MEMORY_POOL_PROFILE_FLAG;
    size_t allocSize = reqSize + sizeof(size_t);

    void* pAddr;

    if (allocSize > kMaxAllocSize)
    {
        pAddr = new char[allocSize];
    }
    else
    {
        MemoryAllocator* memoryAllocatorPtr = GetMemoryAllocator();

        pAddr = memoryAllocatorPtr->Alloc(allocSize);

        ALLOC_COUNT_CHECK(InterlockedIncrement64(&m_llTestAllocCounter);)
    }

    return MemoryHeader::AttachHeader(static_cast<MemoryHeader*>(pAddr), allocSize);

}

void jh::MemorySystem::Free(void* ptr)
{
    MEMORY_POOL_PROFILE_FLAG;
    MemoryHeader* basePtr = MemoryHeader::DetachHeader(ptr);

    size_t allocSize = basePtr->m_size;

    if (allocSize > kMaxAllocSize)
    {
        delete[] basePtr;
        return;
    }

    MemoryAllocator* memoryAllocatorPtr = GetMemoryAllocator();

    memoryAllocatorPtr->Dealloc(basePtr, allocSize);

    ALLOC_COUNT_CHECK(InterlockedIncrement64(&m_llTestDeallocCounter);)
}

jh::MemoryAllocator* jh::MemorySystem::GetMemoryAllocator()
{
    MEMORY_POOL_PROFILE_FLAG;
    static thread_local jh::MemoryAllocator memoryAllocator;

    if (true == memoryAllocator.CanRegisterPool())
        memoryAllocator.RegisterPool(m_poolArr);

    return &memoryAllocator;
}

void jh::MemorySystem::PrintMemoryUsage()
{
    ALLOC_COUNT_CHECK(
        printf("=============================== L2 MemoryPool Usage ===============================\n");
    for (int i = 0; i < kPoolCount; i++)
    {
        LONGLONG l2TotalNodeCount = m_poolArr[i]->m_llL2TotalNode;
        LONGLONG l2AllocNodeCount = m_poolArr[i]->m_llL2AllocNodeCount;
        LONGLONG l2DeallocNodeCount = m_poolArr[i]->m_llL2DeallocNodeCount;

        printf("Pool [%2d] Total : [%10lld], Alloc : [%10lld]\n", i, l2TotalNodeCount, l2AllocNodeCount - l2DeallocNodeCount);
    }

    ULONGLONG l3Bytes = static_cast<ULONGLONG>(m_pageAllocator->GetTotalAllocSize());
    printf("\nManaged Bytes : [%0.2lf]MB \n\n", static_cast<double>(l3Bytes) / 1024 / 1024);
    printf("===================================================================================\n");
    )
}
