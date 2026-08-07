// Regression test: the std::formatter specializations must be usable when
// compiled with exceptions disabled (-fno-exceptions), as on embedded targets
// such as ESP-IDF with CONFIG_COMPILER_CXX_EXCEPTIONS unset.
//
// This is a separate target from thermo_tests because Catch2 requires
// exceptions. It deliberately avoids any test framework so the whole
// translation unit builds without them.

#include <cstdio>
#include <string>
#include <thermo/thermo>

#ifdef __cpp_exceptions
#error "this test must be compiled with exceptions disabled"
#endif

using namespace thermo;

namespace {

int failures = 0;

void check(const std::string& actual, const std::string& expected, const char* what) {
    if (actual != expected) {
        std::printf("FAIL: %s: expected \"%s\", got \"%s\"\n", what, expected.c_str(), actual.c_str());
        ++failures;
    }
}

} // namespace

int main() {
    // Formatting with no spec: instantiating parse() must not pull in a throw.
    check(std::format("{}", celsius(20)), "20°C", "{} celsius");
    check(std::format("{}", decicelsius(225)), "225d°C", "{} decicelsius");
    check(std::format("{}", millicelsius(22500)), "22500m°C", "{} millicelsius");
    check(std::format("{}", fahrenheit(72)), "72°F", "{} fahrenheit");
    check(std::format("{}", delta_celsius(20)), "20Δ°C", "{} delta_celsius");
    check(std::format("{}", delta_millicelsius(1500)), "1500Δm°C", "{} delta_millicelsius");

    // Formatting with an explicit spec.
    check(std::format("{:.1f}", decicelsius(225)), "22.5°C", "{:.1f} decicelsius");
    check(std::format("{:.1f}", millicelsius(22534)), "22.5°C", "{:.1f} millicelsius");
    check(std::format("{:.1f}", delta_millicelsius(1500)), "1.5Δ°C", "{:.1f} delta_millicelsius");

    // A precision with no SI prefix is still usable with an explicit spec.
    using centicelsius = temperature<celsius_scale, delta<int64_t, std::centi>>;
    check(std::format("{:.2f}", centicelsius(2250)), "22.50°C", "{:.2f} centicelsius");

    if (failures != 0) {
        std::printf("%d check(s) failed\n", failures);
        return 1;
    }
    std::printf("all checks passed\n");
    return 0;
}
