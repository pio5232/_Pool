#include "pch.h"
#include <algorithm>
#include <numeric>
#include <bitset>
#include <format>
#include <iostream>
#include <conio.h>
#include "Rand.h"
#include "SRWLockGuard.h"

const int rand_min = 1e4;
const int rand_max = 1e5;

const int alloc_loop_cnt = 2e3;
const int alloc_size[]{ 4,40,400, 1000 };

const unsigned int test_max = 0xff'ff'ff'ff;

struct test_sample
{
    char a[4096];// 할당의 최대 크기느 4096보다 작음, 메모리0 비교를 위한 값
} g_0_sample, g_1_sample;

HANDLE g_starter_handle;
jh::PerformanceDataInfo my_pdh;

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
    SYSTEM_INFO sys_info;
    GetSystemInfo(&sys_info);

    return sys_info.dwNumberOfProcessors;
}
void InitMain()
{
    memset(&g_1_sample, 0xff, sizeof(g_1_sample));

    g_starter_handle = CreateEvent(nullptr, true, false, nullptr);
    ChkHandle(g_starter_handle);
}

void ClearMain()
{
    ClearHandle(g_starter_handle);
}


inline int GetMaxAllocSize()
{
    return sizeof(alloc_size) / sizeof(alloc_size[0]);
}
inline void NewLine() {
    printf("\n\n\n\n\n\n\n");
}
int SelectMenu()
{
    const int width = 50;
    const char* menu[]{ "---------------메뉴---------------", "1. 단일 스레드 테스트", "2. 멀티 스레드 테스트", "3. new/delete 비교 테스트", "종료 : 0" };

    printf("%*s%s\n", (width - strlen(menu[0])) / 2, " ", menu[0]);

    for (int i = 1; i < sizeof(menu) / sizeof(menu[0]); i++)
    {
        printf("%s\n", menu[i]);
    }

    printf("\n메뉴 번호를 입력해주세요 (0은 종료) : ");

    int menu_num;
    scanf_s("%d", &menu_num);

    return menu_num;
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
    int GetExtraThreadCnt() { return m_extra_thread_cnt; }

private:
    virtual bool TestWrite(void* p, int rqst_size) = 0;
    virtual bool TestRead(void* p, int rqst_size) = 0;

protected:
    void DefaultAllocTest(int target_cnt, int thread_cnt = 1)
    {
        printf("------------------ AllocTest ------------------\n");

        // 테스트항목
        // 1. 할당된 노S드 재할당 테스트
        // 2. 재할당이 가능한 구조인가? 테스트
        // 3. 할당/반복의 횟수가 정확한가? 테스트
        // 4. 요청한 사이즈의 할당/반환이 정확하게 되는가? 테스트
        // idx
        const int max_idx = GetMaxAllocSize();

        std::vector<std::pair<void*, int>> pointers(target_cnt, { nullptr,0 });

        // 할당 개수 확인 변수
        std::vector<int> alloc_cnt(max_idx, 0);

        for (int i = 0; i < alloc_loop_cnt; i++)
        {
            if (i % 400 == 0)
                printf("Thread ID : [%5d] loop_cnt : [%d]\n", GetCurrentThreadId(), i);

            for (int j = 0; j < target_cnt; j++)
            {
                // alloc_size에 나온 사이즈중 하나를 랜덤으로 뽑아 할당
                int rand_idx = jh::MyRandom::GetRandInt(0, max_idx - 1);
                pointers[j] = { g_pMemSystem->Alloc(alloc_size[rand_idx]), rand_idx };
                alloc_cnt[rand_idx]++;

                if (false == TestWrite(pointers[j].first, alloc_size[rand_idx]))
                    jh_utility::CrashDump::Crash();

            }

            for (int j = 0; j < target_cnt; j++)
            {
                auto [p, rand_idx] = pointers[j];

                if (false == TestRead(p, alloc_size[rand_idx]))
                    jh_utility::CrashDump::Crash();

                g_pMemSystem->Free(p);
                alloc_cnt[rand_idx]--;

                pointers[j] = {};
            }
        }

        for (int i = 0; i < max_idx; i++)
        {
            printf("\t\t[AllocTest] size : %d, cnt : %d\n", alloc_size[i], alloc_cnt[i]);

            if (alloc_cnt[i] != 0)
                jh_utility::CrashDump::Crash();
        }
    }
private:
    const int m_extra_thread_cnt = 0;
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
        const int alloc_rqst_cnt = jh::MyRandom::GetRandInt(rand_min, rand_max);
        const int realloc_rast_cnt = jh::MyRandom::GetRandInt(rand_min, rand_max) / 10;

        DefaultAllocTest(alloc_rqst_cnt);
        g_pMemSystem->PrintMemoryUsage();

        // 최대 512
        ReallocTest(realloc_rast_cnt, 300);
        g_pMemSystem->PrintMemoryUsage();

        my_pdh.Print();

        NewLine();
    }
    
    // 할당된 메모리 공간을 모두 사용하고, header에 걸리는지 확인
    bool TestWrite(void* p, int rqst_size) override
    {
        if (memcmp(p, &g_0_sample, rqst_size))
            return false;

        memset(p, 0xff, rqst_size);

        return true;
    }
    bool TestRead(void* p, int rqst_size) override
    {
        if (memcmp(p, &g_1_sample, rqst_size))
            return false;

        memset(p, 0, rqst_size);
        return true;
    }
