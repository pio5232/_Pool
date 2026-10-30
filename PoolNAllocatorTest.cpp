#include "pch.h"
#include <algorithm>
#include <numeric>
#include <bitset>
#include <format>
#include <iostream>
#include <conio.h>
#include "Rand.h"
#include "SRWLockGuard.h"

const int g_kRandMin = 1e4;
const int g_kRandMax = 1e5;

const int g_kAllocSize[]{ 4,40,400, 1222 };

const unsigned int g_kTestMax = 0xff'ff'ff'ff;

struct test_sample
{
    char a[4096];// 할당의 최대 크기느 4096보다 작음, 메모리0 비교를 위한 값
} g_sample0, g_sample1;

HANDLE g_hStarterHandle;
jh::PerformanceDataInfo g_myPdh;

inline void ChkHandle(HANDLE handle)
{
    if (nullptr == handle)
        jh_utility::CrashDump::Crash();
}

inline void ClearHandle(HANDLE handle)
{
    if (nullptr != handle)
    {
        CloseHandle(handle);
        handle = nullptr;
    }
}

int GetAvailableProcessors()
{
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);

    return sysInfo.dwNumberOfProcessors;
}
void InitMain()
{
    memset(&g_sample1, 0xff, sizeof(g_sample1));

    g_hStarterHandle = CreateEvent(nullptr, true, false, nullptr);
    ChkHandle(g_hStarterHandle);
}

void ClearMain()
{
    ClearHandle(g_hStarterHandle);
}


inline int GetAllocSizeCnt()
{
    return sizeof(g_kAllocSize) / sizeof(g_kAllocSize[0]);
}
inline void NewLine() {
    printf("\n\n\n\n\n\n\n");
}
int SelectMenu()
{
    const int width = 50;
    const char* menu[]{ "---------------메뉴---------------", "1. 단일 스레드 테스트", "2. 멀티 스레드 테스트", "3. new/delete 비교 테스트", "종료 : 0" };

    int half = (width - strlen(menu[0])) / 2;

    printf("%*s%s%*s\n", half, " ", menu[0],half, " ");

    for (int i = 1; i < sizeof(menu) / sizeof(menu[0]); i++)
    {
        printf("%s\n", menu[i]);
    }

    printf("\n메뉴 번호를 입력해주세요 (0은 종료) : ");

    int menuNum;
    scanf_s("%d", &menuNum);

    return menuNum;
}

template <typename T>
class LockVector
{
public:
    LockVector() { InitializeSRWLock(&m_lock); }
      
    std::vector<T> m_vec;
    SRWLOCK m_lock;
};

//template <>
//class LockQueue<void>
//{
//public :LockQueue() {}
//       void Push(int a = 0) {}
//       void Pop() {}
//       int Size() const { return 0; }
//};
class Mode
{
public:
    Mode() = default;
    virtual ~Mode() = 0;
    
    virtual void AllocTest() = 0;
    int GetExtraThreadCnt() { return m_iExtraThreadCnt; }

private:
    virtual bool TestWrite(void* p, int rqst_size) = 0;
    virtual bool TestRead(void* p, int rqst_size) = 0;

protected:
    static const int kAllocLoopCnt = 2e3;

    enum AllocationOp
    {
        OP_MY_ALLOC = 0,
        OP_MY_FREE,
        OP_NEW,
        OP_DEL,
        ALLOCATION_MAX,
    };

    const WCHAR* kOpStr[ALLOCATION_MAX] = {L"MyAlloc",L"MyFree",L"New",L"Delete"};
    
