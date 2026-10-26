#pragma once

#include <stdexcept>
#include <string>

namespace minidb {

class DBException : public std::runtime_error {
public:
    explicit DBException(const std::string& msg) : std::runtime_error(msg) {}
};

} // namespace minidb