private:
    void ReallocTest(int outer_loop_cnt, const int alloc_cnt)
    {

        printf("----------------- ReallocTest -----------------\n");
        const int max_idx = GetMaxAllocSize();

        // 각 size별로 중복체크를 위한 배열
        std::vector<std::vector<void*>> pointers(max_idx, std::vector<void*>(alloc_cnt, nullptr));

        // 하나의 NodeStack 안에서 제대로 동작하는지 확인 (L1)
        for (int i = 0; i < outer_loop_cnt; i++)
        {
            if (i % 400 == 0)
                printf("Thread ID : [%5d] loop_cnt : [%d]\n", GetCurrentThreadId(), i);

            for (int idx = 0; idx < max_idx; idx++)
            {
                for (int cur_alloc_cnt = 0; cur_alloc_cnt < alloc_cnt; cur_alloc_cnt++)
                {
                    void* p = g_pMemSystem->Alloc(alloc_size[idx]);
                    void*& cur = pointers[idx][cur_alloc_cnt];

                    // 처음의 경우에만 할당, 그 뒤엔 내가 반납한 주소가 그대로 할당되는지 확인한다. (stack구조, L1이 반납되지 않는 선에서)
                    if (nullptr == cur)
                        cur = p;
                    else if (pointers[idx][cur_alloc_cnt] != p)
                        jh_utility::CrashDump::Crash();
                }

                for (int cur_alloc_cnt = alloc_cnt - 1; cur_alloc_cnt >= 0; cur_alloc_cnt--)
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
        const int thread_cnt = GetAvailableProcessors() + GetExtraThreadCnt();
        const int thread_group_cnt = thread_cnt * 2;
        const int alloc_rqst_cnt = jh::MyRandom::GetRandInt(rand_min, rand_max) / 2;
        // const int realloc_rast_cnt = jh::MyRandom::GetRandInt(rand_min, rand_max) / 10;

        printf("Multi : rqst_size : %d\n", alloc_rqst_cnt);
        MultiAllocTest(alloc_rqst_cnt, thread_cnt);
        g_pMemSystem->PrintMemoryUsage();

        // 2 스레드 = 1그룹으로 나눠 여러 그룹으로 테스트한다.
        // 그룹의 공용 큐에 존재하는 주소에 대해 한 스레드는 할당만 하고 다른 스레드는 반환만 한다.
        // 작업이 끝나고 모든 요소를 반환했을 때 오차가 존재하는지 확인한다.
        printf("Multi : realloc_rqst_cnt : %d\n", alloc_rqst_cnt);
        ProducerConsumerTest(alloc_rqst_cnt, thread_group_cnt);

        // 스레드마다 할당/해제의 횟수를 다르게 설정해 테스트한다.
        RandomAllocTest(alloc_rqst_cnt, thread_cnt);
        g_pMemSystem->PrintMemoryUsage();

        my_pdh.Print();

        NewLine();
    }

    void CompareTest() {}
private:
    enum class FuncType
    {
        DEFAULT_ALLOC_TEST = 0,
        PRODUCER_CONSUMER_TEST,
        RANDOM_ALLOC_TEST,
    };
    template <typename T>
    struct param_data
    {
        FuncType m_funcType;    // 실행 함수 번호
        int m_iTargetCnt;   // 반복 횟수
        int m_iRole;        // 생산자 / 소비자 역할
        
        LockVector<T>* m_pLockVec;
        MultiMode* m_pThis;
    };
    static unsigned WINAPI DoMulti(LPVOID lparam)
    {
        // 빠르게 복사한다.
        param_data<void> params = *reinterpret_cast<param_data<void>*>(lparam);

        WaitForSingleObject(g_starter_handle, INFINITE);

        switch (params.m_funcType)
        {
        case FuncType::DEFAULT_ALLOC_TEST: params.m_pThis->DefaultAllocTest(params.m_iTargetCnt);break;
        case FuncType::PRODUCER_CONSUMER_TEST:params.m_pThis->ProducerConsumerTest(params.m_iTargetCnt, params.thread break; 
        case FuncType::RANDOM_ALLOC_TEST: break;
        }

        return 0;
    }
    void ProducerConsumerTest(int target_cnt, int thread_cnt)
    {
        // yet    
        printf("------------- RandomAllocTest..... ------------\n");

    }

    void RandomAllocTest(int target_cnt, int thread_cnt)
    {

    }
    void MultiAllocTest(FuncType funcType, int target_cnt, int thread_cnt)
    {
        std::vector<HANDLE> threads(thread_cnt, nullptr);

        param_data<void>* data = new param_data<void>[thread_cnt];
        LockVector
        for (int i = 0; i < thread_cnt; i++)
        {
            data[i].m_funcType = funcType;
            data[i].m_iRole = i % 2;
            data[i].m_iTargetCnt = target_cnt;
            data[i].m_pThis = this;
        }
    

        for (int i = 0; i < thread_cnt; i++)
        {
            threads[i] = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, &MultiMode::DoMulti, reinterpret_cast<LPVOID>(&data), 0, nullptr));

            ChkHandle(threads[i]);
        }

        // 한 번에 시작!
        SetEvent(g_starter_handle);

        DWORD ret = WaitForMultipleObjects(thread_cnt, threads.data(), true, INFINITE);

        if (ret >= WAIT_OBJECT_0 && ret < WAIT_OBJECT_0 + thread_cnt)
        {
            printf("--------------- multi_thread_test_end ---------------\n");

            for (int i = 0; i < thread_cnt; i++)
            {
                ClearHandle(threads[i]);
            }

            ResetEvent(g_starter_handle);
        }
        else
        {
            jh_utility::CrashDump::Crash();
        }

    }
    // 멀티 스레드 환경에서 중복된 영역으로 할당이 되는지 확인
    bool TestWrite(void* p, int rqst_size) override
    {
        return InterlockedIncrement(static_cast<LONG*>(p)) == 1;
    }
    bool TestRead(void* p, int rqst_size) override
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
