#ifndef LOCKFREESTACK_H
#define LOCKFREESTACK_H

#include <memory>
#include <atomic>
#include <optional>

using namespace std;
using mo = std::memory_order;

template <typename T>
class node_t
{
public:
  shared_ptr<T> m_data;
  atomic<node_t *> m_next;

  explicit node_t(T const &value) : m_data(make_shared<T>(value)), m_next(nullptr) {}
};

template <typename T>
class LockfreeStack
{
  using node_type = node_t<T>;
  atomic<node_type *> m_tail;

public:
  LockfreeStack() : m_tail(nullptr) {}
  LockfreeStack(LockfreeStack const &) = delete;
  LockfreeStack &operator=(LockfreeStack const &) = delete;

  ~LockfreeStack()
  {
    node_type *curr;
    while ((curr = m_tail.load(mo::relaxed)) != nullptr)
    {
      node_type *next = curr->m_next.load(mo::relaxed);
      delete curr;
        m_tail.store(next, mo::relaxed);
    }
  }

  void push(const T &data)
  {
    auto latest = new node_type(data);
    node_type *thread_tail;
    do
    {
      thread_tail = m_tail.load(mo::relaxed);
      latest->m_next.store(thread_tail, mo::relaxed);
    } while (!m_tail.compare_exchange_weak(thread_tail, latest, mo::relaxed));
  }

  optional<T> pop()
  {
    node_type *thread_tail;
    node_type *next;

    do
    {
      thread_tail = m_tail.load(mo::relaxed);
      // the stack might be already empty
      if (thread_tail == nullptr)
      {
        return nullopt;
      }
      next = thread_tail->m_next.load(mo::relaxed);
    } while (!m_tail.compare_exchange_weak(thread_tail, next, mo::relaxed));

    // some other thread might be here as well, how to prevent double deletion
    // in multi-threaded deletion,
    auto data = thread_tail->m_data;
    if (data)
    {
      T value = *data;
      delete thread_tail;
      return value;
    } else {
        return nullopt;
    }

  }

  [[nodiscard]] bool empty() const
  {
    return m_tail.load(mo::acquire) == nullptr;
  }
};

#endif