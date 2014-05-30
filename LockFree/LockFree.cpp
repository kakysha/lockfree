#define MAX_THREADS 4
#define TEST_MUTEX 1
#define TEST_SPINLOCK 1
#define TEST_ABA 1
#define TEST_LEAK 1
#define TEST_TC 1
#define TEST_HP 1

#include <iostream>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <vector>
#include <sys/time.h>
#include <cassert>
#include "lockstack.h"
#include "spinlockedstack.h"
#include "lockfreestack_aba.h"
#include "lockfreestack_leak.h"
#include "lockfreestack_tc.h"
#include "lockfreestack_hp.h"

struct LockedElement
{
    int data;
};

std::atomic<bool> running;

template<class Stack, class Element>
void Worker(Stack& st, Element* elems, int numElements, int* numOps, int threadId)
{
    unsigned int seed = rand();
    std::vector<std::shared_ptr<Element>> mine;
    int ops = 0;
    for(int i=0; i<numElements; i++)
    {
        elems[i] = 0;
        mine.push_back(std::make_shared<Element>(elems[i]));
    }
    while(!running.load(std::memory_order_acquire)){}
    while(running.load(std::memory_order_acquire))
    {
        std::shared_ptr<Element> elem;
        switch(rand_r(&seed)&1)
        {
            case 0:
                if(mine.size())
                {
                    elem = mine.back();
                    assert(*elem == 0);
                    *elem = 1;
                    mine.pop_back();
                    st.push(elem);
                }
                ops++;
                break;
            case 1:
                elem = st.pop();
                if(elem != nullptr)
                {
                    assert(*elem == 1);
                    *elem = 0;
                    mine.push_back(elem);
                }
                ops++;
                break;
        }
    }
    *numOps = ops;
}


template<typename Stack, typename Element>
double Test(int nthreads)
{
    const int num_elements = 1000000;
    const int test_time = 1000;
    const int test_iterations = 5;
    const int elem_per_thread = num_elements / nthreads;
    long long ops = 0;
    
    for(int it = 0; it < test_iterations; it++)
    {
        Stack& st = *(new Stack());
        Element* elements = new Element[num_elements];
        
        std::thread threads[MAX_THREADS];
        int numOps[MAX_THREADS] = {};
        
        for(int i = 0; i < nthreads; i++)
        {
            threads[i] = std::thread(Worker<Stack, Element>, std::ref(st), elements + i*elem_per_thread, elem_per_thread, &numOps[i], i);
        }
        
        running.store(true, std::memory_order_release);
        std::this_thread::sleep_for(std::chrono::milliseconds(test_time));
        running.store(false, std::memory_order_release);
        
        for(int i = 0; i < nthreads; i++)
        {
            threads[i].join();
            ops += numOps[i];
        }
        delete[] elements;
    }
    return (double)ops / (test_time*test_iterations);
}

int main()
{
    for(int i=1; i<=MAX_THREADS; i++)
    {
        double lockedTime = TEST_MUTEX ? Test<LockedStack<std::shared_ptr<int>>, int>(i) : 0;
        double spinLockedTime = TEST_SPINLOCK ? Test<SpinLockedStack<std::shared_ptr<int>>, int>(i) : 0;
        double lockFreeTimeABA = TEST_ABA ? Test<LockFreeStack_aba<int>, int>(i) : 0;
        double lockFreeTimeLeak = TEST_LEAK ? Test<LockFreeStack_leak<int>, int>(i) : 0;
        double lockFreeTimeTC = TEST_TC ? Test<LockFreeStack_tc<int>, int>(i) : 0;
        double lockFreeTimeHP = TEST_HP ? Test<LockFreeStack_hp<int>, int>(i) : 0;
        printf("%d threads, Locked:%5d/msec, Spinlock:%5d/msec, LockFree_ABA:%5d/msec, Lockfree_leak:%5d/msec, LockFree_TC:%5d/msec, LockFree_HP:%5d/msec\n", i, (int)lockedTime, (int)spinLockedTime, (int)lockFreeTimeABA, (int)lockFreeTimeLeak, (int)lockFreeTimeTC, (int)lockFreeTimeHP);
    }
    return 0;
}
