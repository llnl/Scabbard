/**
 * @file IntervalMap.hpp
 * @author osterhoutan (osterhoutan+scabbard@gmail.com)
 * @brief A map from half-open intervals [Start, Stop) to values.
 *
 *       Stores non-overlapping, sorted intervals backed by a std::map keyed on
 *       interval start.  On insertion, any overlapping existing intervals are
 *       trimmed so the new interval is inserted cleanly.  Lookup and find are
 *       O(log N); insert is O(K log N) where K is the number of overlapping
 *       intervals displaced.
 * 
 *      Template parameters:
 *        KeyT       - comparable key type (must support prefix ++ for single-key insert)
 *        ValT       - mapped value type
 *       KeyComparator - strict weak ordering; defaults to std::less<KeyT>
 * 
 *      \em NOTE: generated with Claude.ai Sonnet4.6 medium (modified by hand later).
 * 
 * @version alpha 0.0.1
 * @date 2026-06-03
 * 
 */
#pragma once

#include <map>
#include <stdexcept>
#include <functional>
#include <iterator>
#include <type_traits>

namespace scabbard {
namespace rtl {

/// @brief A map from half-open intervals [Start, Stop) to values.
///
///       Stores non-overlapping, sorted intervals backed by a std::map keyed on
///       interval start.  On insertion, any overlapping existing intervals are
///       trimmed so the new interval is inserted cleanly.  Lookup and find are
///       O(log N); insert is O(K log N) where K is the number of overlapping
///       intervals displaced.
///
/// @tparam KeyT       - comparable key type (must support prefix ++ for single-key insert)
/// @tparam ValT       - mapped value type
/// @tparam KeyComparator - strict weak ordering; defaults to std::less<KeyT>
template<typename KeyT, typename ValT, 
        class KeyComparator = std::less<KeyT>,
        class KeyEquator    = std::equal_to<KeyT>,
        class ValEquator    = std::equal_to<ValT>>
class IntervalMap {
public:
    //<----------------------------------------------------------------------->
    //  Node - the public payload exposed through iterators
    //<----------------------------------------------------------------------->
    struct Node {
        KeyT start;
        KeyT stop;   ///< exclusive upper bound  ([start, stop))
        ValT val;

        Node(KeyT s, KeyT e, ValT v)
            : start(std::move(s)), stop(std::move(e)), val(std::move(v)) {}

        std::size_t width() const { return stop - start; }
        bool is_single() const { return width() == 1u; }

    protected:
        // nothing extra needed here; storage lives in the tree
    };

private:
    //<----------------------------------------------------------------------->
    // Internal storage
    //
    // We keep a std::map<KeyT, Node*, KeyComparator> keyed by interval *start*.
    // This gives O(log N) lower-bound searches for both lookup and insert.
    // Nodes are heap-allocated so that iterator stability is preserved after
    // insertions that do not touch a given node.
    //<----------------------------------------------------------------------->
    using StorageMap  = std::map<KeyT, Node*, KeyComparator>;
    using StorageIter = typename StorageMap::iterator;

    KeyComparator  cmp_;
    KeyEquator eq_;
    StorageMap  storage_;

    // Helper: is a strictly less than b?
    bool lt(const KeyT& a, const KeyT& b) const { return cmp_(a, b); }
    // Helper: are a and b equal (neither less than the other)?
    bool eq(const KeyT& a, const KeyT& b) const { return eq_(a,b); }

    // Delete the Node* owned by a storage iterator and erase from map.
    StorageIter erase_node(StorageIter it) {
        delete it->second;
        return storage_.erase(it);
    }

public:
    //<----------------------------------------------------------------------->
    //  Iterator
    //<----------------------------------------------------------------------->

    /// Forward iterator over IntervalMap nodes.
    /// operator-> returns Node*  (access .start / .stop / .val directly).
    /// operator*  returns Node&.
    class iterator {
        StorageIter it_;
        friend class IntervalMap;
        explicit iterator(StorageIter it) : it_(it) {}
    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type        = Node;
        using difference_type   = std::ptrdiff_t;
        using pointer           = Node*;
        using reference         = Node&;

        iterator() = default;

