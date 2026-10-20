#include "pch.h"
#include <algorithm>
#include <numeric>
#include <bitset>
#include <format>
#include <iostream>
#include <conio.h>
#include "Rand.h"

const int rand_min = 1e4;
const int rand_max = 1e5;

const int alloc_loop_cnt = 2e3;
const int alloc_size[]{ 4,40,400, 1000 };

const int ex_thread_cnt = 0;

HANDLE g_starter_handle;
jh::PerformanceDataInfo my_pdh;

inline void chk_handle(HANDLE handle)
{
    if (nullptr == handle)
        jh_utility::CrashDump::Crash();
}

inline void clear_handle(HANDLE handle)
{
    if (nullptr != handle)
    {
        CloseHandle(handle);
        handle = nullptr;
    }
}
void init_main()
{
    g_starter_handle = CreateEvent(nullptr, true, false, nullptr);
    chk_handle(g_starter_handle);
}

void clear_main()
{
    clear_handle(g_starter_handle);
}
int get_available_processors()
{
    SYSTEM_INFO sys_info;
    GetSystemInfo(&sys_info);

    return sys_info.dwNumberOfProcessors;
}
inline bool test_write(void* p)
{
    return InterlockedIncrement(static_cast<LONG*>(p)) == 1;
}

inline bool test_read(void* p)
{
    return InterlockedDecrement(static_cast<LONG*>(p)) == 0;
}
inline int get_max_alloc_size()
{
    return sizeof(alloc_size) / sizeof(alloc_size[0]);
}
inline void new_line() {
    printf("\n\n\n\n\n\n\n");
}
int select_menu()
{
    const int width = 50;
    const char* menu[]{ "---------------메뉴---------------", "1. 단일 스레드 테스트", "2. 멀티 스레드 테스트", "3. new/delete 비교 테스트", "종료 : 0"};
   
    printf("%*s%s\n", (width - strlen(menu[0])) / 2," ", menu[0]);
    
    for (int i = 1; i < sizeof(menu) / sizeof(menu[0]); i++)
    {
        printf("%s\n", menu[i]);
    }

    printf("\n메뉴 번호를 입력해주세요 (0은 종료) : ");

   int menu_num;
    scanf_s("%d", &menu_num);

    return menu_num;
}
void realloc_test_single(int outer_loop_cnt, const int alloc_cnt)
{

    printf("----------------- realloc_test -----------------\n");
    const int max_idx = get_max_alloc_size();

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
void alloc_test_single(int target_cnt)
{
    printf("------------------ alloc_test ------------------\n");

    // 테스트항목
    // 1. 할당된 노S드 재할당 테스트
    // 2. 재할당이 가능한 구조인가? 테스트
    // 3. 할당/반복의 횟수가 정확한가? 테스트
    // 4. 요청한 사이즈의 할당/반환이 정확하게 되는가? 테스트
    // idx
    const int max_idx = get_max_alloc_size();

    std::vector<std::pair<void*, int>> pointers(target_cnt, { nullptr,0 });
 
    // 할당 개수 확인 변수
    std::vector<int> alloc_cnt(max_idx, 0);

    for (int i = 0; i < alloc_loop_cnt; i++)
    {
        if (i % 400 == 0)
            printf("Thread ID : [%5d] loop_cnt : [%d]\n",GetCurrentThreadId(), i);

        for (int j = 0; j < target_cnt; j++)
        {
            // alloc_size에 나온 사이즈중 하나를 랜덤으로 뽑아 할당
            int rand_idx = jh::MyRandom::GetRandInt(0, max_idx -1);
            pointers[j] = { g_pMemSystem->Alloc(alloc_size[rand_idx]), rand_idx };
            alloc_cnt[rand_idx]++;

            if (false == test_write(pointers[j].first))
                jh_utility::CrashDump::Crash();
            
        }

        for (int j = 0; j < target_cnt; j++)
        {
            auto [p, rand_idx] = pointers[j];
                
            if (false == test_read(p))
                jh_utility::CrashDump::Crash();

            g_pMemSystem->Free(p);
            alloc_cnt[rand_idx]--;

            pointers[j] = {};
        }
    }

    for (int i = 0; i < max_idx; i++)
    {
        printf("\t\t[alloc_test] size : %d, cnt : %d\n", alloc_size[i], alloc_cnt[i]);

        if (alloc_cnt[i] != 0)
            jh_utility::CrashDump::Crash();
    }
}

unsigned WINAPI go_test_multi(LPVOID lparam)
{
    const int target_cnt = reinterpret_cast<int>(lparam);

    WaitForSingleObject(g_starter_handle, INFINITE);
    
    alloc_test_single(target_cnt);

    return 0;
}
void alloc_test_multi(int target_cnt, int thread_cnt)
{
    std::vector<HANDLE> threads(thread_cnt, nullptr);

    for (int i = 0; i < thread_cnt; i++)
    {
        threads[i] = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, go_test_multi, reinterpret_cast<LPVOID>(target_cnt), 0, nullptr));

        chk_handle(threads[i]);
    }

    // 한 번에 시작!
    SetEvent(g_starter_handle);

    DWORD ret = WaitForMultipleObjects(thread_cnt, threads.data(), true, INFINITE);

    if (ret >= WAIT_OBJECT_0 && ret < WAIT_OBJECT_0 + thread_cnt)
    {
        printf("--------------- multi_thread_test_end ---------------\n");

        for (int i = 0; i < thread_cnt; i++)
        {
            clear_handle(threads[i]);
        }

        ResetEvent(g_starter_handle);
    }
    else
    {
        jh_utility::CrashDump::Crash();
    }
    
}

void do_single_thread_test() 
{
    printf("------------- do_single_thread_test ------------\n");
    const int alloc_rqst_cnt = jh::MyRandom::GetRandInt(rand_min, rand_max);
    const int realloc_rast_cnt = jh::MyRandom::GetRandInt(rand_min, rand_max) / 10;

    alloc_test_single(alloc_rqst_cnt);
    g_pMemSystem->PrintMemoryUsage();

    // 최대 512
    realloc_test_single(realloc_rast_cnt, 300);
    g_pMemSystem->PrintMemoryUsage();

    my_pdh.Print();

    new_line();
    
}

void do_random_alloc_test(int target_cnt, int thread_cnt)
{
    // yet    
    printf("------------- do_random_alloc_test..... yet!! ------------\n");

}

void do_multi_thread_test() 
{
    printf("------------- do_multi_thread_test ------------\n");
    const int thread_cnt = get_available_processors() + ex_thread_cnt;
    
    const int alloc_rqst_cnt = jh::MyRandom::GetRandInt(rand_min, rand_max)/ 5;
    const int realloc_rast_cnt = jh::MyRandom::GetRandInt(rand_min, rand_max) / 10;

    alloc_test_multi(alloc_rqst_cnt, thread_cnt);
    g_pMemSystem->PrintMemoryUsage();

    do_random_alloc_test(alloc_rqst_cnt, thread_cnt);
    g_pMemSystem->PrintMemoryUsage();

    my_pdh.Print();

    new_line();
}
void compare_new_delete() {}
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

    init_main();

    bool end_flag = false;
    while (false == end_flag)
    {
        int user_input = select_menu();

        switch (user_input)
        {
            case 0: end_flag = true; break;
            case 1: do_single_thread_test();  break;
            case 2: do_multi_thread_test();  break;
            case 3: compare_new_delete();  break;
            default: printf("올바르지 않은 입력입니다.\n"); break;
        }
    }

    PRO_SAVE("MemoryTest.txt");

    clear_main();
    printf("--------------- 종료합니다 ---------------\n");

    return 0;
}
