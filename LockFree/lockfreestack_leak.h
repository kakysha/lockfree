//
//  lockfreestack_hp.h
//  LockFree
//
//  Created by Drunk on 22.05.14.
//  Copyright (c) 2014 home. All rights reserved.
//

template<typename T>
class LockFreeStach_leak
{
private:
    struct node {
        std::shared_ptr<T> data;
        node* next;
        node(T const& data_): data(std::make_shared<T>(data_)){}
    };
    std::atomic<node*> head;
public:
    void push(T const& data)
    {
        node* const new_node=new node(data);
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