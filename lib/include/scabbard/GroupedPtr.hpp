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
    Slot<T>* storage;

    static Chunk* allocate(size_t count) {
        Chunk* c = new Chunk();
        c->slots_in_use = count;
        c->storage = new Slot<T>[count];
        for (size_t i = 0u; i < count; ++i) {
            c->storage[i].parent_chunk = c;
        }
        return c;
    }

    void release_slot() {
        if (--slots_in_use == 0u) {
            delete[] storage;
            storage = nullptr;
            delete this;
        }
    }

    ~Chunk() {
      if (slots_in_use == 0u && storage)
        delete[] storage;
    }
};

template <typename T>
class GroupedPtr {
private:
    Slot<T>* slot;

public:
    GroupedPtr(Slot<T>* s) : slot(s) {
        if (slot) slot->ref_count++;
    }

    ~GroupedPtr() {
        if (!slot) return;

        if (--slot->ref_count == 0u) {
            // This specific object is no longer used by any GroupedPtr
            slot->parent_chunk->release_slot();
        }
    }

    // Copy Constructor
    GroupedPtr(const GroupedPtr& other) : slot(other.slot) {
        if (slot) slot->ref_count++;
    }

    T& operator*() { return slot->data; }
    T* operator->() { return &slot->data; }
};

template <typename T>
class GroupedPtrFactory {
private:
    std::size_t chunk_size;
    std::size_t next_slot_index;
    Chunk<T>* current_chunk;

    void refresh_chunk() {
        current_chunk = Chunk<T>::allocate(chunk_size);
        next_slot_index = 0u;
    }

public:
    explicit GroupedPtrFactory(size_t pool_size = 64u) 
        : chunk_size(pool_size), next_slot_index(0u), current_chunk(nullptr) {
        refresh_chunk();
    }

    // Creates a new pointer, copying the value into the next available slot
    GroupedPtr<T> create(const T& value) {
        if (next_slot_index >= chunk_size) {
            refresh_chunk();
        }

        Slot<T>* target_slot = &current_chunk->storage[next_slot_index];
        target_slot->data = value; // Copy the data into the slot
        next_slot_index++;

        return GroupedPtr<T>(target_slot);
    }
};

} //?namespace scabbard