    // 변수 할당의 크기가 n개중 랜덤으로 요청되는 테스트
    void RecordRandomAllocTest(int allocFreeRqstCnt, bool isSingleMode, bool usePool,int loopRqstCnt = kAllocLoopCnt, const WCHAR* comment = L"")
    {
        std::mt19937 gen(0);

        const int maxIdx = GetAllocSizeCnt();
        std::vector<void*> pointers(allocFreeRqstCnt, nullptr);
        
        // 랜덤 사이즈 저장용
        std::vector<int> sizeVector(allocFreeRqstCnt);        
        
        // 할당 횟수 / 사이즈n개 나누기
        for (int i = 0; i < sizeVector.size(); i++)
        {
            sizeVector[i] = g_kAllocSize[i % maxIdx];
        }

        std::shuffle(sizeVector.begin(), sizeVector.end(), gen);

        WCHAR buffer[ALLOCATION_MAX][100]{};
        const WCHAR* str[ALLOCATION_MAX];

        for (int i = 0; i < ALLOCATION_MAX; i++)
        {
            wsprintf(buffer[i], L"%s %s_Size_%s_%s", isSingleMode ? L"Single" : L"Multi", PROF_WFUNC, kOpStr[i], comment);

            str[i] = buffer[i];
        }

        for (int i = 0; i < loopRqstCnt + 1; i++)
        {
            if (i > 0)
            {
                if (usePool)
                {
                    // alloc_size에 나온 사이즈중 하나를 랜덤으로 뽑아 할당
                    {
                        PRO_START_AUTO_WNAME(buffer[OP_MY_ALLOC]);
                        for (int j = 0; j < allocFreeRqstCnt; j++)
                        {
                            pointers[j] = { g_pMemSystem->Alloc(sizeVector[j])};
                        }
                    }
                    {
                        PRO_START_AUTO_WNAME(buffer[OP_MY_FREE]);

                        // 역방향 Free >>> NodeStack이니까 최대한 캐시히트를 많이 만들어내자.
                        for (int j = allocFreeRqstCnt - 1; j >= 0; j--)
                        {
                            g_pMemSystem->Free(pointers[j]);

                            // 속도 테스트이기 때문에 초기화 진행하지 않는다. Single에서 이미 테스트를 진행했기 때문.
                            // pointers[j] = nullptr;
                        }
                    }
                }
                else
                {
                    {
                        PRO_START_AUTO_WNAME(buffer[OP_NEW]);
                        for (int j = 0; j < allocFreeRqstCnt; j++)
                        {
                            pointers[j] = ::operator new (sizeVector[j]);
                        }
                    }
                    {
                        PRO_START_AUTO_WNAME(buffer[OP_DEL]);
                        for (int j = allocFreeRqstCnt - 1; j >= 0; j--)
                        {
                            ::operator delete(pointers[j]);

                            // 위와 동일한 이유로 제거한다.
                            // pointers[j] = nullptr;
                        }
                    }
                }
            }
            // 시작하기 전 한 바퀴는 기록하지 않는다.
            else
            {
                for (int j = 0; j < allocFreeRqstCnt; j++)
                {
                    if (usePool)
                        pointers[j] = { g_pMemSystem->Alloc(sizeVector[j]) };
                    else
                        pointers[j] = ::operator new (sizeVector[j]);
                }

                for (int j = allocFreeRqstCnt - 1; j >= 0; j--)
                {
                    if (usePool)
                        g_pMemSystem->Free(pointers[j]);
                    else
                        ::operator delete(pointers[j]);
                }
            }
        }
      
    }
    // 변수 할당의 크기가 고정된 형태의 테스트
    void RecordSimpleAllocTest(int allocFreeRqstCnt, bool isSingleMode, bool usePool,int loopRqstCnt = kAllocLoopCnt, const WCHAR* comment = L"")
    {
        const int maxIdx = GetAllocSizeCnt();
        std::vector<void*> pointers(allocFreeRqstCnt, nullptr);
     
        for (int allocSize : g_kAllocSize)
        {
            WCHAR buffer[ALLOCATION_MAX][100]{};
            const WCHAR* str[ALLOCATION_MAX]; 
          
            for (int i = 0; i < ALLOCATION_MAX; i++)
            {
                wsprintf(buffer[i], L"%s %s_Size_%d_%s_%s", isSingleMode ? L"Single" : L"Multi", PROF_WFUNC, allocSize, kOpStr[i],comment);

                str[i] = buffer[i];
            }
            
            for (int i = 0; i < loopRqstCnt + 1; i++)
            {
                if (i > 0)
                {
                    if (usePool)
                    {
                        // alloc_size에 나온 사이즈중 하나를 랜덤으로 뽑아 할당
                        {
                            PRO_START_AUTO_WNAME(buffer[OP_MY_ALLOC]);
                            for (int j = 0; j < allocFreeRqstCnt; j++)
                            {
                                pointers[j] = { g_pMemSystem->Alloc(allocSize) };
                            }
                        }
                        {
                            PRO_START_AUTO_WNAME(buffer[OP_MY_FREE]);

                            // 역방향 Free >>> NodeStack이니까 최대한 캐시히트를 많이 만들어내자.
                            for (int j = allocFreeRqstCnt - 1; j >= 0; j--)
                            {
                                g_pMemSystem->Free(pointers[j]);

                                // 속도 테스트이기 때문에 초기화 진행하지 않는다. Single에서 이미 테스트를 진행했기 때문.
                                // pointers[j] = nullptr;
                            }
                        }
                    }
                    else
                    {
                        {
                            PRO_START_AUTO_WNAME(buffer[OP_NEW]);
                            for (int j = 0; j < allocFreeRqstCnt; j++)
                            {
                                pointers[j] = ::operator new (allocSize);
                            }
                        }
                        {
                            PRO_START_AUTO_WNAME(buffer[OP_DEL]);
                            for (int j = allocFreeRqstCnt - 1; j >= 0; j--)
                            {
                                ::operator delete(pointers[j]);

                                // 위와 동일한 이유로 제거한다.
                                // pointers[j] = nullptr;
                            }
                        }
                    }
                }
                // 시작하기 전 한 바퀴는 기록하지 않는다.
                else
                {
                    for (int j = 0; j < allocFreeRqstCnt; j++)
                    {
                        if (usePool)
                            pointers[j] = { g_pMemSystem->Alloc(allocSize) };
                        else
                            pointers[j] = ::operator new (allocSize);
                    }

                    for (int j = allocFreeRqstCnt - 1; j >= 0; j--)
                    {
                        if (usePool)
                            g_pMemSystem->Free(pointers[j]);
                        else
                            ::operator delete(pointers[j]);
                    }
                }
            }
        }
    }
    void DefaultAllocTest(int allocRqstCnt, int freeRqstCnt, int loopRqstCnt = kAllocLoopCnt)
    {

        // 테스트항목
        // 1. 할당된 노S드 재할당 테스트
        // 2. 재할당이 가능한 구조인가? 테스트
        // 3. 할당/반복의 횟수가 정확한가? 테스트
        // 4. 요청한 사이즈의 할당/반환이 정확하게 되는가? 테스트
        // idx
        const int maxIdx = GetAllocSizeCnt();

        std::vector<std::pair<void*, int>> pointers(allocRqstCnt, { nullptr,0 });

        // 할당 개수 확인 변수
        std::vector<int> alloc_cnt(maxIdx, 0);

        for (int i = 0; i < loopRqstCnt; i++)
        {

            for (int j = 0; j < allocRqstCnt; j++)
            {
                // alloc_size에 나온 사이즈중 하나를 랜덤으로 뽑아 할당
                int rand_idx = jh::MyRandom::GetRandInt(0, maxIdx - 1);
                pointers[j] = { g_pMemSystem->Alloc(g_kAllocSize[rand_idx]), rand_idx };
                alloc_cnt[rand_idx]++;

                if (false == TestWrite(pointers[j].first, g_kAllocSize[rand_idx]))
                    jh_utility::CrashDump::Crash();

            }

            for (int j = 0; j < freeRqstCnt; j++)
            {
                auto [p, rand_idx] = pointers[j];

                if (false == TestRead(p, g_kAllocSize[rand_idx]))
                    jh_utility::CrashDump::Crash();

                g_pMemSystem->Free(p);
                alloc_cnt[rand_idx]--;

                pointers[j] = {};
            }

            // allocRqstCnt != freeRqstCnt 테스트 (Multi - RandomAllocTest) 정리
            if (allocRqstCnt == freeRqstCnt)
                continue;

            Sleep(0);

            for (int j = freeRqstCnt; j < allocRqstCnt; j++)
            {
                auto [p, rand_idx] = pointers[j];

                if (false == TestRead(p, g_kAllocSize[rand_idx]))
                    jh_utility::CrashDump::Crash();

                g_pMemSystem->Free(p);
                alloc_cnt[rand_idx]--;

                pointers[j] = {};
            }
        }

        for (int i = 0; i < maxIdx; i++)
        {
            if (alloc_cnt[i] != 0)
            {
                printf("[AllocTest]\t\tThread ID : [%5d], vector[%d]'s size : [%5d], cnt : [%5d]\n", GetCurrentThreadId(), i, g_kAllocSize[i], alloc_cnt[i]);
                jh_utility::CrashDump::Crash();
            }
        }

    }
private:
    const int m_iExtraThreadCnt = 4;
};

