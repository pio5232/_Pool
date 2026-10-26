#include "pch.h"
#include <algorithm>
#include <numeric>
#include <bitset>
#include <format>
#include <iostream>
#include <conio.h>
#include "Rand.h"
#include "SRWLockGuard.h"

const int g_iRandMin = 1e4;
const int g_iRandMax = 1e5;

const int g_iAllocLoopCnt = 2e3;
const int g_iAllocSize[]{ 4,40,400, 1000 };

const unsigned int g_iTestMax = 0xff'ff'ff'ff;

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


inline int GetMaxAllocSize()
{
    return sizeof(g_iAllocSize) / sizeof(g_iAllocSize[0]);
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
    void DefaultAllocTest(int targetCnt, int threadCnt = 1)
    {
        printf("------------------ AllocTest ------------------\n");

        // 테스트항목
        // 1. 할당된 노S드 재할당 테스트
        // 2. 재할당이 가능한 구조인가? 테스트
        // 3. 할당/반복의 횟수가 정확한가? 테스트
        // 4. 요청한 사이즈의 할당/반환이 정확하게 되는가? 테스트
        // idx
        const int max_idx = GetMaxAllocSize();

        std::vector<std::pair<void*, int>> pointers(targetCnt, { nullptr,0 });

        // 할당 개수 확인 변수
        std::vector<int> alloc_cnt(max_idx, 0);

        for (int i = 0; i < g_iAllocLoopCnt; i++)
        {
            if (i % 400 == 0)
                printf("Thread ID : [%5d] loop_cnt : [%d]\n", GetCurrentThreadId(), i);

            for (int j = 0; j < targetCnt; j++)
            {
                // alloc_size에 나온 사이즈중 하나를 랜덤으로 뽑아 할당
                int rand_idx = jh::MyRandom::GetRandInt(0, max_idx - 1);
                pointers[j] = { g_pMemSystem->Alloc(g_iAllocSize[rand_idx]), rand_idx };
                alloc_cnt[rand_idx]++;

                if (false == TestWrite(pointers[j].first, g_iAllocSize[rand_idx]))
                    jh_utility::CrashDump::Crash();

            }

            for (int j = 0; j < targetCnt; j++)
            {
                auto [p, rand_idx] = pointers[j];

                if (false == TestRead(p, g_iAllocSize[rand_idx]))
                    jh_utility::CrashDump::Crash();

                g_pMemSystem->Free(p);
                alloc_cnt[rand_idx]--;

                pointers[j] = {};
            }
        }

        for (int i = 0; i < max_idx; i++)
        {
            printf("\t\t[AllocTest] size : %d, cnt : %d\n", g_iAllocSize[i], alloc_cnt[i]);

            if (alloc_cnt[i] != 0)
                jh_utility::CrashDump::Crash();
        }
    }
private:
    const int m_iExtraThreadCnt = 0;
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
        const int alloc_rqst_cnt = jh::MyRandom::GetRandInt(g_iRandMin, g_iRandMax);
        const int realloc_rast_cnt = jh::MyRandom::GetRandInt(g_iRandMin, g_iRandMax) / 10;

        DefaultAllocTest(alloc_rqst_cnt);
        g_pMemSystem->PrintMemoryUsage();

        // 최대 512
        ReallocTest(realloc_rast_cnt, 300);
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
        const int max_idx = GetMaxAllocSize();

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
                    void* p = g_pMemSystem->Alloc(g_iAllocSize[idx]);
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
        const int allocRqstCnt = jh::MyRandom::GetRandInt(g_iRandMin, g_iRandMax) / 2;

        for (int i = 1; i < static_cast<int>(FuncType::MAX_COUNT); i++)
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

    void CompareTest() {}
private:
    enum class Role
    {
        PRODUCER = 0,
        CONSUMER,
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
        MAX_COUNT
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
        int m_iTargetCnt;   // 반복 횟수
        Role m_role;        // 생산자 / 소비자 역할
        
        ProdData* m_pProdData;
        MultiMode* m_pThis;
    };
    static unsigned WINAPI DoMulti(LPVOID lparam)
    {
        // 요소를 복사한다.
        ParamData params = *reinterpret_cast<ParamData*>(lparam);

        WaitForSingleObject(g_hStarterHandle, INFINITE);

        switch (params.m_funcType)
        {
        case FuncType::DEFAULT_ALLOC_TEST: params.m_pThis->DefaultAllocTest(params.m_iTargetCnt);break;
        case FuncType::PRODUCER_CONSUMER_TEST:params.m_pThis->ProducerConsumerTest(params.m_iTargetCnt, params.m_role, params.m_pProdData); break;
        case FuncType::RANDOM_ALLOC_TEST: break;
        }

        return 0;
    }
    void ProducerConsumerTest(int targetCnt, Role role, ProdData* prodData)
    {
        // yet    
        printf("------------- RandomAllocTest..... ------------\n");
        const int max_idx = GetMaxAllocSize();

  
        std::vector<void*>& vec = prodData->m_lockVec.m_vec;
        PSRWLOCK srwlock = &prodData->m_lockVec.m_lock;

        for (int i = 0; i < g_iAllocLoopCnt; i++)
        {
            if (i % 400 == 0)
                printf("%s Thread ID : [%5d], loop_cnt : [%d]\n", role == Role::PRODUCER ? "Producer" : "Consumer", GetCurrentThreadId(), i);
           
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
                    vec[j] = g_pMemSystem->Alloc(g_iAllocSize[rand_idx]);

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

    void RandomAllocTest(int targetCnt, int threadCnt)
    {

    }
    void MultiAllocTest(FuncType funcType, int targetCnt, int threadCnt)
    {
        std::vector<HANDLE> threads(threadCnt, nullptr);
        const int groupCnt = threadCnt / 2 + 1;
        ParamData* data = new ParamData[threadCnt];

        ProdData* prodData = nullptr;
        if (funcType == FuncType::PRODUCER_CONSUMER_TEST)
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
        }
        for (int i = 0; i < threadCnt; i++)
        {
            const int groupNum = i / 2;
            data[i].m_funcType = funcType;
            data[i].m_role = static_cast<Role>(static_cast<int>(i) % 2);
            data[i].m_iTargetCnt = targetCnt;
            data[i].m_pThis = this;

            data[i].m_pProdData = prodData == nullptr ? nullptr : &prodData[groupNum];
        }
    

        for (int i = 0; i < threadCnt; i++)
        {
            threads[i] = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, &MultiMode::DoMulti, reinterpret_cast<LPVOID>(&data[i]), 0, nullptr));

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

        delete[] prodData;
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

    PRO_SAVE("MemoryTest.txt");

    ClearMain();
    printf("--------------- 종료합니다 ---------------\n");

    return 0;
}
