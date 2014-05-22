#include <atomic>

template <typename T>
class LockFreeStack_leak
{
public:
    struct node {
        std::shared_ptr<T> data;
        node* next = nullptr;
        node(T const& data_): data(std::make_shared<T>(data_)){}
        node(std::shared_ptr<T> const& data_ptr): data(data_ptr){}
    };
    
    class TaggedPointer
    {
    public:
        TaggedPointer(): m_node(nullptr), m_counter(0) {}
        
        node* GetNode()
        {
            return m_node.load(std::memory_order_acquire);
        }
        
        uint64_t GetCounter()
        {
            return m_counter.load(std::memory_order_acquire);
        }
        
        bool CompareAndSwap(node* oldNode, uint64_t oldCounter, node* newNode, uint64_t newCounter)
        {
            bool cas_result;
            __asm__ __volatile__
            (
             "lock cmpxchg16b %0;"  // cmpxchg16b sets ZF on success
             "setz       %3;"  // if ZF set, set cas_result to 1
             
             : "+m" (*this), "+a" (oldNode), "+d" (oldCounter), "=q" (cas_result)
             : "b" (newNode), "c" (newCounter)
             : "cc", "memory"
             );
            return cas_result;
        }
    private:
        std::atomic<node*> m_node;
        std::atomic<uint64_t> m_counter;
    }
    // 16-byte alignment is required for double-width
    // compare and swap
    //
    __attribute__((aligned(16)));
    
    bool TryPushStack(std::shared_ptr<T> const& data_ptr)
    {
        node* const new_node=new node(data_ptr);
        node* oldHead;
        uint64_t oldCounter;
        
        oldHead = m_head.GetNode();
        oldCounter = m_head.GetCounter();
        new_node->next = oldHead;
        return m_head.CompareAndSwap(oldHead, oldCounter, new_node, oldCounter + 1);
    }
    
    bool TryPopStack(node*& oldHead)
    {
        oldHead = m_head.GetNode();
        uint64_t oldCounter = m_head.GetCounter();
        if(oldHead == nullptr)
        {
            return true;
        }
        //m_hazard[threadId*8].store(oldHead, std::memory_order_seq_cst);
        if(m_head.GetNode() != oldHead)
        {
            return false;
        }
        return m_head.CompareAndSwap(oldHead, oldCounter, oldHead->next, oldCounter + 1);
    }
    
    void push(std::shared_ptr<T> const& data_ptr)
    {
        while(true)
        {
            if(TryPushStack(data_ptr))
            {
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
    }
    
    std::shared_ptr<T> pop()
    {
        node* res;
        while(true)
        {
            if(TryPopStack(res))
            {
                return res ? res->data : std::shared_ptr<T>();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
    }
    
private:
    TaggedPointer m_head;
    std::atomic<node*> m_hazard[MAX_THREADS*8];
};