        reference operator*()  const { return *it_->second; }
        pointer   operator->() const { return  it_->second; }

        iterator& operator++()    { ++it_; return *this; }
        iterator  operator++(int) { auto tmp = *this; ++(*this); return tmp; }
        iterator& operator--()    { --it_; return *this; }
        iterator  operator--(int) { auto tmp = *this; --(*this); return tmp; }

        bool operator==(const iterator& o) const { return it_ == o.it_; }
        bool operator!=(const iterator& o) const { return it_ != o.it_; }
    };

    /// Const forward iterator.
    class const_iterator {
        typename StorageMap::const_iterator it_;
        friend class IntervalMap;
        explicit const_iterator(typename StorageMap::const_iterator it) : it_(it) {}
    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type        = const Node;
        using difference_type   = std::ptrdiff_t;
        using pointer           = const Node*;
        using reference         = const Node&;

        const_iterator() = default;
        // Allow implicit conversion from iterator
        const_iterator(const iterator& it) : it_(it.it_) {}

        reference operator*()  const { return *it_->second; }
        pointer   operator->() const { return  it_->second; }

        const_iterator& operator++()    { ++it_; return *this; }
        const_iterator  operator++(int) { auto tmp = *this; ++(*this); return tmp; }
        const_iterator& operator--()    { --it_; return *this; }
        const_iterator  operator--(int) { auto tmp = *this; --(*this); return tmp; }

        bool operator==(const const_iterator& o) const { return it_ == o.it_; }
        bool operator!=(const const_iterator& o) const { return it_ != o.it_; }
    };


    class iterator_collection;
    class const_iterator_collection;

    //<----------------------------------------------------------------------->
    // Construction / destruction / copy / move
    //<----------------------------------------------------------------------->

    IntervalMap() = default;

    explicit IntervalMap(const KeyComparator& cmp, const KeyEquator& _eq) 
      : storage_(cmp), cmp_(cmp), eq_(_eq) {}
    explicit IntervalMap(const KeyComparator& cmp) 
      : storage_(cmp), cmp_(cmp) {}
    explicit IntervalMap(const KeyEquator& _eq) 
      : eq_(_eq) {}

    ~IntervalMap() {
        for (auto& [k, n] : storage_) delete n;
    }

    IntervalMap(const IntervalMap& o) : cmp_(o.cmp_), storage_(StorageMap(o.cmp_)) {
        for (auto& [k, n] : o.storage_)
            storage_.emplace(k, new Node(*n));
    }

    IntervalMap(IntervalMap&& o) noexcept
        : cmp_(std::move(o.cmp_)), storage_(std::move(o.storage_)) {}

    IntervalMap& operator=(IntervalMap o) noexcept {
        swap(*this, o);
        return *this;
    }

    friend void swap(IntervalMap& a, IntervalMap& b) noexcept {
        using std::swap;
        swap(a.storage_, b.storage_);
        swap(a.cmp_,     b.cmp_);
    }

    //<----------------------------------------------------------------------->
    // Range accessors
    //<----------------------------------------------------------------------->

    iterator       begin()        { return iterator(storage_.begin()); }
    iterator       end()          { return iterator(storage_.end());   }
    const_iterator begin()  const { return const_iterator(storage_.begin()); }
    const_iterator end()    const { return const_iterator(storage_.end());   }
    const_iterator cbegin() const { return begin(); }
    const_iterator cend()   const { return end();   }

    bool  empty() const { return storage_.empty(); }
    std::size_t size()  const { return storage_.size(); }

    //<----------------------------------------------------------------------->
    // insert(Start, Stop, Val)  - core implementation
    //
    // Inserts [Start, Stop) -> Val, trimming any overlapping intervals.
    // Returns an iterator to the newly inserted node, or end() on failure
    // (e.g., Start >= Stop).
    //<----------------------------------------------------------------------->

    /// Lvalue reference overload.
    iterator insert(const KeyT& Start, const KeyT& Stop, const ValT& Val) {
        return insert_impl(Start, Stop, Val);
    }

    /// Rvalue reference overload (moves Val into the node).
    iterator insert(const KeyT& Start, const KeyT& Stop, ValT&& Val) {
        return insert_impl(Start, Stop, std::move(Val));
    }