Mode::~Mode() = default;
class SingleMode : public Mode
{

private:
    ~SingleMode() override = default;
public:
    static SingleMode& GetInstance()
    {
        static SingleMode instance;

        return instance;
    }
    void AllocTest() override
    {
        printf("------------- do_single_thread_test ------------\n");
        const int allocRqstCnt = jh::MyRandom::GetRandInt(g_kRandMin, g_kRandMax);
        const int reallocRqstCnt = jh::MyRandom::GetRandInt(g_kRandMin, g_kRandMax) / 10;

        // alloc, free 횟수가 동일
        printf("------------------ THREAD [%d] AllocTest ------------------\n",GetCurrentThreadId());
        DefaultAllocTest(allocRqstCnt, allocRqstCnt);
        printf("------------------------ [AllocTest]\t\tThread ID : [%5d] End ------------------------\n", GetCurrentThreadId());

        g_pMemSystem->PrintMemoryUsage();

        // 최대 512
        ReallocTest(reallocRqstCnt, 300);
        g_pMemSystem->PrintMemoryUsage();

        g_myPdh.Print();

        NewLine();
    }
    
    // 할당된 메모리 공간을 모두 사용하고, header에 걸리는지 확인
    bool TestWrite(void* p, int rqstSize) override
    {
        if (memcmp(p, &g_sample0, rqstSize))
            return false;

        memset(p, 0xff, rqstSize);

        return true;
    }
    bool TestRead(void* p, int rqstSize) override
    {
        if (memcmp(p, &g_sample1, rqstSize))
            return false;

        memset(p, 0, rqstSize);
        return true;
    }
private:
    void ReallocTest(int outerLoopCnt, const int allocCnt)
    {

        printf("----------------- ReallocTest -----------------\n");
        const int max_idx = GetAllocSizeCnt();

        // 각 size별로 중복체크를 위한 배열
        std::vector<std::vector<void*>> pointers(max_idx, std::vector<void*>(allocCnt, nullptr));

        // 하나의 NodeStack 안에서 제대로 동작하는지 확인 (L1)
        for (int i = 0; i < outerLoopCnt; i++)
        {
            if (i % 400 == 0)
                printf("Thread ID : [%5d] loop_cnt : [%d]\n", GetCurrentThreadId(), i);

            for (int idx = 0; idx < max_idx; idx++)
            {
                for (int curAllocCnt = 0; curAllocCnt < allocCnt; curAllocCnt++)
                {
                    void* p = g_pMemSystem->Alloc(g_kAllocSize[idx]);
                    void*& cur = pointers[idx][curAllocCnt];

                    // 처음의 경우에만 할당, 그 뒤엔 내가 반납한 주소가 그대로 할당되는지 확인한다. (stack구조, L1이 반납되지 않는 선에서)
                    if (nullptr == cur)
                        cur = p;
                    else if (pointers[idx][curAllocCnt] != p)
                        jh_utility::CrashDump::Crash();
                }

                for (int cur_alloc_cnt = allocCnt - 1; cur_alloc_cnt >= 0; cur_alloc_cnt--)
                {
                    g_pMemSystem->Free(pointers[idx][cur_alloc_cnt]);
                }
            }
        }
    }
};

