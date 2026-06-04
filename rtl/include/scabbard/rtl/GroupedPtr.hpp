/**
 * @file GroupedPtr.hpp
 * @author osterhoutan (osterhoutan+scabbard@gmail.com)
 * @brief A shared pointer system that will reduce the number of allocations and free's that occur
 *        by grouping the allocations into small chunks that will be deallocated only once all
 *        slots in the group have been marked as unreferenced.
 * @version alpha 0.0.1
 * @date 2026-05-08
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cstdlib>


namespace scabbard {

// Forward declaration
template <typename T>
struct Chunk;

template <typename T>
struct Slot {
  T data;
  std::size_t ref_count = 0u;
  Chunk<T>* parent_chunk = nullptr;
};

template <typename T>
struct Chunk {
  std::size_t slots_in_use;
  Chunk* prev = nullptr;
  Chunk* next = nullptr;
  Slot<T>* storage;

  static Chunk* allocate(size_t count)
  {
    Chunk* c = new Chunk();
    c->slots_in_use = count;
    c->storage = new Slot<T>[count];
    for (size_t i = 0u; i < count; ++i) {
      c->storage[i].parent_chunk = c;
    }
    return c;
  }

  void release_slot()
  {
    if (--slots_in_use == 0u) {
      if (storage) delete[] storage;
      storage = nullptr;
      if (prev) prev->next = next;
      if (next) {
        next->prev = prev;
        delete this;
      }
    }
  }

  ~Chunk()
  {
    if (storage)
      delete[] storage;
  }
};

template <typename T>
class GroupedPtr {
private:
  Slot<T>* slot;

public:

  GroupedPtr() = default;

  GroupedPtr(Slot<T>* s) : slot(s)
  {
    if (slot) slot->ref_count++;
  }

  ~GroupedPtr()
  {
    if (not slot) return;

    if (--slot->ref_count == 0u) {
      // This specific object is no longer used by any GroupedPtr
      slot->parent_chunk->release_slot();
    }
  }

  // Copy Constructor
  GroupedPtr(const GroupedPtr& other) : slot(other.slot)
  {
    if (slot) slot->ref_count++;
  }
  GroupedPtr& operator = (const GroupedPtr& other) = default;

  // Move Constructor
  GroupedPtr(GroupedPtr&& other) = delete;
  GroupedPtr& operator = (GroupedPtr&& other) = delete;

  // make a new grouped ptr from the pointer to a data point that is already in a Slot.
  static inline GroupedPtr make(T* ptr) { return GroupedPtr((Slot<T>*)ptr); }

  T& operator*() { return slot->data; }
  T* operator->() { return &slot->data; }
  T* get() const { return (slot && slot->ref_count) ? &slot->data : nullptr; }
  T* blind_get() const { return slot ? &slot->data : nullptr; }
  const T& operator*() const { return slot->data; }
  const T* operator->() const { return &slot->data; }
  const T* get() const { return (slot && slot->ref_count) ? &slot->data : nullptr; }
  const T* blind_get() const { return slot ? &slot->data : nullptr; }
  // T& get_ref() const { return (slot && slot->ref_count) ? slot->data : nullptr; }
  // T& blind_get_ref() const { return slot ? slot->data : nullptr; }
  std::size_t use_count() const { return slot ? block->ref_count : 0ull; }
  explicit operator bool() const { return slot && slot->ref_count; }

  explicit bool operator == (const GroupedPtr& other) const { return slot == other.slot; }
  explicit bool operator == (const void* other) const { return slot == other; }
  explicit bool operator != (const GroupedPtr& other) const { return slot != other.slot; }
  explicit bool operator != (const void* other) const { return slot != other; }

  bool operator == (const T& other) const { return *this == other; }
  bool operator != (const T& other) const { return *this != other; }

  bool operator < (const GroupedPtr& other) const { return *this < *other; }
  bool operator < (const T& other) const { return *this < other; }
  bool operator > (const GroupedPtr& other) const { return *this > *other; }
  bool operator > (const T& other) const { return *this > other; }
  bool operator <= (const GroupedPtr& other) const { return *this <= *other; }
  bool operator <= (const T& other) const { return *this <= other; }
  bool operator >= (const GroupedPtr& other) const { return *this >= *other; }
  bool operator >= (const T& other) const { return *this >= other; }

  class less {
  public:
    bool operator () (const GroupedPtr<T>& l, const GroupedPtr<T> r) const { return *l < *r; }
  };
  // inverse of less for use with std::priority_queue which is weakly ordered 
  class priority_less {
  public:
    bool operator () (const GroupedPtr<T>& l, const GroupedPtr<T> r) const { return not (*l < *r); }
  };
};

template <typename T>
class GroupedPtrFactory {
private:
  using T_t = std::remove_const<T>::type;
  using Chunk_t = Chunk<std::remove_const<T>::type>;
  using Slot_t = Slot<std::remove_const<T>::type>;
  const std::size_t chunk_size;
  std::size_t next_slot_index;
  Chunk_t* root_chunk;

  void refresh_chunk()
  {
    Chunk_t* prev = nullptr; 
    if (root_chunk->slots_in_use == 0u) { //case: current head chunk fully deallocated
      if (root_chunk->prev) prev = root_chunk->prev;
      delete root_chunk;
    } else
      prev = root_chunk;
    root_chunk = Chunk_t::allocate(chunk_size);
    root_chunk->prev = prev;
    if (prev) prev->next = root_chunk;
    next_slot_index = 0u;
  }

  Chunk_t* free_all_prev(Chunk_t* cur)
  {
    if (cur->prev)
      delete free_all_prev(cur->prev);
    return cur;
  }

  inline Slot_t* getSlot()
  {
    if (next_slot_index >= chunk_size)
      refresh_chunk();
    return &root_chunk->storage[next_slot_index++];
  }

public:
  explicit GroupedPtrFactory(size_t pool_size = 64u)
    : chunk_size(pool_size), next_slot_index(0u), 
      root_chunk(Chunk_t::allocate(pool_size))
  {}

  ~GroupedPtrFactory()
  {
    free_all();
  }

  // Creates a new pointer, copying the value into the next available slot
  inline GroupedPtr<T> create(const T& value)
  {
    Slot_t* target_slot = getSlot();
    target_slot->data = value; // Copy the data into the slot
    return GroupedPtr<T>(target_slot);
  }

  // Creates a new pointer, moving the value into the next available slot, invalidating old location.
  inline GroupedPtr<T> create(T_t&& __value)
  {
    Slot_t* target_slot = getSlot();
    target_slot->data = __value; // move the data into the slot
    return GroupedPtr<T>(target_slot);
  }

  // Construct a new object and return a GroupedPtr to the newly constructed object;
  template<class... Args>
  inline GroupedPtr<T> make(Args&&... args)
  {
    Slot_t* target_slot = getSlot();
    new (&target_slot->data) T(std::forward<Args>(args)...);
    return GroupedPtr<T>(target_slot);
  }

  /// @brief Free/delete all memory allocated by this factory \n
  ///        \em WARNING: all \c GroupedPtr 's still around are now invalid.
  void free_all() 
  {
    if (root_chunk)
      delete free_all_prev(root_chunk);
  }
};

} //?namespace scabbard
