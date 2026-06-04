// IntervalMap_test.cpp — comprehensive tests for IntervalMap
// Compile: g++ -std=c++17 -Wall -Wextra -o interval_test IntervalMap_test.cpp && ./interval_test

#include "IntervalMap.h"
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

// ── tiny test harness ──────────────────────────────────────────────────────
static int  g_pass = 0, g_fail = 0;
#define EXPECT(cond) \
    do { if (cond) { ++g_pass; } \
         else { ++g_fail; std::cerr << "FAIL  " << __FILE__ << ":" << __LINE__ \
                                    << "  " #cond "\n"; } } while(0)
#define SECTION(name) std::cout << "\n[" << (name) << "]\n"
// ──────────────────────────────────────────────────────────────────────────

// ── helpers ───────────────────────────────────────────────────────────────

// Collect all nodes into a vector for easy comparison.
template<class IM>
std::vector<std::tuple<int,int,std::string>> snapshot(const IM& m) {
    std::vector<std::tuple<int,int,std::string>> v;
    for (auto it = m.begin(); it != m.end(); ++it)
        v.emplace_back(it->start, it->stop, it->val);
    return v;
}

// ── Test groups ───────────────────────────────────────────────────────────

void test_basic_insert_and_lookup() {
    SECTION("Basic insert and lookup");

    IntervalMap<int,std::string> m;

    auto it = m.insert(1, 5, std::string("A"));
    EXPECT(it != m.end());
    EXPECT(it->start == 1);
    EXPECT(it->stop  == 5);
    EXPECT(it->val   == "A");

    // lookup inside the interval
    EXPECT(m.lookup(1) == "A");
    EXPECT(m.lookup(3) == "A");
    EXPECT(m.lookup(4) == "A");   // last point in [1,5)

    // lookup outside the interval should throw
    bool threw = false;
    try { m.lookup(5); } catch (const std::out_of_range&) { threw = true; }
    EXPECT(threw);

    threw = false;
    try { m.lookup(0); } catch (const std::out_of_range&) { threw = true; }
    EXPECT(threw);
}

void test_non_overlapping_inserts() {
    SECTION("Non-overlapping inserts");

    IntervalMap<int,std::string> m;
    m.insert(1,  5, std::string("A"));
    m.insert(10, 20, std::string("B"));
    m.insert(30, 40, std::string("C"));

    EXPECT(m.size() == 3);
    EXPECT(m.lookup(1)  == "A");
    EXPECT(m.lookup(4)  == "A");
    EXPECT(m.lookup(10) == "B");
    EXPECT(m.lookup(19) == "B");
    EXPECT(m.lookup(30) == "C");
    EXPECT(m.lookup(39) == "C");

    bool threw = false;
    try { m.lookup(5); } catch (const std::out_of_range&) { threw = true; }
    EXPECT(threw);

    threw = false;
    try { m.lookup(20); } catch (const std::out_of_range&) { threw = true; }
    EXPECT(threw);
}

void test_overlap_new_overwrites_left_partial() {
    SECTION("Overlap: new range extends beyond left neighbour");

    // Existing: [1,10) "A"
    // Insert  : [5,15) "B"
    // Expected: [1,5) "A", [5,15) "B"
    IntervalMap<int,std::string> m;
    m.insert(1, 10, std::string("A"));
    m.insert(5, 15, std::string("B"));

    EXPECT(m.size() == 2);
    EXPECT(m.lookup(1) == "A");
    EXPECT(m.lookup(4) == "A");
    EXPECT(m.lookup(5) == "B");
    EXPECT(m.lookup(14)== "B");
}

void test_overlap_new_overwrites_right_partial() {
    SECTION("Overlap: new range starts before right neighbour");

    // Existing: [10,20) "B"
    // Insert  : [5,15)  "C"
    // Expected: [5,15) "C", [15,20) "B"
    IntervalMap<int,std::string> m;
    m.insert(10, 20, std::string("B"));
    m.insert(5,  15, std::string("C"));

    EXPECT(m.size() == 2);
    EXPECT(m.lookup(5)  == "C");
    EXPECT(m.lookup(14) == "C");
    EXPECT(m.lookup(15) == "B");
    EXPECT(m.lookup(19) == "B");
}

void test_overlap_new_inside_existing() {
    SECTION("Overlap: new range entirely inside existing → split");

    // Existing: [1,20) "A"
    // Insert  : [5,10) "B"
    // Expected: [1,5) "A", [5,10) "B", [10,20) "A"
    IntervalMap<int,std::string> m;
    m.insert(1,  20, std::string("A"));
    m.insert(5,  10, std::string("B"));

    EXPECT(m.size() == 3);
    EXPECT(m.lookup(1)  == "A");
    EXPECT(m.lookup(4)  == "A");
    EXPECT(m.lookup(5)  == "B");
    EXPECT(m.lookup(9)  == "B");
    EXPECT(m.lookup(10) == "A");
    EXPECT(m.lookup(19) == "A");
}

void test_overlap_new_covers_multiple() {
    SECTION("Overlap: new range covers multiple existing intervals");

    // Existing: [1,5) "A", [5,10) "B", [10,15) "C"
    // Insert  : [3,12) "X"
    // Expected: [1,3) "A", [3,12) "X", [12,15) "C"
    IntervalMap<int,std::string> m;
    m.insert(1,  5,  std::string("A"));
    m.insert(5,  10, std::string("B"));
    m.insert(10, 15, std::string("C"));
    m.insert(3,  12, std::string("X"));

    EXPECT(m.size() == 3);
    EXPECT(m.lookup(1)  == "A");
    EXPECT(m.lookup(2)  == "A");
    EXPECT(m.lookup(3)  == "X");
    EXPECT(m.lookup(11) == "X");
    EXPECT(m.lookup(12) == "C");
    EXPECT(m.lookup(14) == "C");
}

void test_overlap_exact_replace() {
    SECTION("Overlap: new range exactly replaces existing");

    IntervalMap<int,std::string> m;
    m.insert(5, 10, std::string("A"));
    m.insert(5, 10, std::string("B"));

    EXPECT(m.size() == 1);
    EXPECT(m.lookup(5) == "B");
    EXPECT(m.lookup(9) == "B");
}

void test_adjacent_inserts() {
    SECTION("Adjacent (touching) inserts do not merge");

    IntervalMap<int,std::string> m;
    m.insert(1, 5,  std::string("A"));
    m.insert(5, 10, std::string("B"));

    EXPECT(m.size() == 2);
    EXPECT(m.lookup(4) == "A");
    EXPECT(m.lookup(5) == "B");
}

void test_invalid_range_rejected() {
    SECTION("Invalid range (Start >= Stop) is rejected");

    IntervalMap<int,std::string> m;
    auto it = m.insert(5, 5, std::string("A"));   // empty range
    EXPECT(it == m.end());
    EXPECT(m.empty());

    it = m.insert(10, 5, std::string("A"));       // inverted range
    EXPECT(it == m.end());
    EXPECT(m.empty());
}

void test_find() {
    SECTION("find()");

    IntervalMap<int,std::string> m;
    m.insert(1, 5,   std::string("A"));
    m.insert(10, 20, std::string("B"));

    auto it = m.find(3);
    EXPECT(it != m.end());
    EXPECT(it->val == "A");

    it = m.find(15);
    EXPECT(it != m.end());
    EXPECT(it->val == "B");

    it = m.find(5);   // boundary: 5 is NOT in [1,5)
    EXPECT(it == m.end());

    it = m.find(7);   // gap
    EXPECT(it == m.end());

    // const overload
    const auto& cm = m;
    auto cit = cm.find(3);
    EXPECT(cit != cm.end());
    EXPECT(cit->val == "A");
}

void test_operator_bracket() {
    SECTION("operator[]");

    IntervalMap<int,std::string> m;
    m.insert(1, 5, std::string("A"));

    const auto& cm = m;
    EXPECT(cm[1] == "A");
    EXPECT(cm[4] == "A");

    bool threw = false;
    try { (void)cm[5]; } catch (const std::out_of_range&) { threw = true; }
    EXPECT(threw);
}

void test_single_key_insert() {
    SECTION("Single-key insert (wraps [Key, ++Key))");

    IntervalMap<int,std::string> m;
    auto it = m.insert(7, std::string("Z"));
    EXPECT(it != m.end());
    EXPECT(it->start == 7);
    EXPECT(it->stop  == 8);
    EXPECT(m.lookup(7) == "Z");

    bool threw = false;
    try { m.lookup(8); } catch (const std::out_of_range&) { threw = true; }
    EXPECT(threw);
}

void test_move_insert() {
    SECTION("Rvalue / move insert");

    IntervalMap<int,std::string> m;

    std::string val = "MoveMe";
    auto it = m.insert(1, 5, std::move(val));
    EXPECT(it != m.end());
    EXPECT(it->val == "MoveMe");

    // single-key move variant
    std::string val2 = "MoveKey";
    it = m.insert(20, std::move(val2));
    EXPECT(it->start == 20);
    EXPECT(it->val   == "MoveKey");
}

void test_iterators() {
    SECTION("Iterators: forward traversal and bidirectional");

    IntervalMap<int,std::string> m;
    m.insert(1,  5,  std::string("A"));
    m.insert(5,  10, std::string("B"));
    m.insert(10, 20, std::string("C"));

    // Forward traversal
    std::vector<std::string> vals;
    for (auto it = m.begin(); it != m.end(); ++it)
        vals.push_back(it->val);
    EXPECT(vals == (std::vector<std::string>{"A","B","C"}));

    // Range-for
    vals.clear();
    for (const auto& node : m)
        vals.push_back(node.val);
    EXPECT(vals == (std::vector<std::string>{"A","B","C"}));

    // Bidirectional: step back from end
    auto it = m.end();
    --it;
    EXPECT(it->val == "C");
    --it;
    EXPECT(it->val == "B");
    --it;
    EXPECT(it->val == "A");

    // Post-increment
    it = m.begin();
    auto prev = it++;
    EXPECT(prev->val == "A");
    EXPECT(it->val   == "B");
}

void test_const_iterator_conversion() {
    SECTION("const_iterator implicit conversion from iterator");

    IntervalMap<int,std::string> m;
    m.insert(1, 5, std::string("X"));

    IntervalMap<int,std::string>::iterator it = m.begin();
    IntervalMap<int,std::string>::const_iterator cit = it;  // implicit convert
    EXPECT(cit->val == "X");
}

void test_copy_and_move() {
    SECTION("Copy and move construction/assignment");

    IntervalMap<int,std::string> m;
    m.insert(1,  5,  std::string("A"));
    m.insert(10, 20, std::string("B"));

    // copy
    IntervalMap<int,std::string> m2 = m;
    EXPECT(m2.lookup(3)  == "A");
    EXPECT(m2.lookup(15) == "B");
    EXPECT(m2.size() == 2);

    // mutating copy does not affect original
    m2.insert(1, 5, std::string("Z"));
    EXPECT(m.lookup(3) == "A");

    // move
    IntervalMap<int,std::string> m3 = std::move(m2);
    EXPECT(m3.lookup(3)  == "Z");
    EXPECT(m3.lookup(15) == "B");
}

void test_custom_comparator() {
    SECTION("Custom comparator (std::greater — reversed ordering)");

    // With std::greater<int>, cmp_(a,b) == true when a > b numerically.
    // A valid interval [Start, Stop) requires cmp_(Start, Stop) == true,
    // i.e. Start > Stop numerically.  So [20, 10) is the valid form here.
    IntervalMap<int, std::string, std::greater<int>> m;

    auto it = m.insert(20, 10, std::string("A"));
    EXPECT(it != m.end());
    EXPECT(m.find(15) != m.end());
    EXPECT(m.lookup(15) == "A");

    // A "normal" ascending range is invalid under this comparator.
    auto bad = m.insert(10, 20, std::string("B"));
    EXPECT(bad == m.end());
    EXPECT(m.size() == 1);
}

void test_string_key() {
    SECTION("String keys");

    IntervalMap<std::string, int> m;
    m.insert("apple", "cherry", 1);   // ["apple","cherry")
    m.insert("mango", "peach",  2);

    EXPECT(m.lookup("banana") == 1);
    EXPECT(m.lookup("mango")  == 2);
    EXPECT(m.lookup("orange") == 2);

    bool threw = false;
    try { m.lookup("cherry"); } catch (const std::out_of_range&) { threw = true; }
    EXPECT(threw);
}

void test_dense_coverage_then_clear() {
    SECTION("Dense coverage then a single spanning insert");

    IntervalMap<int,std::string> m;
    for (int i = 0; i < 10; ++i)
        m.insert(i*5, i*5+5, std::string(1, 'A'+i));  // [0,5), [5,10), ...

    EXPECT(m.size() == 10);

    // Overwrite everything with one big range
    m.insert(0, 50, std::string("X"));
    EXPECT(m.size() == 1);
    EXPECT(m.lookup(0)  == "X");
    EXPECT(m.lookup(49) == "X");
}

void test_node_members_public() {
    SECTION("Node public members: start, stop, val");

    IntervalMap<int,std::string> m;
    m.insert(3, 9, std::string("T"));

    auto it = m.begin();
    // All three members must be accessible directly (public).
    [[maybe_unused]] const int&         s = it->start;
    [[maybe_unused]] const int&         e = it->stop;
    [[maybe_unused]] const std::string& v = it->val;
    EXPECT(s == 3 && e == 9 && v == "T");
}

void test_erase_iterator() {
    SECTION("erase(iterator&): single node, pos updated in-place");

    IntervalMap<int,std::string> m;
    m.insert(1,  5,  std::string("A"));
    m.insert(5,  10, std::string("B"));
    m.insert(10, 20, std::string("C"));

    // Erase the middle node via mutable iterator.
    auto it = m.begin();
    ++it;  // points at "B"
    EXPECT(it->val == "B");

    auto next = m.erase(it);  // returns iterator to "C"; it is also updated

    EXPECT(m.size() == 2);
    EXPECT(next != m.end());
    EXPECT(next->val == "C");
    // pos itself should now equal the returned next (loop-safe in-place update)
    EXPECT(it == next);

    // Confirm "B" is gone
    bool threw = false;
    try { m.lookup(7); } catch (const std::out_of_range&) { threw = true; }
    EXPECT(threw);

    // "A" and "C" intact
    EXPECT(m.lookup(2)  == "A");
    EXPECT(m.lookup(15) == "C");
}

void test_erase_iterator_first() {
    SECTION("erase(iterator&): erase first node, return points at new begin");

    IntervalMap<int,std::string> m;
    m.insert(1,  5,  std::string("A"));
    m.insert(5,  10, std::string("B"));

    auto it   = m.begin();
    auto next = m.erase(it);

    EXPECT(m.size() == 1);
    EXPECT(next == m.begin());
    EXPECT(next->val == "B");
    EXPECT(it == m.begin());  // in-place update
}

void test_erase_iterator_last() {
    SECTION("erase(iterator&): erase last node, return is end()");

    IntervalMap<int,std::string> m;
    m.insert(1, 5, std::string("A"));
    m.insert(5, 10, std::string("B"));

    auto it = m.begin();
    ++it;  // last node "B"
    auto next = m.erase(it);

    EXPECT(m.size() == 1);
    EXPECT(next == m.end());
    EXPECT(it   == m.end());  // in-place update
    EXPECT(m.lookup(2) == "A");
}

void test_erase_only_node() {
    SECTION("erase(iterator&): only node leaves map empty");

    IntervalMap<int,std::string> m;
    m.insert(3, 7, std::string("X"));

    auto it   = m.begin();
    auto next = m.erase(it);

    EXPECT(m.empty());
    EXPECT(next == m.end());
    EXPECT(it   == m.end());
}

void test_erase_loop_idiom() {
    SECTION("erase(iterator&): idiomatic erase-in-loop removes selected nodes");

    IntervalMap<int,std::string> m;
    // Insert [0,5) "even-0", [5,10) "odd-5", [10,15) "even-10",
    //        [15,20) "odd-15", [20,25) "even-20"
    m.insert(0,  5,  std::string("even"));
    m.insert(5,  10, std::string("odd"));
    m.insert(10, 15, std::string("even"));
    m.insert(15, 20, std::string("odd"));
    m.insert(20, 25, std::string("even"));

    // Remove all "odd" nodes using the loop-safe pattern.
    for (auto it = m.begin(); it != m.end(); )
        it = (it->val == "odd") ? m.erase(it) : ++it;

    EXPECT(m.size() == 3);
    for (auto& n : m)
        EXPECT(n.val == "even");
}

void test_erase_const_iterator() {
    SECTION("erase(const_iterator): erases correctly, returns mutable next");

    IntervalMap<int,std::string> m;
    m.insert(1,  5,  std::string("A"));
    m.insert(5,  10, std::string("B"));
    m.insert(10, 20, std::string("C"));

    // Obtain a const_iterator via the const overload of begin().
    const auto& cm = m;
    auto cit = cm.begin();  // const_iterator
    ++cit;                  // points at "B"
    EXPECT(cit->val == "B");

    // erase via const_iterator — returns a mutable iterator
    auto next = m.erase(cit);

    EXPECT(m.size() == 2);
    EXPECT(next->val == "C");

    bool threw = false;
    try { m.lookup(7); } catch (const std::out_of_range&) { threw = true; }
    EXPECT(threw);
}

void test_erase_const_iterator_loop() {
    SECTION("erase(const_iterator): loop using const_iterator and mutable return");

    IntervalMap<int,std::string> m;
    m.insert(0,  5,  std::string("keep"));
    m.insert(5,  10, std::string("drop"));
    m.insert(10, 15, std::string("keep"));
    m.insert(15, 20, std::string("drop"));

    // Drive the loop with a mutable iterator but obtain nodes through const ref.
    auto it = m.begin();
    while (it != m.end()) {
        IntervalMap<int,std::string>::const_iterator cit = it;
        if (cit->val == "drop")
            it = m.erase(cit);   // erase(const_iterator) -> mutable iterator
        else
            ++it;
    }

    EXPECT(m.size() == 2);
    for (auto& n : m)
        EXPECT(n.val == "keep");
}

void test_erase_return_value_ignored() {
    SECTION("erase(iterator&): return value can be ignored; pos alone is sufficient");

    IntervalMap<int,std::string> m;
    m.insert(1,  5,  std::string("A"));
    m.insert(5,  10, std::string("B"));
    m.insert(10, 15, std::string("C"));

    auto it = m.begin();  // "A"
    m.erase(it);          // return value ignored; it updated in place to "B"
    EXPECT(it->val == "B");
    m.erase(it);          // it updated in place to "C"
    EXPECT(it->val == "C");
    m.erase(it);          // it updated in place to end()
    EXPECT(it == m.end());
    EXPECT(m.empty());
}

// ── erase(Start, Stop) tests ──────────────────────────────────────────────

void test_erase_range_noop_on_empty() {
    SECTION("erase(Start,Stop): no-op on empty map");

    IntervalMap<int,std::string> m;
    auto it = m.erase(5, 10);
    EXPECT(it == m.end());
    EXPECT(m.empty());
}

void test_erase_range_noop_invalid() {
    SECTION("erase(Start,Stop): no-op when Start >= Stop");

    IntervalMap<int,std::string> m;
    m.insert(1, 20, std::string("A"));

    m.erase(10, 10);   // empty range
    EXPECT(m.size() == 1);

    m.erase(10, 5);    // inverted range
    EXPECT(m.size() == 1);
    EXPECT(m.lookup(1) == "A");
}

void test_erase_range_no_overlap() {
    SECTION("erase(Start,Stop): range lies entirely in a gap — no change");

    IntervalMap<int,std::string> m;
    m.insert(1,  5,  std::string("A"));
    m.insert(10, 20, std::string("B"));

    // Erase entirely within the gap [5, 10)
    auto it = m.erase(6, 9);
    EXPECT(m.size() == 2);
    EXPECT(m.lookup(2)  == "A");
    EXPECT(m.lookup(15) == "B");
    // Return value: first node at or after Stop=9 is "B" (start=10)
    EXPECT(it != m.end());
    EXPECT(it->val == "B");
}

void test_erase_range_trim_left_straddle() {
    SECTION("erase(Start,Stop): left-straddling interval trimmed at Start");

    // [1,15) "A"
    // erase [10,20)
    // expected: [1,10) "A"
    IntervalMap<int,std::string> m;
    m.insert(1, 15, std::string("A"));

    auto it = m.erase(10, 20);
    EXPECT(m.size() == 1);
    EXPECT(m.lookup(1) == "A");
    EXPECT(m.lookup(9) == "A");
    bool threw = false;
    try { m.lookup(10); } catch (const std::out_of_range&) { threw = true; }
    EXPECT(threw);
    EXPECT(it == m.end());   // nothing at or after Stop=20
}

void test_erase_range_trim_right_straddle() {
    SECTION("erase(Start,Stop): right-straddling interval re-keyed at Stop");

    // [5,20) "B"
    // erase [1,10)
    // expected: [10,20) "B"
    IntervalMap<int,std::string> m;
    m.insert(5, 20, std::string("B"));

    auto it = m.erase(1, 10);
    EXPECT(m.size() == 1);
    bool threw = false;
    try { m.lookup(5); } catch (const std::out_of_range&) { threw = true; }
    EXPECT(threw);
    EXPECT(m.lookup(10) == "B");
    EXPECT(m.lookup(19) == "B");
    // Return value: first node at or after Stop=10 is the trimmed "B"
    EXPECT(it != m.end());
    EXPECT(it->start == 10);
    EXPECT(it->val   == "B");
}

void test_erase_range_split_spanning() {
    SECTION("erase(Start,Stop): erased range entirely inside one interval — splits it");

    // [1,20) "A"
    // erase [5,10)
    // expected: [1,5) "A", [10,20) "A"
    IntervalMap<int,std::string> m;
    m.insert(1, 20, std::string("A"));

    auto it = m.erase(5, 10);
    EXPECT(m.size() == 2);
    EXPECT(m.lookup(1) == "A");
    EXPECT(m.lookup(4) == "A");
    bool threw = false;
    try { m.lookup(5); } catch (const std::out_of_range&) { threw = true; }
    EXPECT(threw);
    threw = false;
    try { m.lookup(9); } catch (const std::out_of_range&) { threw = true; }
    EXPECT(threw);
    EXPECT(m.lookup(10) == "A");
    EXPECT(m.lookup(19) == "A");
    // Return value: first node at or after Stop=10
    EXPECT(it != m.end());
    EXPECT(it->start == 10);
}

void test_erase_range_removes_multiple_interior() {
    SECTION("erase(Start,Stop): multiple fully-contained intervals deleted");

    // [1,5) "A", [5,10) "B", [10,15) "C", [15,20) "D"
    // erase [5,15)
    // expected: [1,5) "A", [15,20) "D"
    IntervalMap<int,std::string> m;
    m.insert(1,  5,  std::string("A"));
    m.insert(5,  10, std::string("B"));
    m.insert(10, 15, std::string("C"));
    m.insert(15, 20, std::string("D"));

    auto it = m.erase(5, 15);
    EXPECT(m.size() == 2);
    EXPECT(m.lookup(2)  == "A");
    EXPECT(m.lookup(17) == "D");
    bool threw = false;
    try { m.lookup(7);  } catch (const std::out_of_range&) { threw = true; }
    EXPECT(threw);
    threw = false;
    try { m.lookup(12); } catch (const std::out_of_range&) { threw = true; }
    EXPECT(threw);
    // Return value: first node at or after Stop=15 is "D"
    EXPECT(it != m.end());
    EXPECT(it->val == "D");
}

void test_erase_range_trims_both_sides_and_removes_interior() {
    SECTION("erase(Start,Stop): trims both straddles and removes interior nodes");

    // [1,8) "A", [8,12) "B", [12,16) "C", [16,25) "D"
    // erase [5,20)
    // expected: [1,5) "A", [20,25) "D"
    IntervalMap<int,std::string> m;
    m.insert(1,  8,  std::string("A"));
    m.insert(8,  12, std::string("B"));
    m.insert(12, 16, std::string("C"));
    m.insert(16, 25, std::string("D"));

    auto it = m.erase(5, 20);
    EXPECT(m.size() == 2);
    EXPECT(m.lookup(1)  == "A");
    EXPECT(m.lookup(4)  == "A");
    EXPECT(m.lookup(20) == "D");
    EXPECT(m.lookup(24) == "D");
    bool threw = false;
    try { m.lookup(5);  } catch (const std::out_of_range&) { threw = true; }
    EXPECT(threw);
    threw = false;
    try { m.lookup(19); } catch (const std::out_of_range&) { threw = true; }
    EXPECT(threw);
    EXPECT(it != m.end());
    EXPECT(it->start == 20);
}

void test_erase_range_exact_boundary_match() {
    SECTION("erase(Start,Stop): erased range exactly matches a stored interval");

    IntervalMap<int,std::string> m;
    m.insert(5, 10, std::string("X"));
    m.insert(10, 20, std::string("Y"));

    auto it = m.erase(5, 10);
    EXPECT(m.size() == 1);
    bool threw = false;
    try { m.lookup(5); } catch (const std::out_of_range&) { threw = true; }
    EXPECT(threw);
    EXPECT(m.lookup(10) == "Y");
    EXPECT(it != m.end());
    EXPECT(it->val == "Y");
}

void test_erase_range_entire_map() {
    SECTION("erase(Start,Stop): erase range covering the entire map leaves it empty");

    IntervalMap<int,std::string> m;
    m.insert(1,  5,  std::string("A"));
    m.insert(5,  10, std::string("B"));
    m.insert(10, 20, std::string("C"));

    auto it = m.erase(0, 100);
    EXPECT(m.empty());
    EXPECT(it == m.end());
}

void test_erase_range_touching_boundary_not_overlapping() {
    SECTION("erase(Start,Stop): erase range touching but not overlapping a stored interval");

    // [10, 20) "A"; erase [5, 10) — Stop == stored Start, no overlap.
    IntervalMap<int,std::string> m;
    m.insert(10, 20, std::string("A"));

    auto it = m.erase(5, 10);
    EXPECT(m.size() == 1);       // "A" untouched
    EXPECT(m.lookup(10) == "A");
    EXPECT(it != m.end());
    EXPECT(it->start == 10);     // first node at or after Stop=10
}

void test_erase_range_return_iterator_is_next_usable() {
    SECTION("erase(Start,Stop): return iterator usable for continued iteration");

    // Build [0,5), [5,10), [10,15), [15,20), [20,25)
    for (int i = 0; i < 5; ++i) {
        IntervalMap<int,std::string> m;
        for (int j = 0; j < 5; ++j)
            m.insert(j*5, j*5+5, std::string(1, 'A'+j));

        // Erase the i-th interval exactly and verify return continues correctly.
        auto ret = m.erase(i*5, i*5+5);
        EXPECT(m.size() == 4);
        if (i < 4) {
            EXPECT(ret != m.end());
            EXPECT(ret->start == (i+1)*5);
        } else {
            EXPECT(ret == m.end());
        }
    }
}

// ── clear() tests ─────────────────────────────────────────────────────────

void test_clear_empty_map() {
    SECTION("clear(): clearing an already-empty map is a no-op");

    IntervalMap<int,std::string> m;
    m.clear();
    EXPECT(m.empty());
    EXPECT(m.size() == 0);
    EXPECT(m.begin() == m.end());
}

void test_clear_removes_all() {
    SECTION("clear(): all nodes are removed");

    IntervalMap<int,std::string> m;
    m.insert(1,  5,  std::string("A"));
    m.insert(5,  10, std::string("B"));
    m.insert(10, 20, std::string("C"));

    m.clear();
    EXPECT(m.empty());
    EXPECT(m.size() == 0);
    EXPECT(m.begin() == m.end());

    bool threw = false;
    try { m.lookup(5); } catch (const std::out_of_range&) { threw = true; }
    EXPECT(threw);
}

void test_clear_then_reuse() {
    SECTION("clear(): map is fully usable after clear");

    IntervalMap<int,std::string> m;
    m.insert(1, 10, std::string("old"));
    m.clear();

    // Re-insert completely different data.
    m.insert(50, 60, std::string("new"));
    EXPECT(m.size() == 1);
    EXPECT(m.lookup(55) == "new");

    bool threw = false;
    try { m.lookup(5); } catch (const std::out_of_range&) { threw = true; }
    EXPECT(threw);
}

void test_clear_repeated() {
    SECTION("clear(): repeated clear calls are safe");

    IntervalMap<int,std::string> m;
    m.insert(1, 5, std::string("A"));
    m.clear();
    m.clear();   // second clear on already-empty map
    m.clear();
    EXPECT(m.empty());

    m.insert(1, 5, std::string("B"));
    EXPECT(m.lookup(3) == "B");
}

void test_clear_preserves_comparator() {
    SECTION("clear(): custom comparator is preserved after clear");

    IntervalMap<int,std::string,std::greater<int>> m;
    m.insert(20, 10, std::string("A"));   // valid under std::greater
    m.clear();
    EXPECT(m.empty());

    // Re-insert — must still accept the same comparator-valid range.
    auto it = m.insert(30, 15, std::string("B"));
    EXPECT(it != m.end());
    EXPECT(m.lookup(20) == "B");
}

// ── find_all tests ────────────────────────────────────────────────────────

// Helper: snapshot the map's full state as a vector of (start,stop,val) tuples.
using Snap = std::vector<std::tuple<int,int,std::string>>;
Snap snap(const IntervalMap<int,std::string>& m) {
    Snap v;
    for (const auto& n : m) v.emplace_back(n.start, n.stop, n.val);
    return v;
}

// ── iterator_collection (mutable) ─────────────────────────────────────────

void test_find_all_empty_map() {
    SECTION("find_all (mut): empty map returns empty collection");

    IntervalMap<int,std::string> m;
    auto col = m.find_all(1, 10);
    EXPECT(col.empty());
    EXPECT(col.size() == 0);
}

void test_find_all_invalid_range() {
    SECTION("find_all (mut): Start >= Stop returns empty collection");

    IntervalMap<int,std::string> m;
    m.insert(1, 20, std::string("A"));

    auto col1 = m.find_all(10, 10);
    EXPECT(col1.empty());
    auto col2 = m.find_all(10, 5);
    EXPECT(col2.empty());
}

void test_find_all_no_overlap() {
    SECTION("find_all (mut): query range in a gap returns empty collection");

    IntervalMap<int,std::string> m;
    m.insert(1,  5,  std::string("A"));
    m.insert(10, 20, std::string("B"));

    {
        auto col = m.find_all(5, 10);   // gap
        EXPECT(col.empty());
    }
    // Map must be unchanged after the empty collection is destroyed.
    EXPECT(snap(m) == Snap({{1,5,"A"},{10,20,"B"}}));
}

void test_find_all_fully_contained_nodes() {
    SECTION("find_all (mut): query covers some whole nodes, no splits needed");

    IntervalMap<int,std::string> m;
    m.insert(1,  5,  std::string("A"));
    m.insert(5,  10, std::string("B"));
    m.insert(10, 20, std::string("C"));

    std::vector<std::pair<int,int>> ranges;
    {
        auto col = m.find_all(5, 10);
        for (auto it : col)
            ranges.emplace_back(it->start, it->stop);
        EXPECT(col.size() == 1);
    }
    // "B" was not split so no coalescing needed; map unchanged.
    EXPECT(snap(m) == Snap({{1,5,"A"},{5,10,"B"},{10,20,"C"}}));
    EXPECT(ranges == (std::vector<std::pair<int,int>>{{5,10}}));
}

void test_find_all_splits_left_boundary() {
    SECTION("find_all (mut): left-straddling node is split at Start");

    // [1,15) "A"  →  query [5,20)
    // Expected collection: [5,15) "A"
    // Left remnant in map: [1,5) "A"
    IntervalMap<int,std::string> m;
    m.insert(1, 15, std::string("A"));

    std::vector<std::pair<int,int>> col_ranges;
    {
        auto col = m.find_all(5, 20);
        EXPECT(col.size() == 1);
        for (auto it : col)
            col_ranges.emplace_back(it->start, it->stop);
    }
    EXPECT(col_ranges == (std::vector<std::pair<int,int>>{{5,15}}));

    // After destruction: coalescer sees [1,5)+"A" adjacent to [5,15)+"A"
    // → merges back to [1,15) "A".
    EXPECT(snap(m) == Snap({{1,15,"A"}}));
}

void test_find_all_splits_right_boundary() {
    SECTION("find_all (mut): right-straddling node is split at Stop");

    // [5,20) "B"  →  query [1,12)
    // Expected collection: [5,12) "B"
    // Right remnant in map: [12,20) "B"
    IntervalMap<int,std::string> m;
    m.insert(5, 20, std::string("B"));

    std::vector<std::pair<int,int>> col_ranges;
    {
        auto col = m.find_all(1, 12);
        EXPECT(col.size() == 1);
        for (auto it : col) col_ranges.emplace_back(it->start, it->stop);
    }
    EXPECT(col_ranges == (std::vector<std::pair<int,int>>{{5,12}}));

    // After destruction: coalescer sees [5,12)+"B" and right neighbour [12,20)+"B"
    // → merges back to [5,20) "B".
    EXPECT(snap(m) == Snap({{5,20,"B"}}));
}

void test_find_all_splits_both_boundaries() {
    SECTION("find_all (mut): query range splits both left and right straddling nodes");

    // [1,10) "A", [10,20) "B", [20,30) "C"
    // query [5,25)
    // Expected collection: [5,10) "A", [10,20) "B", [20,25) "C"
    // Left  remnant: [1,5)  "A"
    // Right remnant: [25,30) "C"
    IntervalMap<int,std::string> m;
    m.insert(1,  10, std::string("A"));
    m.insert(10, 20, std::string("B"));
    m.insert(20, 30, std::string("C"));

    std::vector<std::tuple<int,int,std::string>> col_snap;
    {
        auto col = m.find_all(5, 25);
        EXPECT(col.size() == 3);
        for (auto it : col)
            col_snap.emplace_back(it->start, it->stop, it->val);
    }
    EXPECT(col_snap == (decltype(col_snap){{5,10,"A"},{10,20,"B"},{20,25,"C"}}));

    // After destruction: no adjacent equal-valued nodes survive → map has 5 pieces,
    // then coalescer: [1,5)+"A" with [5,10)+"A" merge, [20,25)+"C" with [25,30)+"C" merge.
    EXPECT(snap(m) == Snap({{1,10,"A"},{10,20,"B"},{20,30,"C"}}));
}

void test_find_all_query_inside_single_node() {
    SECTION("find_all (mut): query entirely inside one node splits it into three");

    // [1,20) "A"  →  query [5,10)
    // During construction: [1,5) "A" (left remnant), [5,10) "A" (collected), [10,20) "A" (right remnant)
    // After destruction: all three coalesce back to [1,20) "A"
    IntervalMap<int,std::string> m;
    m.insert(1, 20, std::string("A"));

    {
        auto col = m.find_all(5, 10);
        EXPECT(col.size() == 1);
        for (auto it : col) {
            EXPECT(it->start == 5);
            EXPECT(it->stop  == 10);
        }
        // Map currently has three nodes.
        EXPECT(m.size() == 3);
    }
    // After collection destructs: all three equal-valued adjacent nodes merged.
    EXPECT(snap(m) == Snap({{1,20,"A"}}));
}

void test_find_all_range_for() {
    SECTION("find_all (mut): range-for correctly yields all collected nodes");

    IntervalMap<int,std::string> m;
    m.insert(0,  5,  std::string("X"));
    m.insert(5,  10, std::string("Y"));
    m.insert(10, 15, std::string("Z"));

    std::vector<std::string> vals;
    {
        auto col = m.find_all(0, 15);
        for (auto it : col)
            vals.push_back(it->val);
    }
    EXPECT(vals == (std::vector<std::string>{"X","Y","Z"}));
    // All different values — no coalescing occurs.
    EXPECT(snap(m) == Snap({{0,5,"X"},{5,10,"Y"},{10,15,"Z"}}));
}

void test_find_all_coalesce_intra_all_same_value() {
    SECTION("find_all (mut): all collected nodes have equal values → fully coalesced after destruction");

    // Build [0,5) "A", [5,10) "A", [10,15) "A"  (deliberately fragmented)
    // Then query the full range — all three are collected.
    // On destruction all three merge into one.
    IntervalMap<int,std::string> m;
    // Use raw insert_impl pathway: insert, split, re-insert to create fragments.
    // Simplest: insert one big range, then erase+reinsert to fragment it.
    m.insert(0,  5,  std::string("A"));
    m.insert(5,  10, std::string("A"));
    m.insert(10, 15, std::string("A"));
    EXPECT(m.size() == 3);  // confirm they were stored separately

    {
        auto col = m.find_all(0, 15);
        EXPECT(col.size() == 3);
    }
    // Destructor should have merged all three.
    EXPECT(snap(m) == Snap({{0,15,"A"}}));
}

void test_find_all_coalesce_partial_same_value() {
    SECTION("find_all (mut): only some adjacent nodes coalesce");

    // [0,5) "A", [5,10) "A", [10,15) "B"  — query [0,15)
    // Collection: all three.  Destructor merges [0,5)+"A" with [5,10)+"A"
    // but leaves [10,15)+"B" separate.
    IntervalMap<int,std::string> m;
    m.insert(0,  5,  std::string("A"));
    m.insert(5,  10, std::string("A"));
    m.insert(10, 15, std::string("B"));

    {
        auto col = m.find_all(0, 15);
        EXPECT(col.size() == 3);
    }
    EXPECT(snap(m) == Snap({{0,10,"A"},{10,15,"B"}}));
}

void test_find_all_coalesce_with_left_neighbour() {
    SECTION("find_all (mut): coalescing absorbs the left-split remnant");

    // [1,20) "A"  →  query [10,15)
    // Splits: [1,10) "A" (remnant, not in col), [10,15) "A" (col), [15,20) "A" (remnant)
    // Destructor: col has only [10,15)"A"; checks left neighbour [1,10)"A" → merges to [1,15)"A";
    //             then checks right neighbour [15,20)"A" → merges further to [1,20)"A".
    IntervalMap<int,std::string> m;
    m.insert(1, 20, std::string("A"));

    {
        auto col = m.find_all(10, 15);
        EXPECT(col.size() == 1);
        EXPECT(m.size() == 3);  // split into three during construction
    }
    EXPECT(snap(m) == Snap({{1,20,"A"}}));
}

void test_find_all_coalesce_does_not_merge_different_values() {
    SECTION("find_all (mut): coalescing does not merge nodes with different values");

    // [0,5) "A", [5,10) "B"  →  query [0,10)
    // Collection: both.  Destructor: different values → no merge.
    IntervalMap<int,std::string> m;
    m.insert(0,  5,  std::string("A"));
    m.insert(5,  10, std::string("B"));

    {
        auto col = m.find_all(0, 10);
        EXPECT(col.size() == 2);
    }
    EXPECT(snap(m) == Snap({{0,5,"A"},{5,10,"B"}}));
}

void test_find_all_move_collection() {
    SECTION("find_all (mut): move-constructed collection coalesces once on destruction");

    IntervalMap<int,std::string> m;
    m.insert(0,  5,  std::string("A"));
    m.insert(5,  10, std::string("A"));

    {
        auto col  = m.find_all(0, 10);
        auto col2 = std::move(col);   // col is now dead (map_ == nullptr)
        EXPECT(col2.size() == 2);
        // col2 destructs → coalesces.
    }
    EXPECT(snap(m) == Snap({{0,10,"A"}}));
}

// ── const_iterator_collection (read-only) ────────────────────────────────

void test_find_all_const_empty() {
    SECTION("find_all (const): empty map");

    const IntervalMap<int,std::string> m;
    auto col = m.find_all(1, 10);
    EXPECT(col.empty());
}

void test_find_all_const_no_overlap() {
    SECTION("find_all (const): query in a gap");

    IntervalMap<int,std::string> m;
    m.insert(1,  5,  std::string("A"));
    m.insert(10, 20, std::string("B"));
    const auto& cm = m;

    auto col = cm.find_all(6, 9);
    EXPECT(col.empty());
}

void test_find_all_const_includes_straddlers() {
    SECTION("find_all (const): nodes that straddle boundaries are included unclipped");

    // [1,15) "A"  →  query [5,20) const
    // No split occurs: col contains [1,15) "A" as-is.
    IntervalMap<int,std::string> m;
    m.insert(1, 15, std::string("A"));
    const auto& cm = m;

    auto col = cm.find_all(5, 20);
    EXPECT(col.size() == 1);
    for (auto cit : col) {
        EXPECT(cit->start == 1);   // not clipped to 5
        EXPECT(cit->stop  == 15);
        EXPECT(cit->val   == "A");
    }
    // Map must be completely unchanged.
    EXPECT(snap(m) == Snap({{1,15,"A"}}));
}

void test_find_all_const_range_for() {
    SECTION("find_all (const): range-for yields const_iterators");

    IntervalMap<int,std::string> m;
    m.insert(0,  5,  std::string("X"));
    m.insert(5,  10, std::string("Y"));
    m.insert(10, 15, std::string("Z"));
    const auto& cm = m;

    std::vector<std::string> vals;
    for (auto cit : cm.find_all(0, 15))
        vals.push_back(cit->val);

    EXPECT(vals == (std::vector<std::string>{"X","Y","Z"}));
    EXPECT(snap(m) == Snap({{0,5,"X"},{5,10,"Y"},{10,15,"Z"}}));
}

void test_find_all_const_map_unchanged() {
    SECTION("find_all (const): map state is identical before and after");

    IntervalMap<int,std::string> m;
    m.insert(1,  10, std::string("A"));
    m.insert(10, 20, std::string("B"));
    m.insert(20, 30, std::string("C"));

    Snap before = snap(m);
    {
        const auto& cm = m;
        auto col = cm.find_all(5, 25);
        EXPECT(col.size() == 3);
    }
    EXPECT(snap(m) == before);
}

void test_find_all_query_exact_node_boundaries() {
    SECTION("find_all (mut): query exactly matches stored node boundaries — no splits");

    IntervalMap<int,std::string> m;
    m.insert(5, 10, std::string("A"));
    m.insert(10, 15, std::string("B"));

    {
        auto col = m.find_all(5, 15);
        EXPECT(col.size() == 2);
    }
    EXPECT(snap(m) == Snap({{5,10,"A"},{10,15,"B"}}));
}

void test_find_all_single_point_query() {
    SECTION("find_all (mut): single-width query [k, k+1) collects at most one node");

    IntervalMap<int,std::string> m;
    m.insert(1, 10, std::string("A"));

    {
        auto col = m.find_all(5, 6);
        EXPECT(col.size() == 1);
        for (auto it : col) {
            EXPECT(it->start == 5);
            EXPECT(it->stop  == 6);
        }
        EXPECT(m.size() == 3);   // [1,5) "A", [5,6) "A", [6,10) "A"
    }
    // All three same-valued pieces coalesce.
    EXPECT(snap(m) == Snap({{1,10,"A"}}));
}

void test_find_all_mut_yields_modifiable_nodes() {
    SECTION("find_all (mut): iterators from collection allow node mutation");

    IntervalMap<int,std::string> m;
    m.insert(0, 5,  std::string("old"));
    m.insert(5, 10, std::string("keep"));

    {
        auto col = m.find_all(0, 5);
        for (auto it : col)
            it->val = "new";   // mutate through the iterator
    }
    // "old" becomes "new"; "new" != "keep" so no coalescing.
    EXPECT(snap(m) == Snap({{0,5,"new"},{5,10,"keep"}}));
}


// ── Additional find_all tests ─────────────────────────────────────────────

void test_find_all_coalesce_after_mutation_no_merge() {
    SECTION("find_all (mut): mutating val prevents coalescing with neighbour");

    // [1,10) "A", [10,20) "A"  →  query [5,15)
    // Splits: [1,5) "A" (remnant), [5,10) "A" (col), [10,15) "A" (col), [15,20) "A" (remnant)
    // Mutate [5,10) → "B" while collection is live.
    // Destructor: [5,10)"B" != [10,15)"A" → no intra merge.
    //             left  neighbour [1,5)"A"  != [5,10)"B"  → no left merge.
    //             right neighbour [15,20)"A" adjacent to [10,15)"A" → merges → [10,20)"A"
    IntervalMap<int,std::string> m;
    m.insert(1,  10, std::string("A"));
    m.insert(10, 20, std::string("A"));

    {
        auto col = m.find_all(5, 15);
        EXPECT(col.size() == 2);
        bool first = true;
        for (auto it : col) {
            if (first) { it->val = "B"; first = false; }
        }
    }
    // After destruction:
    //   [1,5)"A", [5,10)"B", [10,15)"A" merged with [15,20)"A" → [10,20)"A"
    EXPECT(snap(m) == Snap({{1,5,"A"},{5,10,"B"},{10,20,"A"}}));
}

void test_find_all_coalesce_three_way_chain() {
    SECTION("find_all (mut): intra merge creates a new adjacency with map neighbour");

    // Layout: [0,5)"A", [5,10)"A", [10,15)"A", [15,20)"A"
    // Query [5,15): collects [5,10) and [10,15).
    // Left  remnant: none (0,5 is a separate node, but has same val).
    // Right remnant: none (15,20 is a separate node, but has same val).
    // Intra-merge: [5,10)+"A" absorbs [10,15)+"A" → [5,15)"A"
    // Left  check: left neighbour [0,5)"A" adjacent → merges to [0,15)"A"
    // Right check: right neighbour [15,20)"A" adjacent → merges to [0,20)"A"
    IntervalMap<int,std::string> m;
    m.insert(0,  5,  std::string("A"));
    m.insert(5,  10, std::string("A"));
    m.insert(10, 15, std::string("A"));
    m.insert(15, 20, std::string("A"));
    EXPECT(m.size() == 4);

    {
        auto col = m.find_all(5, 15);
        EXPECT(col.size() == 2);
    }
    EXPECT(snap(m) == Snap({{0,20,"A"}}));
}

void test_find_all_query_starts_at_node_start_no_left_split() {
    SECTION("find_all (mut): query starts exactly at a node start — no left split");

    // [5,20) "A"  →  query [5,10)
    // lower_bound(5) hits the node directly; predecessor check is skipped.
    // Only a right split at 10 should occur.
    IntervalMap<int,std::string> m;
    m.insert(5, 20, std::string("A"));

    {
        auto col = m.find_all(5, 10);
        EXPECT(col.size() == 1);
        for (auto it : col) {
            EXPECT(it->start == 5);
            EXPECT(it->stop  == 10);
        }
        EXPECT(m.size() == 2);  // [5,10)"A" collected, [10,20)"A" remnant
    }
    EXPECT(snap(m) == Snap({{5,20,"A"}}));
}

void test_find_all_query_ends_at_node_stop_no_right_split() {
    SECTION("find_all (mut): query ends exactly at a node stop — no right split");

    // [1,10) "A"  →  query [5,10)
    // The node's stop == Stop, so no right split.
    IntervalMap<int,std::string> m;
    m.insert(1, 10, std::string("A"));

    {
        auto col = m.find_all(5, 10);
        EXPECT(col.size() == 1);
        for (auto it : col) {
            EXPECT(it->start == 5);
            EXPECT(it->stop  == 10);
        }
        // [1,5)"A" left remnant + [5,10)"A" collected = 2 nodes
        EXPECT(m.size() == 2);
    }
    // Coalesce: [1,5)+"A" and [5,10)+"A" → [1,10)"A"
    EXPECT(snap(m) == Snap({{1,10,"A"}}));
}

void test_find_all_move_assign_collection() {
    SECTION("find_all (mut): move-assignment triggers coalesce on overwritten collection");

    IntervalMap<int,std::string> m;
    m.insert(0,  5,  std::string("A"));
    m.insert(5,  10, std::string("A"));
    m.insert(20, 25, std::string("B"));
    m.insert(25, 30, std::string("B"));

    {
        auto col1 = m.find_all(0, 10);   // collects [0,5)"A" and [5,10)"A"
        auto col2 = m.find_all(20, 30);  // collects [20,25)"B" and [25,30)"B"
        EXPECT(col1.size() == 2);
        EXPECT(col2.size() == 2);

        // Move-assign col2 into col1:
        //  • col1's destructor logic runs → coalesces [0,5)+"A" + [5,10)+"A" → [0,10)"A"
        //  • col1 now owns col2's contents
        col1 = std::move(col2);

        // col1 now has col2's iterators; col2 is dead (map_==nullptr).
        EXPECT(col1.size() == 2);
        // The [0,10)"A" coalesce already happened.
        EXPECT(snap(m) == Snap({{0,10,"A"},{20,25,"B"},{25,30,"B"}}));
        // col1 destructs → coalesces [20,25)+"B" + [25,30)+"B" → [20,30)"B"
    }
    EXPECT(snap(m) == Snap({{0,10,"A"},{20,30,"B"}}));
}

void test_find_all_empty_collection_destructs_safely() {
    SECTION("find_all (mut): empty collection destructs without touching the map");

    IntervalMap<int,std::string> m;
    m.insert(1, 5, std::string("A"));

    {
        auto col = m.find_all(10, 20);  // no overlap → empty
        EXPECT(col.empty());
        // destructor should not crash or modify m
    }
    EXPECT(snap(m) == Snap({{1,5,"A"}}));
}

void test_find_all_const_collection_is_copyable() {
    SECTION("find_all (const): const_iterator_collection is copyable");

    IntervalMap<int,std::string> m;
    m.insert(1,  5,  std::string("A"));
    m.insert(5,  10, std::string("B"));
    const auto& cm = m;

    auto col1 = cm.find_all(1, 10);
    auto col2  = col1;   // copy

    EXPECT(col1.size() == 2);
    EXPECT(col2.size() == 2);

    std::vector<std::string> v1, v2;
    for (auto cit : col1) v1.push_back(cit->val);
    for (auto cit : col2) v2.push_back(cit->val);
    EXPECT(v1 == v2);
    EXPECT(snap(m) == Snap({{1,5,"A"},{5,10,"B"}}));
}

void test_find_all_const_right_straddler_included() {
    SECTION("find_all (const): right-straddling node included whole (not clipped)");

    // [5,20) "B"  →  query [1,12) const
    // No split → col contains [5,20) "B" with stop=20, not 12.
    IntervalMap<int,std::string> m;
    m.insert(5, 20, std::string("B"));
    const auto& cm = m;

    auto col = cm.find_all(1, 12);
    EXPECT(col.size() == 1);
    for (auto cit : col) {
        EXPECT(cit->start == 5);
        EXPECT(cit->stop  == 20);  // NOT clipped to 12
    }
    EXPECT(snap(m) == Snap({{5,20,"B"}}));
}

void test_find_all_mut_adjacent_equal_value_neighbours_not_in_collection() {
    SECTION("find_all (mut): left and right equal-value neighbours coalesce with boundary nodes");

    // [0,5)"A", [5,10)"A", [10,15)"B", [15,20)"A", [20,25)"A"
    // Query [5,15): collects [5,10)"A" and [10,15)"B"
    // Left  remnant: none (0,5 separate node, same val as first collected)
    // Right remnant: none (15,20 separate node, different val from last collected "B")
    // Intra: [5,10)"A" != [10,15)"B" → no intra merge
    // Left  check: [0,5)"A" + [5,10)"A" adjacent → merge → [0,10)"A"
    // Right check: [15,20)"A" != [10,15)"B" → no right merge
    IntervalMap<int,std::string> m;
    m.insert(0,  5,  std::string("A"));
    m.insert(5,  10, std::string("A"));
    m.insert(10, 15, std::string("B"));
    m.insert(15, 20, std::string("A"));
    m.insert(20, 25, std::string("A"));

    {
        auto col = m.find_all(5, 15);
        EXPECT(col.size() == 2);
    }
    // Coalescer only merges direct neighbours of collection boundary nodes:
    // [0,5)+"A" adjacent to leftmost collected [5,10)+"A" → [0,10)"A" (left merge)
    // [15,20)"A" != last collected [10,15)"B" → no right merge
    // [15,20)"A" and [20,25)"A" are TWO hops away → not touched by this coalesce pass.
    EXPECT(snap(m) == Snap({{0,10,"A"},{10,15,"B"},{15,20,"A"},{20,25,"A"}}));
}

void test_find_all_multiple_sequential_find_alls() {
    SECTION("find_all (mut): two sequential find_all calls each coalesce correctly");

    IntervalMap<int,std::string> m;
    m.insert(0,  10, std::string("A"));
    m.insert(10, 20, std::string("B"));

    // First call: split [0,10) at 5 → [0,5) remnant + [5,10) collected.
    // Destructor: [0,5)+"A" merges with [5,10)+"A" → back to [0,10)"A".
    {
        auto col = m.find_all(5, 10);
        EXPECT(col.size() == 1);
        EXPECT(m.size() == 3);   // [0,5)"A" remnant + [5,10)"A" collected + [10,20)"B" unchanged
        EXPECT((*col.begin())->start == 5);
    }
    EXPECT(snap(m) == Snap({{0,10,"A"},{10,20,"B"}}));

    // Second call: split [10,20) at 15 → [10,15) collected + [15,20) remnant.
    // Destructor: [10,15)+"B" merges with [15,20)+"B" → back to [10,20)"B".
    {
        auto col = m.find_all(10, 15);
        EXPECT(col.size() == 1);
    }
    EXPECT(snap(m) == Snap({{0,10,"A"},{10,20,"B"}}));
}

// ── main ──────────────────────────────────────────────────────────────────

int main() {
    test_basic_insert_and_lookup();
    test_non_overlapping_inserts();
    test_overlap_new_overwrites_left_partial();
    test_overlap_new_overwrites_right_partial();
    test_overlap_new_inside_existing();
    test_overlap_new_covers_multiple();
    test_overlap_exact_replace();
    test_adjacent_inserts();
    test_invalid_range_rejected();
    test_find();
    test_operator_bracket();
    test_single_key_insert();
    test_move_insert();
    test_iterators();
    test_const_iterator_conversion();
    test_copy_and_move();
    test_custom_comparator();
    test_string_key();
    test_dense_coverage_then_clear();
    test_node_members_public();

    test_erase_iterator();
    test_erase_iterator_first();
    test_erase_iterator_last();
    test_erase_only_node();
    test_erase_loop_idiom();
    test_erase_const_iterator();
    test_erase_const_iterator_loop();
    test_erase_return_value_ignored();

    test_erase_range_noop_on_empty();
    test_erase_range_noop_invalid();
    test_erase_range_no_overlap();
    test_erase_range_trim_left_straddle();
    test_erase_range_trim_right_straddle();
    test_erase_range_split_spanning();
    test_erase_range_removes_multiple_interior();
    test_erase_range_trims_both_sides_and_removes_interior();
    test_erase_range_exact_boundary_match();
    test_erase_range_entire_map();
    test_erase_range_touching_boundary_not_overlapping();
    test_erase_range_return_iterator_is_next_usable();
    test_clear_empty_map();
    test_clear_removes_all();
    test_clear_then_reuse();
    test_clear_repeated();
    test_clear_preserves_comparator();

    test_find_all_empty_map();
    test_find_all_invalid_range();
    test_find_all_no_overlap();
    test_find_all_fully_contained_nodes();
    test_find_all_splits_left_boundary();
    test_find_all_splits_right_boundary();
    test_find_all_splits_both_boundaries();
    test_find_all_query_inside_single_node();
    test_find_all_range_for();
    test_find_all_coalesce_intra_all_same_value();
    test_find_all_coalesce_partial_same_value();
    test_find_all_coalesce_with_left_neighbour();
    test_find_all_coalesce_does_not_merge_different_values();
    test_find_all_move_collection();
    test_find_all_const_empty();
    test_find_all_const_no_overlap();
    test_find_all_const_includes_straddlers();
    test_find_all_const_range_for();
    test_find_all_const_map_unchanged();
    test_find_all_query_exact_node_boundaries();
    test_find_all_single_point_query();
    test_find_all_mut_yields_modifiable_nodes();

    test_find_all_coalesce_after_mutation_no_merge();
    test_find_all_coalesce_three_way_chain();
    test_find_all_query_starts_at_node_start_no_left_split();
    test_find_all_query_ends_at_node_stop_no_right_split();
    test_find_all_move_assign_collection();
    test_find_all_empty_collection_destructs_safely();
    test_find_all_const_collection_is_copyable();
    test_find_all_const_right_straddler_included();
    test_find_all_mut_adjacent_equal_value_neighbours_not_in_collection();
    test_find_all_multiple_sequential_find_alls();

    std::cout << "\n========================================\n"
              << "  Results: " << g_pass << " passed, " << g_fail << " failed\n"
              << "========================================\n";
    return g_fail ? 1 : 0;
}