class MultiMode : public Mode
{
private:
    ~MultiMode() override = default;

public :
    static MultiMode& GetInstance()
    {
        static MultiMode instance;
        
        return instance;
    }
    void AllocTest() override
    {
        printf("------------- do_multi_thread_test ------------\n");
        const int threadCnt = GetAvailableProcessors() + GetExtraThreadCnt();
        const int threadGroupCnt = threadCnt * 2;
        const int allocRqstCnt = jh::MyRandom::GetRandInt(g_kRandMin, g_kRandMax);

        for (int i = 0; i < static_cast<int>(FuncType::MAX_ALLOC_TEST_COUNT); i++)
        {
            FuncType funcType = static_cast<FuncType>(i);

            const int half = (30 - strlen(GetFuncTypeStr(funcType))) / 2;
            // 가운데 정렬 출력
            printf("FuncType : [%*s%-*s], request Count : [%d]\n", half, " ", strlen(GetFuncTypeStr(funcType)) + half, GetFuncTypeStr(funcType), allocRqstCnt);
            MultiAllocTest(funcType, allocRqstCnt, threadCnt);

            g_pMemSystem->PrintMemoryUsage();
        }
        g_myPdh.Print();

        NewLine();
    }

    void CompareTest()
    {

        // 테스트 할 항목
        // 싱글/멀티, 고정/랜덤, 풀의 L1 사이즈 미만/초과  << 이 조건들을 이용해 new/delete와 성능을 비교해보자.

        // 멀티스레드 환경에서는 코어 수보다 스레드를 많이 생성하는 경우 / 적게 생성하는 경우로 나누도록 한다.

        enum
        {
            SINGLE_SIZE_BELOW_L1 = 0,
            SINGLE_SIZE_ABOVE_L1 = 1
        };
        // [0] : L1사이즈 미만, [1] : L1 사이즈 초과
        const int singleSizeThreshold[2] = {jh::kNodeCountPerBlock - 3 ,jh::kNodeCountPerBlock * 4};
        
        const int randMultiplier = GetAllocSizeCnt();
        const int singleLoopCnt = 5e4;

        printf("[Compare] - Simple Alloc Test Start (Single) \n");
        {
            WCHAR buffer[2][40]{ {},{} };
            wsprintf(buffer[0], L"AllocBatchSize_[%d]", singleSizeThreshold[SINGLE_SIZE_BELOW_L1]);
            wsprintf(buffer[1], L"AllocBatchSize_[%d]", singleSizeThreshold[SINGLE_SIZE_ABOVE_L1]);

            // 1. 싱글, 고정, L1사이즈 미만
            RecordSimpleAllocTest(singleSizeThreshold[SINGLE_SIZE_BELOW_L1], true, true, singleLoopCnt, buffer[SINGLE_SIZE_BELOW_L1]);
            RecordSimpleAllocTest(singleSizeThreshold[SINGLE_SIZE_BELOW_L1], true, false, singleLoopCnt, buffer[SINGLE_SIZE_BELOW_L1]);

            // 2. 싱글, 고정, L1사이즈 초과
            RecordSimpleAllocTest(singleSizeThreshold[SINGLE_SIZE_ABOVE_L1], true, true, singleLoopCnt, buffer[SINGLE_SIZE_ABOVE_L1]);
            RecordSimpleAllocTest(singleSizeThreshold[SINGLE_SIZE_ABOVE_L1], true, false, singleLoopCnt, buffer[SINGLE_SIZE_ABOVE_L1]);

            printf("[Compare] - Simple Alloc Test End (Single)\n");
            printf("[Compare] - Random Alloc Test Start (Single)\n");
            // 3. 싱글, 랜덤, 풀의 L1 사이즈 미만
            RecordRandomAllocTest(singleSizeThreshold[SINGLE_SIZE_BELOW_L1] * randMultiplier, true, true, singleLoopCnt, buffer[SINGLE_SIZE_BELOW_L1]);
            RecordRandomAllocTest(singleSizeThreshold[SINGLE_SIZE_BELOW_L1] * randMultiplier, true, false, singleLoopCnt, buffer[SINGLE_SIZE_BELOW_L1]);

            // 4. 싱글, 랜덤, 풀의 L1 사이즈 초과
            RecordRandomAllocTest(singleSizeThreshold[SINGLE_SIZE_ABOVE_L1] * randMultiplier, true, true, singleLoopCnt, buffer[SINGLE_SIZE_ABOVE_L1]);
            RecordRandomAllocTest(singleSizeThreshold[SINGLE_SIZE_ABOVE_L1] * randMultiplier, true, false, singleLoopCnt, buffer[SINGLE_SIZE_ABOVE_L1]);
        }

        printf("[Compare] - Random Alloc Test End (Single)\n");
        ParamData data{};

        // 스레드의 개수를 코어의 수보다 적게 / 많게 하는 경우 2가지
        int threadCnt[2] = { GetAvailableProcessors() / 2 + 1, GetAvailableProcessors() + 4 };

        data.m_iLoopCnt = singleLoopCnt;
        data.m_pThis = this;

        for (int i = 0; i < 2; i++)
        {
            const int tCnt = threadCnt[i];
            printf("[Compare] - Thread %d Test Start (Multi)\n", tCnt);
            data.m_pProdData = reinterpret_cast<ProdData*>(tCnt);
            // RunThreads는 항상 여러 스레드 생성 -> 함수 호출 후 모든 스레드가 종료될때까지 대기한다. 순차적 실행

            // 지금 코드에서는 하나의 항목에 대해 Pool / New를 실행하는 것이 아니라
            // Pool / New에 대해 여러 항목을 쭉 실행한다.
            for (int j = static_cast<int>(FuncType::RECORD_POOL_TEST); j < static_cast<int>(FuncType::MAX_ALL_TEST_CNT); j++)
            {
                // 할당 함수를 Pool / New 중 선택
                data.m_funcType = static_cast<FuncType>(j);

                // 할당 횟수 - L1 사이즈 미만으로 변경
                
                // L1 사이즈 미만 고정
                data.m_iTargetCnt = singleSizeThreshold[SINGLE_SIZE_BELOW_L1];
                data.m_role = static_cast<Role>(Pattern::SIMPLE);
                RunThreads(tCnt, &data, true);

                // L1 사이즈 미만 랜덤
                data.m_iTargetCnt = singleSizeThreshold[SINGLE_SIZE_BELOW_L1] * GetAllocSizeCnt();
                data.m_role = static_cast<Role>(Pattern::RANDOM);
                RunThreads(tCnt, &data, true);           
                
                // --------------------------------------------------------------------------------
                
                // 할당 횟수 - L1 사이즈 초과로 변경
               
                // L1 사이즈 초과 고정
                data.m_iTargetCnt = singleSizeThreshold[SINGLE_SIZE_ABOVE_L1];
                data.m_role = static_cast<Role>(Pattern::SIMPLE);
                RunThreads(tCnt, &data, true);
                // L1 사이즈 초과 랜덤

                // L1 사이즈 미만 랜덤
                data.m_iTargetCnt = singleSizeThreshold[SINGLE_SIZE_ABOVE_L1] * GetAllocSizeCnt();
                data.m_role = static_cast<Role>(Pattern::RANDOM);
                RunThreads(tCnt, &data, true);

            }

            printf("[Compare] - Thread %d Test End (Multi)\n", tCnt);

        }
    }
private:
    enum class Role
    {
        PRODUCER = 0,
        CONSUMER,
    };
    enum class Pattern
    {
        SIMPLE,
        RANDOM,
    };
    enum class FuncType
    {
        DEFAULT_ALLOC_TEST = 0,