    /// Lvalue single-key insert: wraps [Key, ++Key).
    iterator insert(const KeyT& Key, const ValT& Val) {
        KeyT stop = Key;
        ++stop;
        return insert(Key, stop, Val);
    }

    /// Rvalue single-key insert.
    iterator insert(const KeyT& Key, ValT&& Val) {
        KeyT stop = Key;
        ++stop;
        return insert(Key, stop, std::move(Val));
    }

    //<----------------------------------------------------------------------->
    //  find - return iterator to interval that contains x, or end()
    //<----------------------------------------------------------------------->

    iterator find(const KeyT& x) {
        // Find the first interval whose start > x, then step back one.
        auto it = storage_.upper_bound(x);
        if (it == storage_.begin()) return end();
        --it;
        // it->second->start <= x; check x < stop
        if (lt(x, it->second->stop))
            return iterator(it);
        return end();
    }

    const_iterator find(const KeyT& x) const {
        auto it = storage_.upper_bound(x);
        if (it == storage_.begin()) return cend();
        --it;
        if (lt(x, it->second->stop))
            return const_iterator(it);
        return cend();
    }

    //<----------------------------------------------------------------------->
    // lookup - return value reference for interval containing x
    //          throws std::out_of_range if x is not covered
    //<----------------------------------------------------------------------->

    const ValT& lookup(const KeyT& x) const {
        auto it = find(x);
        if (it == cend())
            throw std::out_of_range("IntervalMap::lookup: key not found");
        return it->val;
    }

    //<----------------------------------------------------------------------->
    //  operator[] - read-only, delegates to lookup
    //<----------------------------------------------------------------------->

    const ValT& operator[](const KeyT& Key) const {
        return lookup(Key);
    }


    //<----------------------------------------------------------------------->
    // find_all - return a lazily-clipped, coalescing view of [Start, Stop)
    //
    // Mutable overload (iterator_collection)
    // ───────────────────────────────────────
    // . Boundary-straddling nodes are split so every collected iterator is
    //   fully confined to [Start, Stop).
    // . On destruction, adjacent equal-valued collected nodes are merged, and
    //   the surviving boundary nodes are checked against their immediate map
    //   neighbours (i.e., the pre-split remnants) for further merging.
    //
    // Const overload (const_iterator_collection)
    // ───────────────────────────────────────────
    // . No splits are performed; nodes may extend beyond [Start, Stop).
    // . No coalescing occurs on destruction.
    //
    // Both overloads return an empty collection when Start >= Stop or no
    // nodes overlap the query range.
    //<----------------------------------------------------------------------->
 
    /// Mutable view: splits boundary nodes, coalesces on destruction.
    iterator_collection find_all(const KeyT& Start, const KeyT& Stop) {
        return iterator_collection(*this, Start, Stop);
    }
 
    /// Read-only view: no splits, no coalescing.
    const_iterator_collection find_all(const KeyT& Start, const KeyT& Stop) const {
        return const_iterator_collection(*this, Start, Stop);
    }


    //<----------------------------------------------------------------------->
    // erase - remove the node at pos and return an iterator to the next node.
    //
    // Loop-safe contract
    //.------------------
    // The returned iterator points to the successor of the erased node, so
    // callers can use the idiomatic erase-in-loop pattern safely:
    //
    //   for (auto it = m.begin(); it != m.end(); )
    //       it = should_remove(*it) ? m.erase(it) : ++it;
    //
    // Passing end() is undefined behaviour (mirroring std::map::erase).
    //
    // The mutable iterator overload also updates the caller's pos in-place
    // (it becomes the successor) so the return value can be ignored if the
    // caller prefers to test pos directly after the call.
    //
    // const_iterator overload
    //.-----------------------
    // A const_iterator gives read-only node access but the map is mutable,
    // so erasure through one is legal.  Because const_iterator holds a
    // const StorageIter we cannot update the caller's handle; the overload
    // therefore accepts by value and the caller must use the return value.
    //<----------------------------------------------------------------------->
 
