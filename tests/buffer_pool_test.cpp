#include "storage/disk/disk_manager.hpp"
#include "storage/buffer/buffer_pool_manager.hpp"
#include <cassert>
#include <iostream>
#include <cstdio>
#include <cstring>
#include <vector>

void TestBufferPoolBasic() {
    std::cout << "[Test] BufferPoolManager Basic Operations..." << std::endl;
    const std::string db_name = "test_bpm_basic.db";
    std::remove(db_name.c_str());

    {
        auto disk_manager = std::make_unique<minidb::DiskManager>(db_name);
        minidb::BufferPoolManager bpm(5, disk_manager.get());

        // Allocate 5 pages (filling the buffer pool)
        std::vector<minidb::page_id_t> page_ids;
        for (int i = 0; i < 5; ++i) {
            minidb::page_id_t pid;
            minidb::Page* page = bpm.NewPage(&pid);
            assert(page != nullptr);
            assert(pid == i);
            std::snprintf(page->GetData(), minidb::PAGE_SIZE, "Page data for page ID %d", pid);
            bpm.UnpinPage(pid, true); // Dirty
            page_ids.push_back(pid);
        }

        // Allocate a 6th page - should evict one of the unpinned pages
        minidb::page_id_t pid5;
        minidb::Page* page5 = bpm.NewPage(&pid5);
        assert(page5 != nullptr);
        assert(pid5 == 5);
        std::snprintf(page5->GetData(), minidb::PAGE_SIZE, "Page data for page ID 5");
        bpm.UnpinPage(pid5, true);

        // Fetch back page 0 (which may have been evicted to disk and reloaded)
        minidb::Page* page0 = bpm.FetchPage(0);
        assert(page0 != nullptr);
        assert(std::string(page0->GetData()) == "Page data for page ID 0");
        bpm.UnpinPage(0, false);
    }

    // Reopen database with fresh buffer pool to test disk persistence
    {
        auto disk_manager = std::make_unique<minidb::DiskManager>(db_name);
        minidb::BufferPoolManager bpm(10, disk_manager.get());

        for (int i = 0; i <= 5; ++i) {
            minidb::Page* page = bpm.FetchPage(i);
            assert(page != nullptr);
            std::string expected = "Page data for page ID " + std::to_string(i);
            assert(std::string(page->GetData()) == expected);
            bpm.UnpinPage(i, false);
        }
    }

    std::remove(db_name.c_str());
    std::cout << "  Passed!" << std::endl;
}

void TestBufferPoolPinningLimit() {
    std::cout << "[Test] BufferPoolManager Pinning Exhaustion..." << std::endl;
    const std::string db_name = "test_bpm_pinning.db";
    std::remove(db_name.c_str());

    auto disk_manager = std::make_unique<minidb::DiskManager>(db_name);
    // Buffer pool of size 3
    minidb::BufferPoolManager bpm(3, disk_manager.get());

    minidb::page_id_t p0, p1, p2, p3;
    minidb::Page* page0 = bpm.NewPage(&p0);
    minidb::Page* page1 = bpm.NewPage(&p1);
    minidb::Page* page2 = bpm.NewPage(&p2);

    assert(page0 != nullptr && page1 != nullptr && page2 != nullptr);
    // All 3 pages are currently pinned (pin_count = 1)

    // Attempting to allocate a 4th page must fail because all frames are pinned
    minidb::Page* page3 = bpm.NewPage(&p3);
    assert(page3 == nullptr);

    // Unpin page 0
    bpm.UnpinPage(p0, false);

    // Now allocation should succeed by evicting page 0
    page3 = bpm.NewPage(&p3);
    assert(page3 != nullptr);
    bpm.UnpinPage(p3, false);
    bpm.UnpinPage(p1, false);
    bpm.UnpinPage(p2, false);

    std::remove(db_name.c_str());
    std::cout << "  Passed!" << std::endl;
}

int main() {
    std::cout << "=== Running Buffer Pool Manager Unit Tests ===" << std::endl;
    TestBufferPoolBasic();
    TestBufferPoolPinningLimit();
    std::cout << "All Buffer Pool tests passed successfully!\n" << std::endl;
    return 0;
}
