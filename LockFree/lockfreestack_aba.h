//  Created by Kakysha on 22.05.14.
//  Copyright (c) 2014 home. All rights reserved.
//
//  Stack with ABA problem. No memory reclamation.
//


template<typename T>
class LockFreeStack_aba
{
private:
    struct node {
        std::shared_ptr<T> data;
        node* next = nullptr;
        node(T const& data_): data(std::make_shared<T>(data_)){}
        node(std::shared_ptr<T> const& data_ptr): data(data_ptr){}
    };
    std::atomic<node*> head;
public:
    LockFreeStack_aba() {
        head.store(nullptr);
    }
    void push(T const& data)
    {
        node* const new_node=new node(data);
        push_node(new_node);
    }
    void push(std::shared_ptr<T> const& data_ptr)
    {
        node* const new_node=new node(data_ptr);
        push_node(new_node);
    }
    void push_node(node* const& new_node) {
        new_node->next=head.load();
        while(!head.compare_exchange_weak(new_node->next,new_node));
    }
    std::shared_ptr<T> pop()
    {
        node* old_head=head.load();
        while(old_head && !head.compare_exchange_weak(old_head,old_head->next));
        return old_head ? old_head->data : std::shared_ptr<T>();
    }
};