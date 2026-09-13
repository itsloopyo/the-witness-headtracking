#pragma once

#include <cmath>
#include <iostream>

// Same shape as cameraunlock-core's own tests: no framework, one process, a
// non-zero exit code when anything failed.
namespace twht_test {

inline int& Failures() {
    static int failures = 0;
    return failures;
}

inline void Check(bool condition, const char* name) {
    if (condition) {
        std::cout << "  [PASS] " << name << "\n";
    } else {
        std::cout << "  [FAIL] " << name << "\n";
        ++Failures();
    }
}

inline bool NearEqual(float a, float b, float eps = 1e-4f) {
    return std::fabs(a - b) <= eps;
}

}  // namespace twht_test
