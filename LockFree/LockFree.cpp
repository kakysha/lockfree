#define MAX_THREADS 4

#include <cstdlib>
#include <thread>
#include <chrono>
#include <vector>
#include <sys/time.h>
#include <cassert>
#include "lockstack.h"
#include "lockfreestack.h"
#include "spinlockedstack.h"
#include "lockfreestack_leak.h"

struct LockedElement
{
    int data;
};
struct LockFreeElement: public LockFreeStack::Node
{
    int data;
};

std::atomic<bool> running;

template<class Stack, class Element>
void Worker(Stack& st, Element* elems, int numElements, int* numOps, int threadId)
{
    unsigned int seed = rand();
    std::vector<Element*> mine;
    int ops = 0;
    for(int i=0; i<numElements; i++)
    {
        mine.push_back(&elems[i]);
        elems[i].data = 0;
    }
    while(!running.load(std::memory_order_acquire)){}
    while(running.load(std::memory_order_acquire))
    {
        Element* elem;
        switch(rand_r(&seed)&1)
        {
            case 0:
                if(mine.size())
                {
                    elem = mine.back();
                    assert(elem->data == 0);
                    elem->data = 1;
                    mine.pop_back();
                    st.Push(elem);
                }
                ops++;
                break;
            case 1:
                elem = static_cast<Element*>(st.Pop(threadId));
                if(elem != nullptr)
                {
                    assert(elem->data == 1);
                    elem->data = 0;
                    mine.push_back(elem);
                }
                ops++;
                break;
        }
    }
    *numOps = ops;
}


template<class Stack, class Element>
double Test(int nthreads)
{
    const int num_elements = 10000;
    const int test_time = 1000;
    const int test_iterations = 2;
    const int elem_per_thread = num_elements / nthreads;
    long long ops = 0;
    
    for(int it = 0; it < test_iterations; it++)
    {
        Stack st;
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
        double lockFreeTime = Test<LockFreeStack, LockFreeElement>(i);
        double lockedTime = Test<LockedStack<LockedElement>, LockedElement>(i);
        double spinLockedTime = Test<SpinLockedStack<LockedElement>, LockedElement>(i);
        double lockFreeTimeLeak = 0;//Test<LockFreeStack_leak, LockFreeLeakElement>(i);
        printf("%d threads, Locked:%6d/msec, Lockfree:%6d/msec, Spinlock:%6d/msec, LockFree_leak:%6d/msec\n", i, (int)lockedTime, (int)lockFreeTime, (int)spinLockedTime, (int)lockFreeTimeLeak);
    }
    return 0;
}