        // 2 스레드 = 1그룹으로 나눠 여러 그룹으로 테스트한다.
        // 그룹의 공용 큐에 존재하는 주소에 대해 한 스레드는 할당만 하고 다른 스레드는 반환만 한다.
        // 작업이 끝나고 모든 요소를 반환했을 때 오차가 존재하는지 확인한다.
        PRODUCER_CONSUMER_TEST,

        // 스레드마다 할당/해제의 횟수를 다르게 설정해 테스트한다.
        RANDOM_ALLOC_TEST,
        
        MAX_ALLOC_TEST_COUNT,

        // new delete 비교 테스트, 멀티스레드 환경의 여러 스레드에서 동시 실행을 위해 설정.
        RECORD_POOL_TEST,
        RECORD_NEW_TEST,

        MAX_ALL_TEST_CNT,
        
    };
    const char* GetFuncTypeStr(FuncType funcType)
    {
        switch (funcType)
        {
        case FuncType::DEFAULT_ALLOC_TEST: return "DEFAULT_ALLOC_TEST";
        case FuncType::PRODUCER_CONSUMER_TEST: return "PRODUCER_CONSUMER_TEST";
        case FuncType::RANDOM_ALLOC_TEST: return "RANDOM_ALLOC_TEST";
        default: return "";
        }
    }
    struct ProdData
    {
        // 생산자 소비자 스레드 작동을 위해
        LockVector<void*> m_lockVec;
        CONDITION_VARIABLE m_cvProduced;
        CONDITION_VARIABLE m_cvConsumed;
        char m_bProduced;
        char m_bConsumed;
    };
    struct ParamData
    {
        FuncType m_funcType;    // 실행 함수 번호
        int m_iTargetCnt;   // 할당 반복 횟수
        int m_iLoopCnt;     // 전체 반복 횟수
        Role m_role;        // 생산자 / 소비자 역할, Compare Test의 경우 이 항목에 Simple / Random 을 넣는다.
        