    /// Erase the node at \p pos.
    /// \p pos is updated to the next iterator in place (loop-safe).
    /// \return Iterator to the element following the erased one.
    iterator erase(iterator& pos) {
        // Advance to successor *before* invalidating cur.
        StorageIter cur  = pos.it_;
        StorageIter next = std::next(cur);
        delete cur->second;
        storage_.erase(cur);
        // Repair the caller's iterator so it is not left dangling.
        pos.it_ = next;
        return iterator(next);
    }
 
    /// Erase the node at \p pos given as a const_iterator (accepted by value).
    /// \return Iterator to the element following the erased one.
    iterator erase(const_iterator pos) {
        // Convert the const storage iterator to a mutable one via the key.
        // storage_.find() is O(log N) and guaranteed to succeed for a valid pos.
        StorageIter cur  = storage_.find(pos.it_->second->start);
        StorageIter next = std::next(cur);
        delete cur->second;
        storage_.erase(cur);
        return iterator(next);
    }

    //<----------------------------------------------------------------------->
    // erase(Start, Stop) - range erasure over [Start, Stop)
    //
    // Removes all coverage in the half-open interval [Start, Stop):
    //
    //   . A stored interval completely contained within [Start, Stop) is
    //     deleted outright.
    //   . A stored interval that straddles Start from the left is trimmed:
    //     its stop is set to Start (its left portion is kept).
    //   . A stored interval that straddles Stop from the right is trimmed:
    //     its start is advanced to Stop and re-keyed (its right portion is kept).
    //   . A single stored interval that spans both Start and Stop (i.e. the
    //     erased range lies entirely inside it) is split into two: a left
    //     remnant [node.start, Start) and a right remnant [Stop, node.stop),
    //     both carrying the original value.
    //
    // If Start >= Stop the call is a no-op (mirrors the insert contract).
    //
    // Returns an iterator to the first node that begins at or after Stop,
    // or end() if none exists - providing a stable re-entry point for
    // callers that want to continue iterating after the erased region.
    //
    // Complexity: O(K log N) where K is the number of affected intervals.
    //<----------------------------------------------------------------------->
 
    /// Erase all coverage in [Start, Stop).
    /// \return Iterator to the first node whose start >= Stop, or end().
    iterator erase(const KeyT& Start, const KeyT& Stop) {
        return erase_range_impl(Start, Stop);
    }
 
    //<----------------------------------------------------------------------->
    // clear - remove all intervals and reset internal state
    //
    // Every heap-allocated Node is destroyed and the underlying storage map
    // is cleared.  The comparator is preserved so the object is immediately
    // ready to accept new insertions with the same ordering.
    //
    // Complexity: O(N) - every node must be individually deleted.
    //<----------------------------------------------------------------------->
 
