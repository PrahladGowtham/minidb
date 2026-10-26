#include "storage/disk/disk_manager.hpp"
#include "storage/buffer/buffer_pool_manager.hpp"
#include "index/b_plus_tree.hpp"
#include <cassert>
#include <iostream>
#include <vector>
#include <algorithm>
#include <random>

struct IntComparator {
    int operator()(int64_t a, int64_t b) const {
        if (a < b) return -1;
        if (a > b) return 1;
        return 0;
    }
};

void TestBPlusTreeBasic() {
    std::cout << "[Test] BPlusTree Basic Operations (Point Lookups & Small Splits)..." << std::endl;
    const std::string db_name = "test_bpt_basic.db";
    std::remove(db_name.c_str());

    auto disk_manager = std::make_unique<minidb::DiskManager>(db_name);
    minidb::BufferPoolManager bpm(10, disk_manager.get());
    IntComparator cmp;

    // Small leaf/internal max size to force early splits
    minidb::BPlusTree<int64_t, minidb::RID, IntComparator> tree("test_index", &bpm, cmp, 4, 4);

    assert(tree.IsEmpty());

    // Insert 10 keys
    for (int64_t i = 1; i <= 10; ++i) {
        minidb::RID rid(static_cast<minidb::page_id_t>(i), static_cast<minidb::slot_id_t>(i * 2));
        assert(tree.Insert(i, rid));
    }

    assert(!tree.IsEmpty());

    // Verify all keys can be retrieved
    for (int64_t i = 1; i <= 10; ++i) {
        std::vector<minidb::RID> result;
        assert(tree.GetValue(i, &result));
        assert(result.size() == 1);
        assert(result[0].GetPageId() == i);
        assert(result[0].GetSlotId() == i * 2);
    }

    // Verify non-existent key
    std::vector<minidb::RID> missing_res;
    assert(!tree.GetValue(999, &missing_res));

    std::remove(db_name.c_str());
    std::cout << "  Passed!" << std::endl;
}

void TestBPlusTreeScaleAndRangeScan() {
    std::cout << "[Test] BPlusTree Deep Multi-Level Splits & Range Scan Iterators..." << std::endl;
    const std::string db_name = "test_bpt_scale.db";
    std::remove(db_name.c_str());

    auto disk_manager = std::make_unique<minidb::DiskManager>(db_name);
    minidb::BufferPoolManager bpm(50, disk_manager.get());
    IntComparator cmp;

    // Small degree = high tree depth
    minidb::BPlusTree<int64_t, minidb::RID, IntComparator> tree("scale_index", &bpm, cmp, 5, 5);

    const int NUM_KEYS = 1000;
    std::vector<int64_t> keys;
    for (int64_t i = 1; i <= NUM_KEYS; ++i) {
        keys.push_back(i);
    }

    // Shuffle keys to test arbitrary insertion orders
    std::mt19937 g(42);
    std::shuffle(keys.begin(), keys.end(), g);

    for (int64_t key : keys) {
        minidb::RID rid(static_cast<minidb::page_id_t>(key), static_cast<minidb::slot_id_t>(key % 100));
        assert(tree.Insert(key, rid));
    }

    // Verify all keys can be queried point-wise
    for (int64_t key : keys) {
        std::vector<minidb::RID> result;
        assert(tree.GetValue(key, &result));
        assert(result.size() == 1);
        assert(result[0].GetPageId() == key);
    }

    // Verify sequential range scan over full index
    int64_t expected_key = 1;
    int count = 0;
    for (auto it = tree.Begin(); it != tree.End(); ++it) {
        const auto& [k, v] = *it;
        assert(k == expected_key);
        assert(v.GetPageId() == expected_key);
        expected_key++;
        count++;
    }
    assert(count == NUM_KEYS);

    // Verify partial range scan starting from key 500
    int64_t expected_partial = 500;
    int partial_count = 0;
    for (auto it = tree.Begin(500); it != tree.End(); ++it) {
        const auto& [k, v] = *it;
        assert(k == expected_partial);
        assert(v.GetPageId() == expected_partial);
        expected_partial++;
        partial_count++;
    }
    assert(partial_count == (NUM_KEYS - 500 + 1));

    std::remove(db_name.c_str());
    std::cout << "  Passed!" << std::endl;
}

void TestBPlusTreeDeletions() {
    std::cout << "[Test] BPlusTree Deletions and Merging..." << std::endl;
    const std::string db_name = "test_bpt_deletions.db";
    std::remove(db_name.c_str());

    auto disk_manager = std::make_unique<minidb::DiskManager>(db_name);
    minidb::BufferPoolManager bpm(20, disk_manager.get());
    IntComparator cmp;

    minidb::BPlusTree<int64_t, minidb::RID, IntComparator> tree("del_index", &bpm, cmp, 4, 4);

    for (int64_t i = 1; i <= 50; ++i) {
        tree.Insert(i, minidb::RID(i, 0));
    }

    // Delete odd keys
    for (int64_t i = 1; i <= 50; i += 2) {
        tree.Remove(i);
    }

    // Verify odd keys are gone and even keys remain
    for (int64_t i = 1; i <= 50; ++i) {
        std::vector<minidb::RID> res;
        if (i % 2 == 1) {
            assert(!tree.GetValue(i, &res));
        } else {
            assert(tree.GetValue(i, &res));
            assert(res[0].GetPageId() == i);
        }
    }

    // Verify range scan iterates over even keys only
    int64_t expected_even = 2;
    int even_count = 0;
    for (auto it = tree.Begin(); it != tree.End(); ++it) {
        const auto& [k, v] = *it;
        assert(k == expected_even);
        expected_even += 2;
        even_count++;
    }
    assert(even_count == 25);

    std::remove(db_name.c_str());
    std::cout << "  Passed!" << std::endl;
}

int main() {
    std::cout << "=== Running B+ Tree Index Unit Tests ===" << std::endl;
    TestBPlusTreeBasic();
    TestBPlusTreeScaleAndRangeScan();
    TestBPlusTreeDeletions();
    std::cout << "All B+ Tree Index tests passed successfully!\n" << std::endl;
    return 0;
}
