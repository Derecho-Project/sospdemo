#pragma once

#include <derecho/core/derecho.hpp>
#include <vector>

/**
 * NDArray data type
 */
enum TypeFlag {
    kFloat32 = 0,
    kFloat64 = 1,
    kFloat16 = 2,
    kUint8 = 3,
    kInt32 = 4,
    kInt8 = 5,
    kInt64 = 6,
};
