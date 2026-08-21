#pragma once

#include <cstdint>

struct Order{
    uint64_t bid;
    char side;
    uint32_t price;
    uint32_t quantity;
};