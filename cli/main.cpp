#include "api/minidb.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>

void PrintTable(const std::vector<std::string>& headers, const std::vector<std::vector<std::string>>& rows) {
    if (headers.empty()) return;

    std::vector<size_t> col_widths(headers.size(), 0);
    for (size_t i = 0; i < headers.size(); ++i) {
        col_widths[i] = std::max(col_widths[i], headers[i].size());
    }

    for (const auto& row : rows) {
        for (size_t i = 0; i < row.size(); ++i) {
            col_widths[i] = std::max(col_widths[i], row[i].size());
        }
    }

    auto print_divider = [&]() {
        std::cout << "+";
        for (size_t w : col_widths) {
            std::cout << std::string(w + 2, '-') << "+";
        }
        std::cout << "\n";
    };

    print_divider();
    std::cout << "|";
    for (size_t i = 0; i < headers.size(); ++i) {
        std::cout << " " << std::left << std::setw(col_widths[i]) << headers[i] << " |";
    }
    std::cout << "\n";
    print_divider();

    for (const auto& row : rows) {
        std::cout << "|";
        for (size_t i = 0; i < row.size(); ++i) {
            std::cout << " " << std::left << std::setw(col_widths[i]) << row[i] << " |";
        }
        std::cout << "\n";
    }
    print_divider();
}

int main(int argc, char* argv[]) {
    std::string db_file = "minidb.db";
    if (argc > 1) {
        db_file = argv[1];
    }

    std::cout << "========================================================\n";
    std::cout << "        MiniDB - Relational Database Engine (C++20)      \n";
    std::cout << "   Storage: Slotted-Page Buffer Pool | Indexing: B+ Tree\n";
    std::cout << "========================================================\n";
    std::cout << "Connected to database: " << db_file << "\n";
    std::cout << "Type .help for instructions, .exit to quit.\n\n";

    auto db = minidb::Database::Open(db_file);
    bool timer_enabled = true;
    std::string sql_buffer;

    while (true) {
        if (sql_buffer.empty()) {
            std::cout << "minidb> ";
        } else {
            std::cout << "   ...> ";
        }

        std::string line;
        if (!std::getline(std::cin, line)) {
            std::cout << "\nExiting MiniDB.\n";
            break;
        }

        // Trim leading and trailing whitespace
        size_t first = line.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) {
            continue;
        }
        std::string trimmed = line.substr(first, line.find_last_not_of(" \t\r\n") - first + 1);

        // Handle Meta-commands
        if (sql_buffer.empty() && trimmed[0] == '.') {
            if (trimmed == ".exit" || trimmed == ".quit") {
                std::cout << "Closing database and syncing disk buffers... Bye!\n";
                break;
            } else if (trimmed == ".help") {
                std::cout << "Meta-Commands:\n";
                std::cout << "  .tables            List all tables in the database\n";
                std::cout << "  .schema [table]    Show schema of a table or all tables\n";
                std::cout << "  .timer on|off      Toggle query execution time measurement\n";
                std::cout << "  .flush             Sync all buffer pool pages to disk\n";
                std::cout << "  .exit / .quit      Exit the CLI\n";
            } else if (trimmed == ".tables") {
                auto tables = db->GetTableNames();
                if (tables.empty()) {
                    std::cout << "No tables found in database.\n";
                } else {
                    for (const auto& t : tables) {
                        std::cout << "  - " << t << "\n";
                    }
                }
            } else if (trimmed.rfind(".schema", 0) == 0) {
                std::istringstream iss(trimmed);
                std::string cmd, tbl;
                iss >> cmd >> tbl;

                std::vector<std::string> target_tables;
                if (tbl.empty()) {
                    target_tables = db->GetTableNames();
                } else {
                    target_tables.push_back(tbl);
                }

                for (const auto& tname : target_tables) {
                    const auto* info = db->GetTable(tname);
                    if (!info) {
                        std::cout << "Table '" << tname << "' not found.\n";
                        continue;
                    }
                    std::cout << "Table: " << tname << "\n";
                    for (uint32_t i = 0; i < info->schema.GetColumnCount(); ++i) {
                        const auto& col = info->schema.GetColumn(i);
                        std::cout << "  " << std::left << std::setw(16) << col.GetName()
                                  << " " << minidb::TypeIdToString(col.GetType()) << "\n";
                    }
                }
            } else if (trimmed == ".timer on") {
                timer_enabled = true;
                std::cout << "Timer enabled.\n";
            } else if (trimmed == ".timer off") {
                timer_enabled = false;
                std::cout << "Timer disabled.\n";
            } else if (trimmed == ".flush") {
                db->Flush();
                std::cout << "Flushed all dirty buffer pool frames to disk.\n";
            } else {
                std::cout << "Unknown command: " << trimmed << " (try .help)\n";
            }
            continue;
        }

        sql_buffer += " " + trimmed;

        if (sql_buffer.back() == ';') {
            auto start_time = std::chrono::high_resolution_clock::now();
            auto result = db->Execute(sql_buffer);
            auto end_time = std::chrono::high_resolution_clock::now();
            double elapsed_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

            if (!result.success) {
                std::cout << "Error: " << result.message << "\n";
            } else {
                if (!result.column_names.empty()) {
                    PrintTable(result.column_names, result.rows);
                }
                std::cout << result.message;
                if (timer_enabled) {
                    std::cout << " (" << std::fixed << std::setprecision(3) << elapsed_ms << " ms)";
                }
                std::cout << "\n";
            }

            sql_buffer.clear();
        }
    }

    return 0;
}