        ProdData* m_pProdData; // 생산자 소비자 작업이 아닌 Compare Test의 경우 이 항목에 스레드 개수를 넣는다.
        MultiMode* m_pThis;
    };

    static unsigned WINAPI DoMulti(LPVOID lparam)
    {
        // 요소를 복사한다.
        ParamData params = *reinterpret_cast<ParamData*>(lparam);
        WCHAR buffer[50]{};

        WaitForSingleObject(g_hStarterHandle, INFINITE);

        switch (params.m_funcType)
        {
        case FuncType::DEFAULT_ALLOC_TEST:
        {        
            printf("------------------ THREAD [%d] Multi - AllocTest ------------------\n", GetCurrentThreadId());
            params.m_pThis->DefaultAllocTest(params.m_iTargetCnt, params.m_iTargetCnt, params.m_iLoopCnt); break;
            printf("------------------------ [Multi - AllocTest]\t\tThread ID : [%5d] End ------------------------\n", GetCurrentThreadId());
        }
        case FuncType::PRODUCER_CONSUMER_TEST:params.m_pThis->ProducerConsumerTest(params.m_iTargetCnt, params.m_role, params.m_pProdData, params.m_iLoopCnt); break;
        case FuncType::RANDOM_ALLOC_TEST: params.m_pThis->RandomAllocTest(params.m_iLoopCnt);  break;
        case FuncType::RECORD_POOL_TEST:
        {
            int threadCnt = reinterpret_cast<int>(params.m_pProdData);
            wsprintf(buffer, L"Thread_Cnt_[%d]_AllocBatchSize_[%d]", threadCnt, params.m_iTargetCnt);

            if (Pattern::SIMPLE == static_cast<Pattern>(params.m_role))
            {
                params.m_pThis->RecordSimpleAllocTest(params.m_iTargetCnt, false, true, params.m_iLoopCnt, buffer); break;
            }
            else
            {
                params.m_pThis->RecordRandomAllocTest(params.m_iTargetCnt, false, true, params.m_iLoopCnt, buffer); break;
            }
        }
        case FuncType::RECORD_NEW_TEST:
        {
            int threadCnt = reinterpret_cast<int>(params.m_pProdData);

            wsprintf(buffer, L"Thread_Cnt_[%d]_AllocBatchSize_[%d]", threadCnt, params.m_iTargetCnt);

            if (Pattern::SIMPLE == static_cast<Pattern>(params.m_role))
            {
                params.m_pThis->RecordSimpleAllocTest(params.m_iTargetCnt, false, false, params.m_iLoopCnt, buffer); break;
            }
            else
            {
                params.m_pThis->RecordRandomAllocTest(params.m_iTargetCnt, false, false, params.m_iLoopCnt, buffer); break;
            }
        }
        }

        return 0;
    }

