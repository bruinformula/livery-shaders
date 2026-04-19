#pragma once 

#include <string>
#include <cstdint>

constexpr uint32_t hashString(std::string_view s) {
    uint32_t h = 2166136261u;
    for (char c : s)
        h = (h ^ c) * 16777619u;
    return h;
}

struct ArgumentHandler {

    enum ParseResult {
        SUCCESS,
        SUCCESS_CONSUME_NEXT,
        FAILURE,
        EXIT
    };

    virtual ParseResult parse(const std::string& token, const std::string& nextToken) = 0;

    virtual bool verify() const = 0;
};