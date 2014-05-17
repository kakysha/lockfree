#include <stack>
#include <atomic>
#include <unistd.h>

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
            usleep(250);
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
    void Push(T* entry)
    {
        m_lock.Acquire();
        m_stack.push(entry);
        m_lock.Release();
    }
    
    // For compatability with the LockFreeStack interface,
    // add an unused int parameter.
    //
    T* Pop(int)
    {
        m_lock.Acquire();
        if(m_stack.empty())
        {
            m_lock.Release();
            return nullptr;
        }
        T* ret = m_stack.top();
        m_stack.pop();
        m_lock.Release();
        return ret;
    }
    
private:
    SpinLock m_lock;
    std::stack<T*> m_stack;
};
