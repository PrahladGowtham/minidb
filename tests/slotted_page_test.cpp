#include "storage/page/slotted_page.hpp"
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

void TestSlottedPageBasic() {
    std::cout << "[Test] SlottedPage Basic Insertion & Retrieval..." << std::endl;
    char page_buffer[minidb::PAGE_SIZE];
    minidb::SlottedPage page(page_buffer);
    page.Init(1);

    assert(page.GetPageId() == 1);
    assert(page.GetNumRecords() == 0);
    assert(page.GetNumSlots() == 0);

    // Insert 3 records of different lengths
    std::string rec1 = "Hello, Minidb Database Engine!";
    std::string rec2 = "A shorter row";
    std::string rec3 = "A much longer record containing detailed binary/text information for testing variable slotted records.";

    minidb::slot_id_t slot0, slot1, slot2;
    assert(page.InsertRecord(rec1.data(), rec1.size(), &slot0));
    assert(page.InsertRecord(rec2.data(), rec2.size(), &slot1));
    assert(page.InsertRecord(rec3.data(), rec3.size(), &slot2));

    assert(slot0 == 0);
    assert(slot1 == 1);
    assert(slot2 == 2);
    assert(page.GetNumRecords() == 3);

    // Read and verify
    std::string_view read_val;
    assert(page.GetRecord(slot0, &read_val));
    assert(read_val == rec1);

    assert(page.GetRecord(slot1, &read_val));
    assert(read_val == rec2);

    assert(page.GetRecord(slot2, &read_val));
    assert(read_val == rec3);

    std::cout << "  Passed!" << std::endl;
}

void TestSlottedPageDeleteAndDefrag() {
    std::cout << "[Test] SlottedPage Deletion and Defragmentation..." << std::endl;
    char page_buffer[minidb::PAGE_SIZE];
    minidb::SlottedPage page(page_buffer);
    page.Init(2);

    std::vector<minidb::slot_id_t> slots;
    // Insert 20 records
    for (int i = 0; i < 20; ++i) {
        std::string data = "Record number: " + std::to_string(i) + " with padding bytes 1234567890";
        minidb::slot_id_t slot;
        assert(page.InsertRecord(data.data(), data.size(), &slot));
        slots.push_back(slot);
    }
    assert(page.GetNumRecords() == 20);

    // Delete odd slots
    for (size_t i = 1; i < slots.size(); i += 2) {
        assert(page.DeleteRecord(slots[i]));
    }
    assert(page.GetNumRecords() == 10);

    // Defragment page
    page.Defragment();

    // Verify even slots remain intact
    for (size_t i = 0; i < slots.size(); i += 2) {
        std::string expected = "Record number: " + std::to_string(i) + " with padding bytes 1234567890";
        std::string_view read_val;
        assert(page.GetRecord(slots[i], &read_val));
        assert(read_val == expected);
    }

    std::cout << "  Passed!" << std::endl;
}

int main() {
    std::cout << "=== Running Slotted Page Unit Tests ===" << std::endl;
    TestSlottedPageBasic();
    TestSlottedPageDeleteAndDefrag();
    std::cout << "All Slotted Page tests passed successfully!\n" << std::endl;
    return 0;
}
