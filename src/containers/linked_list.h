#pragma once

namespace nslib
{

template<class T>
struct slnode
{
    using value_type = T;    
    T data{};
    slnode<T> *next{};
};

template<class T>
struct dlnode
{
    using value_type = T;    
    T data{};
    dlnode<T> *prev{};
    dlnode<T> *next{};
};

template<class T>
struct slist
{
    using node_type = slnode<T>;
    slnode<T> *head{};
};

template<class T>
struct dlist
{
    using node_type = dlnode<T>;
    node_type *head{};
};


template<class T>
void ll_insert(slist<T> *ll, slnode<T> *prev_node, slnode<T> *new_node)
{
    if (!prev_node) {
        new_node->next = ll->head;
        ll->head = new_node;
    }
    else {
        if (!prev_node->next) {
            // Is the last node
            new_node->next = nullptr;
            prev_node->next = new_node;
        }
        else {
            // Is a middle node
            new_node->next = prev_node->next;
            prev_node->next = new_node;
        }
    }
}

template<class T>
void ll_insert(dlist<T> *ll, dlnode<T> *prev_node, dlnode<T> *new_node)
{
    if (!prev_node) {
        // Insert at the beginning
        if (ll->head) {
            // The list has more elements
            new_node->next = ll->head;
            new_node->next->prev = new_node;
        }
        else {
            new_node->next = nullptr;
        }
        ll->head = new_node;
        ll->head->prev = nullptr;
    }
    else {
        if (!prev_node->next) {
            // Is the last node
            new_node->next = nullptr;
            new_node->prev = prev_node;
            prev_node->next = new_node;
        }
        else {
            // Is a middle node
            new_node->next = prev_node->next;
            new_node->prev = prev_node;
            
            prev_node->next = new_node;
            new_node->next->prev = new_node;
        }
    }
}

template<class T>
void ll_remove(slist<T> *ll, slnode<T> *prev_node, slnode<T> *del_node)
{
    if (!prev_node) {
        // Is the first node
        if (!del_node->next) {
            // List only has one element
            ll->head = nullptr;
        }
        else {
            // List has more elements
            ll->head = del_node->next;
        }
    }
    else {
        prev_node->next = del_node->next;
    }
}

template<class T>
void ll_remove(dlist<T> *ll, dlnode<T> *del_node)
{
    if (!del_node->prev) {
        // Is the first node
        if (!del_node->next) {
            // List only has one element
            ll->head = nullptr;
        }
        else {
            // List has more elements
            ll->head = del_node->next;
            ll->head->prev = nullptr;
        }
    }
    else {
        del_node->prev->next = del_node->next;
        if (del_node->next) {
            del_node->next->prev = del_node->prev;
        }
    }
}

// Find the node that is before the passed in "node". If nullptr is passed in as node, this will return the end of the list.
template<class T>
slnode<T> *ll_prev(slist<T> *ll, slnode<T> *node)
{
    auto ret = ll->head;
    while (ret && ret->next != node) {
        ret = ret->next;
    }
    return ret;
}

template<class T>
const slnode<T> *ll_prev(const slist<T> *ll, const slnode<T> *node)
{
    auto ret = ll->head;
    while (ret && ret->next != node) {
        ret = ret->next;
    }
    return ret;
}


// Find the node that is before the passed in "node". If nullptr is passed in as node, this will return the end of the list.
template<class T>
dlnode<T> *ll_prev(dlist<T> *ll, dlnode<T> *node)
{
    auto ret = ll->head;
    if (node) {
        ret = node->prev;
    }
    else {
        while (ret && ret->next != node) {
            ret = ret->next;
        }
    }
    return ret;
}

template<class T>
const dlnode<T> *ll_prev(const dlist<T> *ll, const dlnode<T> *node)
{
    auto ret = ll->head;
    if (node) {
        ret = node->prev;
    }
    else {
        while (ret && ret->next != node) {
            ret = ret->next;
        }
    }
    return ret;
}

template<class NodeType>
NodeType  * ll_next(NodeType *ll) {
    return ll->next;
}

template<class NodeType>
const NodeType  * ll_next(const NodeType *ll) {
    return ll->next;
}

template<class LL>
typename LL::node_type * ll_begin(LL *ll) {
    return ll->head;
}

template<class LL>
const typename LL::node_type * ll_begin(const LL *ll) {
    return ll->head;
}

template<class LL>
typename LL::node_type * ll_rbegin(LL *ll) {
    return ll_prev(ll, {});
}

template<class LL>
const typename LL::node_type * ll_rbegin(const LL *ll) {
    return ll_prev(ll, {});
}


template<class LL>
void ll_push_front(LL *ll, typename LL::node_type *new_node)
{
    ll_insert(ll, {}, new_node);
}

template<class LL>
void ll_push_back(LL *ll, typename LL::node_type *new_node)
{
    auto end = ll_rbegin(ll);
    ll_insert(ll, end, new_node);
}

template<class LL>
typename LL::node_type *ll_pop_front(LL *ll)
{
    typename LL::node_type *top = ll->head;
    ll_remove(ll, {}, top);
    return top;
}

template<class T>
slnode<T> *ll_pop_back(slist<T> *ll)
{
    auto end = ll_rbegin(ll);
    auto prev = ll_prev(ll, end);
    ll_remove(ll, prev, end);
    return end;
}

template<class T>
dlnode<T> *ll_pop_back(dlist<T> *ll)
{
    auto end = ll_rbegin(ll);
    ll_remove(ll, end);
    return end;
}

} // namespace nslib