    /// Remove all intervals.  The object is left empty but fully usable.
    void clear() noexcept {
        for (auto& [k, n] : storage_) delete n;
        storage_.clear();
    }

private:
    //<----------------------------------------------------------------------->
    //  insert_impl - shared template core for lvalue/rvalue Val
    //<----------------------------------------------------------------------->
    template<typename V>
    iterator insert_impl(const KeyT& Start, const KeyT& Stop, V&& Val) {
        // Reject empty or inverted ranges.
        if (!lt(Start, Stop)) return end();

        //<----------------------------------------------------------------->
        // Step 1: handle all intervals whose start lies strictly before Stop
        //         AND whose stop is strictly after Start  (overlapping set).
        //
        // We want to keep this case analysis clear with named variables.
        //<----------------------------------------------------------------->

        // Find the first interval with start >= Start.
        auto lo = storage_.lower_bound(Start);

        // Also check the interval just before lo - it may start before Start
        // but extend into our new range.
        if (lo != storage_.begin()) {
            auto prev = std::prev(lo);
            Node* pn = prev->second;
            if (!lt(pn->stop, Start) && !eq(pn->stop, Start)) {
                // pn overlaps our new range from the left.
                if (lt(Stop, pn->stop) || eq(Stop, pn->stop)) {
                    // New range is completely inside pn: split pn into two.
                    //   pn becomes [pn->start, Start)
                    //   We insert [Start, Stop) -> new val
                    //   Then [Stop, pn->stop) -> pn->val (right remainder)
                    KeyT  right_start = Stop;           // copy before pn changes
                    KeyT  right_stop  = pn->stop;
                    ValT  right_val   = pn->val;        // copy value for right tail

                    pn->stop = Start;                   // trim pn in-place

                    // Insert right remainder first (it's after our new node).
                    Node* right_node = new Node(right_start, right_stop, std::move(right_val));
                    storage_.emplace(right_start, right_node);

                    // Now insert the new node.
                    Node* new_node = new Node(Start, Stop, std::forward<V>(Val));
                    auto [ins_it, ok] = storage_.emplace(Start, new_node);
                    (void)ok;
                    return iterator(ins_it);
                }
                // New range extends past pn->stop: trim pn's stop to Start.
                pn->stop = Start;
                // lo stays pointing to the next candidate.
            }
        }

        //<----------------------------------------------------------------->
        //  Step 2: remove / trim intervals whose start >= Start and < Stop.
        //<----------------------------------------------------------------->
        auto it = lo;
        while (it != storage_.end() && lt(it->second->start, Stop)) {
            Node* n = it->second;

            if (lt(n->stop, Stop) || eq(n->stop, Stop)) {
                // Interval fully covered by new range - delete it.
                it = erase_node(it);
            } else {
                // Interval extends past Stop - trim its start to Stop.
                Node* moved = n;
                it = storage_.erase(it);          // remove old key entry
                moved->start = Stop;
                storage_.emplace(Stop, moved);    // re-insert under new key
                break;
            }
        }

        //<----------------------------------------------------------------->
        //  Step 3: insert the new interval.
        //<----------------------------------------------------------------->
        Node* new_node = new Node(Start, Stop, std::forward<V>(Val));
        auto [ins_it, ok] = storage_.emplace(Start, new_node);
        if (!ok) {
            // A node with exactly the same start already exists (edge case
            // after trimming).  Overwrite it.
            delete ins_it->second;
            ins_it->second = new_node;
        }
        return iterator(ins_it);
    }

    //<----------------------------------------------------------------------->
    // erase_range_impl - shared core for erase(Start, Stop)
    //
    // Case analysis (identical in structure to insert_impl step 1 & 2,
    // but without inserting anything):
    //
    //  Left-straddle  : stored interval starts before Start, stops inside
    //                   [Start, Stop) or at/past Stop.
    //    . If it stops <= Stop  → trim its stop to Start.
    //    . If it stops >  Stop  → split: left piece [n.start, Start),
    //                             right piece [Stop, n.stop) re-keyed.
    //
    //  Interior       : stored interval starts >= Start and stops <= Stop
    //                   → delete entirely.
    //
    //  Right-straddle : stored interval starts inside [Start, Stop) but
    //                   stops past Stop → advance its start to Stop & re-key.
    //<----------------------------------------------------------------------->
    iterator erase_range_impl(const KeyT& Start, const KeyT& Stop) {
        // No-op on empty or inverted range.
        if (!lt(Start, Stop)) return end();
 
        //<------------------------------------------------------------------>
        // Step 1: handle the interval (if any) whose start < Start but whose
        //         stop > Start - i.e. it overlaps from the left.
        //<------------------------------------------------------------------>
        auto lo = storage_.lower_bound(Start);
 
        if (lo != storage_.begin()) {
            auto prev = std::prev(lo);
            Node* pn  = prev->second;
 
            // Does pn extend into [Start, Stop)?
            if (lt(Start, pn->stop)) {              // pn->stop > Start
                if (lt(Stop, pn->stop) || eq(Stop, pn->stop)) {
                    // Erased range lies entirely inside pn → split pn.
                    //   Left  piece: [pn->start, Start)  - trim pn in place.
                    //   Right piece: [Stop, pn->stop)    - new node, re-keyed.
                    KeyT right_start = Stop;
                    KeyT right_stop  = pn->stop;
                    ValT right_val   = pn->val;          // copy for right tail
 
                    pn->stop = Start;                    // trim left piece
 
                    Node* right_node = new Node(right_start, right_stop,
                                                std::move(right_val));
                    storage_.emplace(right_start, right_node);
 
                    // Nothing left to scan - return iterator to right piece.
                    return iterator(storage_.find(right_start));
                }
                // pn straddles only the left boundary: trim its stop to Start.
                pn->stop = Start;
                // lo is still valid; fall through to step 2.
            }
        }
 
        //<------------------------------------------------------------------>
        // Step 2: sweep through intervals whose start >= Start and < Stop.
        //<------------------------------------------------------------------>
        auto it = lo;
        while (it != storage_.end() && lt(it->second->start, Stop)) {
            Node* n = it->second;
 
            if (lt(n->stop, Stop) || eq(n->stop, Stop)) {
                // Fully contained - delete.
                it = erase_node(it);
            } else {
                // Right-straddle: starts inside the erase range, stops past it.
                // Re-key the node to Stop.
                it = storage_.erase(it);        // remove old entry (no delete)
                n->start = Stop;
                storage_.emplace(Stop, n);      // re-insert at new start key
                break;
            }
        }
 
        // Return iterator to the first node at or after Stop.
        return iterator(storage_.lower_bound(Stop));
    }

