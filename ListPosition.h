#pragma once

#include <string>

struct ListPosition {
    size_t listPos;
    std::string listPath;
    size_t  listLength;
    size_t listStartPosition;
    size_t listEndPosition;
    size_t listStartMatch;
};