    void RunThreads(int threadCnt, ParamData* data, bool isSimpleData)
    {
        std::vector<HANDLE> threads(threadCnt, nullptr);

        for (int i = 0; i < threadCnt; i++)
        {
            threads[i] = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, &MultiMode::DoMulti,
                isSimpleData ? static_cast<LPVOID>(data) : reinterpret_cast<LPVOID>(&data[i]), 0, nullptr));

            ChkHandle(threads[i]);
        }

        // 한 번에 시작!
        SetEvent(g_hStarterHandle);

        DWORD ret = WaitForMultipleObjects(threadCnt, threads.data(), true, INFINITE);

        if (ret >= WAIT_OBJECT_0 && ret < WAIT_OBJECT_0 + threadCnt)
        {
            printf("--------------- multi_thread_test_end ---------------\n");

            for (int i = 0; i < threadCnt; i++)
            {
                ClearHandle(threads[i]);
            }

            ResetEvent(g_hStarterHandle);
        }
        else
        {
            jh_utility::CrashDump::Crash();
        }

    }
    void ProducerConsumerTest(int targetCnt, Role role, ProdData* prodData, int loopRqstCnt)
    {
        // yet    
        printf("------------- ProducerConsumerTest..... ------------\n");
        const int max_idx = GetAllocSizeCnt();

  
        std::vector<void*>& vec = prodData->m_lockVec.m_vec;
        PSRWLOCK srwlock = &prodData->m_lockVec.m_lock;

        for (int i = 0; i < loopRqstCnt; i++)
        {
            if (i % 400 == 0)
                printf("%s Thread ID : [%5d], loop_cnt : [%d]\n", role == Role::PRODUCER ? "Producer"   : "Consumer", GetCurrentThreadId(), i);
           
            SRWLockGuard lockGuard(srwlock);

            // Producer는 consumer가 소비한 후 제공하도록 해야한다.
            if (Role::PRODUCER == role)
            {
                // bConsumed 0 -> 0 이면 아직 처리되지 않았음.
                // bConsumed 1 -> 0 이면 consumer가 처리 -> for문으로 진입한다.
                while (InterlockedExchange8(&prodData->m_bConsumed, 0) == 0)
                {
                    // cv : lock 해제 후 consumed 활성화 될때까지 대기 -> consumed 활성화되면 깨어났을때 lock 획득
                    SleepConditionVariableSRW(&prodData->m_cvConsumed, srwlock, INFINITE, 0);
                }

                // alloc_size에 나온 사이즈중 하나를 랜덤으로 뽑아 할당
                for (int j = 0; j < targetCnt; j++)
                {
                    int rand_idx = jh::MyRandom::GetRandInt(0, max_idx - 1);
                    vec[j] = g_pMemSystem->Alloc(g_kAllocSize[rand_idx]);

                    if (false == TestWrite(vec[j]))
                        jh_utility::CrashDump::Crash();    
                }
                
                InterlockedExchange8(&prodData->m_bProduced, 1);
                WakeConditionVariable(&prodData->m_cvProduced);

            }
            else // consumer는 Producer가 제공을 한 후에 소비하도록 해야한다.
            {
                // bConsumed 0 -> 0 이면 아직 처리되지 않았음.
                // bConsumed 1 -> 0 이면 consumer가 처리 -> for문으로 진입한다.
                while (InterlockedExchange8(&prodData->m_bProduced, 0) == 0)
                {
                    // cv : lock 해제 후 consumed 활성화 될때까지 대기 -> consumed 활성화되면 깨어났을때 lock 획득
                    SleepConditionVariableSRW(&prodData->m_cvProduced, srwlock, INFINITE, 0);
                }

                for (int j = 0; j < targetCnt; j++)
                {
                    if (false == TestRead(vec[j]))
                        jh_utility::CrashDump::Crash();

                    g_pMemSystem->Free(vec[j]);

                    vec[j] = nullptr;
                }

                InterlockedExchange8(&prodData->m_bConsumed, 1);
                WakeConditionVariable(&prodData->m_cvConsumed);
            }
        }
    }

    // Alloc, Free의 횟수를 다르게 설정한다.
    void RandomAllocTest(int loopRqstCnt)
    {
        //printf("THREAD ID : [%5d], allocCnt : [%5d], freeCnt : [%5d]\n", GetCurrentThreadId(), allocRqstCnt, freeRqstCnt);

        printf("------------------ THREAD [%d] RandomAllocTest ------------------\n",GetCurrentThreadId());

        for (int i = 0; i < loopRqstCnt; i++)
        {
            const int allocRqstCnt = jh::MyRandom::GetRandInt(g_kRandMin, g_kRandMax);
            const int freeRqstCnt = jh::MyRandom::GetRandInt(3000, allocRqstCnt);

            DefaultAllocTest(allocRqstCnt, freeRqstCnt, 1);
        }
        
        printf("------------------------ [RandomAllocTest]\t\tThread ID : [%5d] End ------------------------\n", GetCurrentThreadId());

    }
    void MultiAllocTest(FuncType funcType, int targetCnt, int threadCnt)
    {
        const int groupCnt = threadCnt / 2 + 1;
        ParamData* data = new ParamData[threadCnt];

        ProdData* prodData = nullptr;
        if (FuncType::PRODUCER_CONSUMER_TEST == funcType)
        {
            prodData = new ProdData[groupCnt];

            for(int i =0;i<groupCnt;i++)
            {
                prodData[i].m_lockVec.m_vec.resize(targetCnt, nullptr);

                InitializeConditionVariable(&prodData[i].m_cvProduced);
                InitializeConditionVariable(&prodData[i].m_cvConsumed);

                prodData[i].m_bProduced = 0;
                
                // 생산자 start를 위해 설정
                prodData[i].m_bConsumed = 1;
            }
            for (int i = 0; i < threadCnt; i++)
            {
                const int groupNum = i / 2;
                data[i].m_funcType = funcType;
                data[i].m_role = static_cast<Role>(static_cast<int>(i) % 2);
                data[i].m_iTargetCnt = targetCnt;
                data[i].m_iLoopCnt = kAllocLoopCnt;
                data[i].m_pThis = this;
                data[i].m_pProdData = prodData == nullptr ? nullptr : &prodData[groupNum];
            }
             // Producer - Consumer Test
            RunThreads(threadCnt, data, false);
            delete[] prodData;
        }    
        else if(FuncType::RANDOM_ALLOC_TEST == funcType)
        {
            for (int i = 0; i < threadCnt; i++)
            {
                data[i].m_funcType = funcType;
                data[i].m_pThis = this;
                data[i].m_iLoopCnt = kAllocLoopCnt;
            }

            // Random Alloc Test
            RunThreads(threadCnt, data, false);
        }
        else if (FuncType::DEFAULT_ALLOC_TEST == funcType)
        {
            for (int i = 0; i < threadCnt; i++)
            {
                data[i].m_funcType = funcType;
                data[i].m_pThis = this;
                data[i].m_iLoopCnt = kAllocLoopCnt;
                data[i].m_iTargetCnt = targetCnt;
            }
            RunThreads(threadCnt, data, false);
        }
        delete[] data;

    }
    // 멀티 스레드 환경에서 중복된 영역으로 할당이 되는지 확인
    bool TestWrite(void* p, int rqstSize = 0) override
    {
        return InterlockedIncrement(static_cast<LONG*>(p)) == 1;
    }
    bool TestRead(void* p, int rqstSize = 0) override
    {
        return InterlockedDecrement(static_cast<LONG*>(p)) == 0;
    }
};
















