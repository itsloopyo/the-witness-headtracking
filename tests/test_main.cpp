#include "test_harness.h"

#include <iostream>

void RunPoseConversionTests();
void RunReticleProjectionTests();
void RunConfigTests();

int main() {
    RunPoseConversionTests();
    RunReticleProjectionTests();
    RunConfigTests();

    const int failures = twht_test::Failures();
    if (failures == 0) {
        std::cout << "All tests passed.\n";
        return 0;
    }
    std::cout << failures << " test(s) failed.\n";
    return 1;
}
