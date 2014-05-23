//  Created by Kakysha on 22.05.14.
//  Copyright (c) 2014 home. All rights reserved.
//
//  Stack without ABA problem with memory reclamation based
//  on counting threads currently in pop()
//

#include <atomic>

template <typename T>
class LockFreeStack_tc
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
            //std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
    }
    
    std::shared_ptr<T> pop()
    {
        ++threads_in_pop;
        node* old_head = m_head.GetNode();
        uint64_t oldCounter = m_head.GetCounter();
        while (old_head && !m_head.CompareAndSwap(old_head, oldCounter, old_head->next, oldCounter + 1));
        std::shared_ptr<T> res;
        if (old_head) {
            res.swap(old_head->data);
        }
        try_reclaim(old_head);
        return res;
    }
    
private:
    TaggedPointer m_head;
    std::atomic<node*> m_hazard[MAX_THREADS*8];
    std::atomic<unsigned> threads_in_pop;
    std::atomic<node*> to_be_deleted;
    static void delete_nodes(node* nodes)
    {
        while(nodes)
        {
            node* next=nodes->next;
            delete nodes;
            nodes=next;
        }
    }
    
    void try_reclaim(node* old_head)
    {
        if(threads_in_pop == 1)
        {
            node* nodes_to_delete = to_be_deleted.exchange(nullptr);
            if(!--threads_in_pop) {
                delete_nodes(nodes_to_delete);
            } else if (nodes_to_delete) {
                chain_pending_nodes(nodes_to_delete);
            }
            delete old_head;
        } else {
            if (old_head) chain_pending_node(old_head);
            --threads_in_pop;
        }
    }
    
    void chain_pending_nodes(node* nodes)
    {
        node* last=nodes;
        while(node* const next=last->next)
        {
            last=next; }
        chain_pending_nodes(nodes,last);
    }
    
    void chain_pending_nodes(node* first,node* last)
    {
        last->next=to_be_deleted;
        while(!to_be_deleted.compare_exchange_weak(last->next,first));
    }
    
    void chain_pending_node(node* n)
    {
        chain_pending_nodes(n,n);
    }
};