int main(int argc, char* argv[])
{
    // 무엇을 테스트할까 => 중복없이 할당이 제대로 이루어지는가 ? 를 봐야한다. 메모리 할당량이 늘어나지는 않는가?
    // 1. 풀에서 alloc 했을 때 이미 할당했던 노드를 재할당하지 않는가?
    // 2. 풀에 free했을 때 제대로 반환이 되는가? (재사용이 가능하도록)
    // 3. 할당과 해제를 수없이 반복했을 때 할당/반환의 횟수에 오차가 있지는 않나?
    // 4. 내가 요청한 사이즈가 제대로 할당/반환이 되는가? + 앞에 attach된 사이즈가 깨지지는 않는가?도 체크
    // 
    // 5. 멀티스레드 환경에서도 위의 사항이 제대로 이루어지는가? (연속적인 주소가 아닌 무작위로 할당/해제했을때도) 
    // >>> 서로 다른 스레드에서 할당 후 무작위 스레드에 반환된는 상황에서도 제대로 동작하는가?
    // 
    // 6. 모든 것이 잘 동작한다면, 그것의 속도는 new/delete와 같은 기존의 방식에 비교해서 어떠한 장점이 있는가??

    InitMain();
    
    printf("저장할 파일 명을 입력하세요 : ");

    WCHAR buffer[100]{};

    fgetws(buffer, sizeof(buffer) / sizeof(buffer[0]), stdin);
    // \n 제거
    buffer[wcslen(buffer) - 1] = 0;
    wcscat_s(buffer, L".txt");

    bool end_flag = false;
    while (false == end_flag)
    {
        int user_input = SelectMenu();

        switch (user_input)
        {
            case 0: end_flag = true; break;
            case 1: SingleMode::GetInstance().AllocTest();  break;
            case 2: MultiMode::GetInstance().AllocTest();break;
            case 3: MultiMode::GetInstance().CompareTest();  break;
            default: printf("올바르지 않은 입력입니다.\n"); break;
        }
    }

    PRO_WSAVE(buffer);
    
    ClearMain();
    printf("--------------- 종료합니다 ---------------\n");

    return 0;
}