//  Created by Kakysha on 22.05.14.
//  Copyright (c) 2014 home. All rights reserved.
//
//  Stack without ABA problem. No memory reclamation.
//

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
        
        bool CompareAndSwap(node*& oldNode, uint64_t& oldCounter, node* newNode, uint64_t newCounter)
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
        
    void push(std::shared_ptr<T> const& data_ptr)
    {
        node* const new_node=new node(data_ptr);
        node* oldHead;
        uint64_t oldCounter;
        do {
            oldHead = m_head.GetNode();
            oldCounter = m_head.GetCounter();
            new_node->next = oldHead;
        } while (!m_head.CompareAndSwap(oldHead, oldCounter, new_node, oldCounter + 1));
    }
    
    std::shared_ptr<T> pop()
    {
        node* old_head = m_head.GetNode();
        uint64_t oldCounter = m_head.GetCounter();
        while (old_head && !m_head.CompareAndSwap(old_head, oldCounter, old_head->next, oldCounter + 1));
        std::shared_ptr<T> res;
        if (old_head) {
            res.swap(old_head->data);
        }
        return res;
    }
    
private:
    TaggedPointer m_head;
    std::atomic<node*> m_hazard[MAX_THREADS*8];
};