    //<----------------------------------------------------------------------->
    // coalesce_impl - merge adjacent equal-valued nodes after find_all
    //
    // Called by ~iterator_collection().  The iters vector holds StorageIters
    // in ascending key order (built by a forward sweep in the constructor).
    //
    // Phase 1 - intra-collection merge
    //   Walk pairs (iters[i], iters[i+1]).  When cur->stop == next->start
    //   and cur->val == next->val, extend cur's stop to next->stop and delete
    //   next.  Otherwise advance the write cursor.
    //
    // Phase 2 - left-neighbour check
    //   The leftmost surviving iterator may be adjacent to the left-split
    //   remnant produced during construction.  If values match, absorb the
    //   collection node into the remnant (the remnant becomes the survivor so
    //   that its key in the map remains valid).
    //
    // Phase 3 - right-neighbour check
    //   The rightmost surviving iterator may be adjacent to the right-split
    //   remnant.  If values match, extend the survivor's stop and delete the
    //   remnant.
    //
    // Complexity: O(K log N) where K = |iters|.
    //<----------------------------------------------------------------------->
    void coalesce_impl(std::vector<StorageIter>& iters) {
        if (iters.empty()) return;
 
        // ── Phase 1: intra-collection merge ──────────────────────────────────
        // Compact iters[] in-place using a two-pointer approach.
        std::size_t write = 0;
        for (std::size_t read = 1; read < iters.size(); ++read) {
            Node* cur  = iters[write]->second;
            Node* next = iters[read]->second;
            if (eq(cur->stop, next->start) && cur->val == next->val) {
                // Absorb next into cur: extend stop, delete next's node and entry.
                cur->stop = next->stop;
                delete next;
                storage_.erase(iters[read]);
                // iters[write] survives unchanged; don't advance write.
            } else {
                iters[++write] = iters[read];
            }
        }
        iters.resize(write + 1);
 
        // ── Phase 2: merge leftmost survivor with its left map neighbour ──────
        {
            StorageIter& first = iters.front();
            if (first != storage_.begin()) {
                StorageIter left = std::prev(first);
                Node* ln = left->second;
                Node* fn = first->second;
                if (eq(ln->stop, fn->start) && ln->val == fn->val) {
                    // Absorb fn into ln: extend ln's stop and delete fn.
                    // ln's key is earlier so it remains the map entry.
                    ln->stop = fn->stop;
                    delete fn;
                    storage_.erase(first);
                    first = left;  // update so Phase 3 starts from the right place
                }
            }
        }
 
        // ── Phase 3: merge rightmost survivor with its right map neighbour ────
        {
            StorageIter last  = iters.back();
            StorageIter right = std::next(last);
            if (right != storage_.end()) {
                Node* ln = last->second;
                Node* rn = right->second;
                if (eq(ln->stop, rn->start) && ln->val == rn->val) {
                    // Absorb rn into ln: extend ln's stop and delete rn.
                    ln->stop = rn->stop;
                    delete rn;
                    storage_.erase(right);
                }
            }
        }
    }
};

//<----------------------------------------------------------------------->
// iterator_collection - mutable ordered view of [Start, Stop)
//
// Constructed only by IntervalMap::find_all().  On construction:
//   . Any node that straddles Start from the left is split into a left
//     remnant [node.start, Start) (stays in map, not collected) and a
//     collected piece [Start, min(node.stop, Stop)).  If that piece does
//     not yet reach Stop, a further right-split is performed during the
//     interior sweep.
//   . Any node that straddles Stop from the right is split into a
//     collected piece [node.start, Stop) and a right remnant [Stop,
//     node.stop) (stays in map, not collected).
//   . All other nodes fully contained in [Start, Stop) are collected
//     without modification.
//
// On destruction, coalesces: adjacent collected nodes whose values are
// equal are merged into a single node (the left one survives, its stop
// is extended, the right one is deleted).  After the intra-collection
// merge, the surviving boundary nodes are checked against their immediate
// map neighbours (which may be the pre-split remnants from construction)
// for the same merge opportunity.  This is safe because at most one
// neighbour on each side can be a remnant, and a remnant always carries
// the same value as the node it was split from.
//
// Not copyable (owns structural mutations to the parent map); movable.
//<----------------------------------------------------------------------->
template<typename KeyT, typename ValT, 
        class KeyComparator, class KeyEquator, class ValEquator>
class IntervalMap<KeyT,ValT,
                  KeyComparator,
                  KeyEquator,
                  ValEquator>::iterator_collection {
    IntervalMap*             map_;    ///< owning map (nullptr after move)
    std::vector<StorageIter> iters_;  ///< ordered collection of iterators

    friend class IntervalMap;

    // Private constructor - called only from IntervalMap::find_all().
    iterator_collection(IntervalMap& m, const KeyT& Start, const KeyT& Stop)
        : map_(&m)
    {
        if (!m.lt(Start, Stop)) return;  // empty / inverted range

        //<---------------------------------------------------------------->
        // Step 1 - left-boundary split
        //
        // lower_bound(Start) gives the first node whose start >= Start.
        // The node immediately before it (if any) may straddle Start.
        //<---------------------------------------------------------------->
        auto lo = m.storage_.lower_bound(Start);

        if (lo != m.storage_.begin()) {
            auto prev_it = std::prev(lo);
            Node* pn     = prev_it->second;

            if (m.lt(Start, pn->stop)) {
                // pn straddles Start.  Split into:
                //   left remnant  [pn->start, Start)  - trim pn in-place
                //   split piece   [Start, pn->stop)   - new node, collected

                KeyT split_start = Start;
                KeyT split_stop  = pn->stop;  // full extent of original right half
                ValT split_val   = pn->val;   // copy

                pn->stop = Start;             // trim left remnant in place

                Node* split_node = new Node(split_start, split_stop,
                                            std::move(split_val));
                auto [sit, ok] = m.storage_.emplace(split_start, split_node);
                (void)ok;
                lo = sit;   // lo now points at the new split piece
            }
        }

        //<---------------------------------------------------------------->
        // Step 2 - interior sweep + right-boundary split
        //
        // Walk from lo forward, collecting nodes whose start < Stop.
        // The last node in the sweep may straddle Stop and must be split.
        //<---------------------------------------------------------------->
        for (auto it = lo;
              it != m.storage_.end() && m.lt(it->second->start, Stop);
              ++it)
        {
            Node* n = it->second;

            if (m.lt(Stop, n->stop)) {
                // n straddles Stop - right-boundary split.
                // Trim n to [n->start, Stop) and insert a right remnant
                // [Stop, n->stop) that stays in the map but is not collected.
                KeyT right_start = Stop;
                KeyT right_stop  = n->stop;
                ValT right_val   = n->val;  // copy for right remnant

                n->stop = Stop;             // clip n in place (still in map)
                iters_.push_back(it);       // collect the clipped node

                Node* right_node = new Node(right_start, right_stop,
                                            std::move(right_val));
                m.storage_.emplace(right_start, right_node);
                break;  // nothing past Stop is relevant
            }

            // n is fully contained in [Start, Stop) - collect as-is.
            iters_.push_back(it);
        }
    }

public:
    // Not copyable - owns structural mutations that must be coalesced once.
    iterator_collection(const iterator_collection&)            = delete;
    iterator_collection& operator=(const iterator_collection&) = delete;

    iterator_collection(iterator_collection&& o) noexcept
        : map_(o.map_), iters_(std::move(o.iters_))
    { o.map_ = nullptr; }

    iterator_collection& operator=(iterator_collection&& o) noexcept {
        if (this != &o) {
            if (map_ && !iters_.empty()) map_->coalesce_impl(iters_);
            map_   = o.map_;
            iters_ = std::move(o.iters_);
            o.map_ = nullptr;
        }
        return *this;
    }

    /// Coalesce adjacent equal-valued nodes back into the map.
    ~iterator_collection() {
        if (map_ && !iters_.empty())
            map_->coalesce_impl(iters_);
    }

    // ── range-for support ────────────────────────────────────────────────
    // The proxy wraps a vector<StorageIter>::iterator so that range-for
    // yields an IntervalMap::iterator (preserving the public interface)
    // rather than a raw StorageIter.
    struct iter_proxy {
        using inner = typename std::vector<StorageIter>::iterator;
        inner pos;
        IntervalMap::iterator operator*()  const { return IntervalMap::iterator(*pos); }
        iter_proxy& operator++()                 { ++pos; return *this; }
        bool operator!=(const iter_proxy& o) const { return pos != o.pos; }
    };

    iter_proxy begin() { return {iters_.begin()}; }
    iter_proxy end()   { return {iters_.end()};   }

    bool        empty() const { return iters_.empty(); }
    std::size_t size()  const { return iters_.size();  }
};

//<----------------------------------------------------------------------->
// const_iterator_collection - read-only ordered view of [Start, Stop)
//
// Constructed only by IntervalMap::find_all() const.
//
// Because the map is const:
//   . No splits are performed.  Iterators may therefore point at nodes
//     that extend beyond [Start, Stop) (i.e., boundary-straddling nodes
//     are included whole).
//   . No coalescing occurs on destruction.
//
// Copyable and trivially destructible.
//<----------------------------------------------------------------------->
template<typename KeyT, typename ValT, 
        class KeyComparator, class KeyEquator, class ValEquator>
class IntervalMap<KeyT,ValT,
                  KeyComparator,
                  KeyEquator,
                  ValEquator>::const_iterator_collection {
    using CStorageIter = typename StorageMap::const_iterator;
    std::vector<CStorageIter> iters_;

    friend class IntervalMap;

    const_iterator_collection(const IntervalMap& m,
                                const KeyT& Start, const KeyT& Stop)
    {
        if (!m.lt(Start, Stop)) return;

        // Include the predecessor if it straddles Start.
        auto lo = m.storage_.upper_bound(Start);
        if (lo != m.storage_.begin()) {
            auto prev_it = std::prev(lo);
            if (m.lt(Start, prev_it->second->stop))
                lo = prev_it;
        }

        // Collect all nodes whose start < Stop (the predecessor, if
        // included above, satisfies start < Start < Stop).
        for (auto it = lo;
              it != m.storage_.end() && m.lt(it->second->start, Stop);
              ++it)
        {
            iters_.push_back(it);
        }
    }

public:
    const_iterator_collection()                                             = default;
    const_iterator_collection(const const_iterator_collection&)             = default;
    const_iterator_collection& operator=(const const_iterator_collection&)  = default;
    const_iterator_collection(const_iterator_collection&&)                  = default;
    const_iterator_collection& operator=(const_iterator_collection&&)       = default;
    ~const_iterator_collection()                                            = default;

    // ── range-for support ────────────────────────────────────────────────
    struct iter_proxy {
        using inner = typename std::vector<CStorageIter>::const_iterator;
        inner pos;
        IntervalMap::const_iterator operator*()  const { return IntervalMap::const_iterator(*pos); }
        iter_proxy& operator++()                       { ++pos; return *this; }
        bool operator!=(const iter_proxy& o) const    { return pos != o.pos; }
    };

    iter_proxy begin() const { return {iters_.begin()}; }
    iter_proxy end()   const { return {iters_.end()};   }

    bool        empty() const { return iters_.empty(); }
    std::size_t size()  const { return iters_.size();  }
};

} //? namespace rtl
} //? namespace scabbard
