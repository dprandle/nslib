#pragma once

namespace nslib
{

template<class T>
struct ll_node
{
    T data;
    ll_node<T> *next{};
};

template<class T>
struct singly_linked_list
{
    ll_node<T> *head{};
};

template<class T>
void ll_insert(singly_linked_list<T> *ll, ll_node<T> *prev_node, ll_node<T> *new_node)
{
    if (!prev_node)
    {
        // Is the first node
        if (ll->head)
        {
            // The list has more elements
            new_node->next = ll->head;
        }
        else
        {
            new_node->next = nullptr;
        }
        ll->head = new_node;
    }
    else
    {
        if (!prev_node->next)
        {
            // Is the last node
            prev_node->next = new_node;
            new_node->next = nullptr;
        }
        else
        {
            // Is a middle node
            new_node->next = prev_node->next;
            prev_node->next = new_node;
        }
    }
}

template<class T>
void ll_remove(singly_linked_list<T> *ll, ll_node<T> *prev_node, ll_node<T> *del_node)
{
    if (!prev_node)
    {
        // Is the first node
        if (!del_node->next)
        {
            // List only has one element
            ll->head = nullptr;
        }
        else
        {
            // List has more elements
            ll->head = del_node->next;
        }
    }
    else
    {
        prev_node->next = del_node->next;
    }
}

template<class T>
void ll_push(singly_linked_list<T> *ll, ll_node<T> *new_node)
{
    new_node->next = ll->head;
    ll->head = new_node;
}

template<class T>
ll_node<T> *ll_pop(singly_linked_list<T> *ll)
{
    ll_node<T> *top = ll->head;
    ll->head = ll->head->next;
    return top;
}

} // namespace nslib
