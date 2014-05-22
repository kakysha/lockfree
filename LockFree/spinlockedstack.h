//  Created by Kakysha on 22.05.14.
//  Copyright (c) 2014 home. All rights reserved.
//
//  Simple spinlocked stack.
//


#include <stack>
#include <atomic>

class SpinLock
{
public:
    void Acquire()
    {
        while (true)
        {
            if (!m_locked.test_and_set(std::memory_order_acquire))
            {
                return;
            }
            //std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
    }
    void Release()
    {
        m_locked.clear(std::memory_order_release);
    }
    
private:
    std::atomic_flag m_locked;
};

template<class T>
class SpinLockedStack
{
public:
    void push(T entry)
    {
        m_lock.Acquire();
        m_stack.push(entry);
        //std::this_thread::sleep_for(std::chrono::nanoseconds(1));
        m_lock.Release();
    }
    
    // For compatability with the LockFreeStack interface,
    // add an unused int parameter.
    //
    T pop()
    {
        m_lock.Acquire();
        if(m_stack.empty())
        {
            m_lock.Release();
            return nullptr;
        }
        T ret = m_stack.top();
        m_stack.pop();
        m_lock.Release();
        return ret;
    }
    
private:
    SpinLock m_lock;
    std::stack<T> m_stack;
};
