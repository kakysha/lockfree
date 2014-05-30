//  Created by Kakysha on 22.05.14.
//  Copyright (c) 2014 home. All rights reserved.
//
//  Stack without ABA problem with memory reclamation based
//  on hazard pointers.
//

#include <atomic>

unsigned const max_hazard_pointers=100;

struct hazard_pointer
{
    std::atomic<std::thread::id> id;
    std::atomic<void*> pointer;
};

hazard_pointer hazard_pointers[max_hazard_pointers];

class hp_owner
{
    hazard_pointer* hp;
public:
    hp_owner(hp_owner const&)=delete;
    hp_owner operator=(hp_owner const&)=delete;
    hp_owner():
    hp(nullptr)
    {
        for(unsigned i=0;i<max_hazard_pointers;++i)
        {
            std::thread::id old_id;
            if(hazard_pointers[i].id.compare_exchange_strong(old_id,std::this_thread::get_id()))
            {
                hp=&hazard_pointers[i];
                break;
            }
        }
        if(!hp)
        {
            throw std::runtime_error("No hazard pointers available");
        }
    }
    std::atomic<void*>& get_pointer()
    {
        return hp->pointer;
    }
    ~hp_owner()
    {
        hp->pointer.store(nullptr);
        hp->id.store(std::thread::id());
    }
};

std::atomic<void*>& get_hazard_pointer_for_current_thread()
{
    thread_local hp_owner hazard;
    return hazard.get_pointer();
}

bool outstanding_hazard_pointers_for(void* p)
{
    for(unsigned i=0;i<max_hazard_pointers;++i)
    {
        if(hazard_pointers[i].pointer.load()==p)
        {
            return true;
        }
    }
    return false;
}
template<typename TP>
void do_delete(void* p)
{
    delete static_cast<TP*>(p);
}
struct data_to_reclaim
{
    void* data;
    std::function<void(void*)> deleter;
    data_to_reclaim* next;
    template<typename TP>
    data_to_reclaim(TP* p): data(p),deleter(&do_delete<TP>), next(0) {}
    ~data_to_reclaim()
    {
        deleter(data);
    }
};
std::atomic<data_to_reclaim*> nodes_to_reclaim;

void add_to_reclaim_list(data_to_reclaim* node)
{
    node->next=nodes_to_reclaim.load();
    while(!nodes_to_reclaim.compare_exchange_weak(node->next,node));
}
template<typename TP>
void reclaim_later(TP* data)
{
    add_to_reclaim_list(new data_to_reclaim(data));
}
void delete_nodes_with_no_hazards()
{
    data_to_reclaim* current=nodes_to_reclaim.exchange(nullptr);
    while(current)
    {
        data_to_reclaim* const next=current->next;
        if(!outstanding_hazard_pointers_for(current->data))
        {
            delete current;
        }
        else {
            add_to_reclaim_list(current);
        }
        current=next;
    }
}

template <typename T>
class LockFreeStack_hp
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
        std::atomic<void*>& hp=get_hazard_pointer_for_current_thread();
        node* old_head = m_head.GetNode();
        uint64_t oldCounter = m_head.GetCounter();
        do {
            node* temp;
            do
            {
                temp=old_head;
                hp.store(old_head);
                old_head=m_head.GetNode();
            } while(old_head!=temp);
        } while (old_head && !m_head.CompareAndSwap(old_head, oldCounter, old_head->next, oldCounter + 1));
        hp.store(nullptr);
        std::shared_ptr<T> res;
        if (old_head) {
            res.swap(old_head->data);
            if(outstanding_hazard_pointers_for(old_head))
            {
                reclaim_later(old_head);
            } else {
                delete old_head;
            }
            delete_nodes_with_no_hazards();
        }
        return res;
    }
    
private:
    TaggedPointer m_head;
};
