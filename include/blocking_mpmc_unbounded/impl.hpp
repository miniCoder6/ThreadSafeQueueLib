#ifndef BLOCKING_MPMC_UNBOUNDED_IMPL
#define BLOCKING_MPMC_UNBOUNDED_IMPL

#include "defs.hpp"

namespace tsfqueue::__impl {

template <typename T> void blocking_mpmc_unbounded<T>::push(T value) {
    std::shared_ptr<T>temp_data(std::make_shared<T>(std::move(value)));/*temp data value shared */
    std::unique_ptr<node>temp_node(new node);/*new node temp*/
    {
        std::lock_guard<std::mutex>tail_lock(tail_mutex);/*lock the tail*/
        tail->data=temp_data;/*push the data to temp*/ 
        node * const temp_tail=temp_node.get();
        tail->next=std::move(temp_node);
        tail=temp_tail;
    }
    cond.notify_one();/*we can notify other depend on condition variable while waiting or error just after first element */
}

template <typename T> typename blocking_mpmc_unbounded<T>::node *blocking_mpmc_unbounded<T>::get_tail() {
    std::lock_guard<std::mutex>tail_lock(tail_mutex);
    return tail;
}

template <typename T> std::unique_ptr<typename blocking_mpmc_unbounded<T>::node> blocking_mpmc_unbounded<T>::wait_and_get() {
    /*helper function for wait and pop  and it return the head after poping it also wait till data arrives */
    std::unique_lock<std::mutex>head_lock(head_mutex);
    cond.wait(head_lock,[&]{return head.get()!=get_tail();});
    std::unique_ptr<node>old_head=std::move(head);
    head=std::move(old_head->next);
    return old_head;
}

template <typename T> std::unique_ptr<typename blocking_mpmc_unbounded<T>::node> blocking_mpmc_unbounded<T>::try_get() {
    /*helper function for try_pop */
    std::lock_guard<std::mutex>head_lock(head_mutex);
    if(head.get()==get_tail()){
        return std::unique_ptr<node>();
    }
    std::unique_ptr<node>old_head=std::move(head);
    head=std::move(old_head->next);
    return old_head;
}

template <typename T> void blocking_mpmc_unbounded<T>::wait_and_pop(T &value) {
    /*wrapper around wait_and_get*/
    std::unique_ptr<node>old_head=wait_and_get();
    value=std::move(*old_head->data);
}

template <typename T> std::shared_ptr<T> blocking_mpmc_unbounded<T>::wait_and_pop() {
   std::unique_ptr<node>old_head=wait_and_get();
   return(std::move(old_head->data));
}

template <typename T> bool blocking_mpmc_unbounded<T>::try_pop(T &value) {
    std::unique_ptr<node>old_head=try_get();
    if(!old_head)return false;
    value=std::move(*old_head->data);
    return true;
}

template <typename T> std::shared_ptr<T> blocking_mpmc_unbounded<T>::try_pop() {
    std::unique_ptr<node>old_head=try_get();
    return old_head ?std::move(old_head->data):std::shared_ptr<T>();
}

template <typename T> bool blocking_mpmc_unbounded<T>::empty() {
    std::lock_guard<std::mutex>head_lock(head_mutex);
    return (head.get()==get_tail());
}
template <typename T> size_t blocking_mpmc_unbounded<T>::size() {
   std::lock_guard<std::mutex>head_lock(head_mutex);
   std::lock_guard<std::mutex>tail_lock(tail_mutex);

   size_t count = 0;
   for (node* curr = head.get(); curr != tail; curr = curr->next.get()) {
      ++count;
   }
   return count;
}

} // namespace tsfqueue::__impl
#endif

// 1. Add static asserts
// 2. Add emplace_back using perfect forwarding and variadic templates (you
// can use this in push then)
// 3. Add size() function
// 4. Any more suggestions ??