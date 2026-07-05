// Unit tests for thermo library
// Uses Catch2 test framework with compile-time static_asserts

#include <catch2/catch_test_macros.hpp>
#include <thermo/thermo>

using namespace thermo;
using namespace thermo_literals;

// =============================================================================
// Delta construction and arithmetic
// =============================================================================

// Compile-time tests
static_assert(delta_celsius(10).count() == 10);
static_assert(delta_celsius(5) + delta_celsius(3) == delta_celsius(8));
static_assert(delta_celsius(5) - delta_celsius(3) == delta_celsius(2));
static_assert(delta_celsius(5) * 2 == delta_celsius(10));
static_assert(delta_celsius(10) / 2 == delta_celsius(5));

// Runtime tests
TEST_CASE("delta construction", "[thermo][delta]") {
    REQUIRE(delta_celsius(10).count() == 10);
    REQUIRE(delta_celsius::zero().count() == 0);
}

TEST_CASE("delta arithmetic", "[thermo][delta]") {
    REQUIRE((delta_celsius(5) + delta_celsius(3)).count() == 8);
    REQUIRE((delta_celsius(5) - delta_celsius(3)).count() == 2);
    REQUIRE((delta_celsius(5) * 2).count() == 10);
    REQUIRE((delta_celsius(10) / 2).count() == 5);
}

// =============================================================================
// Delta precision conversions
// =============================================================================

// Compile-time tests
static_assert(delta_celsius(1) == delta_millicelsius(1000));
static_assert(delta_fahrenheit(9) == delta_celsius(5));

// Runtime tests
TEST_CASE("delta precision conversion", "[thermo][delta]") {
    // milli to base (explicit, lossy - divides by 1000)
    delta_celsius dc = delta_cast<delta_celsius>(delta_millicelsius(1000));
    REQUIRE(dc.count() == 1);

    // base to milli (implicit, lossless - multiplies by 1000)
    delta_millicelsius dmc = delta_celsius(1);
    REQUIRE(dmc.count() == 1000);

    // fahrenheit to celsius delta
    REQUIRE(delta_fahrenheit(9) == delta_celsius(5));
}

// =============================================================================
// Temperature construction
// =============================================================================

// Compile-time tests
static_assert(celsius(0).count() == 0);
static_assert(celsius(100).count() == 100);
static_assert(kelvin(273).count() == 273);
static_assert(millicelsius(1000).count() == 1000);
static_assert(millikelvin(273150).count() == 273150);

// Runtime tests
TEST_CASE("temperature construction", "[thermo][temperature]") {
    REQUIRE(celsius(0).count() == 0);
    REQUIRE(celsius(100).count() == 100);
    REQUIRE(kelvin(273).count() == 273);
    REQUIRE(fahrenheit(32).count() == 32);
}

// =============================================================================
// Same-scale precision conversions
// =============================================================================

// Compile-time tests
constexpr millicelsius mc_from_c = celsius(1);
static_assert(mc_from_c.count() == 1000);
constexpr millikelvin mk_from_k = kelvin(273);
static_assert(mk_from_k.count() == 273000);
static_assert(celsius(millicelsius(1500)).count() == 1);

// Runtime tests
TEST_CASE("celsius to millicelsius conversion", "[thermo][temperature]") {
    millicelsius mc = celsius(1); // implicit, lossless
    REQUIRE(mc.count() == 1000);

    millicelsius mc2 = celsius(25);
    REQUIRE(mc2.count() == 25000);
}

TEST_CASE("millicelsius to celsius conversion", "[thermo][temperature]") {
    // explicit required (lossy)
    celsius c = celsius(millicelsius(1500));
    REQUIRE(c.count() == 1); // truncates

    celsius c2 = celsius(millicelsius(1999));
    REQUIRE(c2.count() == 1); // still truncates
}

// =============================================================================
// Cross-scale conversions
// =============================================================================

// Compile-time tests
static_assert(kelvin(celsius(0)).count() == 273);
static_assert(kelvin(celsius(100)).count() == 373);
static_assert(celsius(kelvin(273)).count() == 0);

// Millikelvin precision (lossless)
constexpr millikelvin mk_from_0c = temperature_cast<millikelvin>(celsius(0));
static_assert(mk_from_0c.count() == 273150);
constexpr millikelvin mk_from_mc = millicelsius(0);
static_assert(mk_from_mc.count() == 273150);

// Fahrenheit conversions
static_assert(fahrenheit(32).count() == 32);
static_assert(celsius(fahrenheit(32)).count() == 0);
static_assert(celsius(fahrenheit(212)).count() == 100);
static_assert(celsius(fahrenheit(0)).count() == -17);

// Lossless conversion concept
static_assert(_lossless_temperature_conversion<celsius_scale, delta_celsius, celsius_scale, delta_millicelsius>);
static_assert(!_lossless_temperature_conversion<celsius_scale, delta_millicelsius, celsius_scale, delta_celsius>);
static_assert(_lossless_temperature_conversion<kelvin_scale, delta_kelvin, kelvin_scale, delta_millikelvin>);
static_assert(_lossless_temperature_conversion<celsius_scale, delta_celsius, kelvin_scale, delta_millikelvin>);
static_assert(!_lossless_temperature_conversion<kelvin_scale, delta_kelvin, celsius_scale, delta_celsius>);

// Runtime tests
TEST_CASE("celsius to kelvin conversion", "[thermo][temperature]") {
    // 0°C = 273.15 K -> 273 (truncated)
    kelvin k0 = kelvin(celsius(0));
    REQUIRE(k0.count() == 273);

    // 100°C = 373.15 K -> 373 (truncated)
    kelvin k100 = kelvin(celsius(100));
    REQUIRE(k100.count() == 373);

    // -40°C = 233.15 K -> 233 (truncated)
    kelvin km40 = kelvin(celsius(-40));
    REQUIRE(km40.count() == 233);
}

TEST_CASE("kelvin to celsius conversion", "[thermo][temperature]") {
    // 273 K = -0.15°C -> 0 (truncated)
    celsius c273 = celsius(kelvin(273));
    REQUIRE(c273.count() == 0);

    // 373 K = 99.85°C -> 99 (truncated)
    celsius c373 = celsius(kelvin(373));
    REQUIRE(c373.count() == 99);

    // 0 K = -273.15°C -> -273 (truncated)
    celsius c0 = celsius(kelvin(0));
    REQUIRE(c0.count() == -273);
}

TEST_CASE("celsius to millikelvin conversion", "[thermo][temperature]") {
    // With millikelvin precision, conversion is exact
    millikelvin mk = temperature_cast<millikelvin>(celsius(0));
    REQUIRE(mk.count() == 273150);

    millikelvin mk100 = temperature_cast<millikelvin>(celsius(100));
    REQUIRE(mk100.count() == 373150);
}

TEST_CASE("millicelsius to millikelvin implicit conversion", "[thermo][temperature]") {
    // Same precision, different scale - should be lossless and implicit
    millikelvin mk = millicelsius(0);
    REQUIRE(mk.count() == 273150);

    millikelvin mk1000 = millicelsius(1000); // 1°C
    REQUIRE(mk1000.count() == 274150);
}

TEST_CASE("fahrenheit to celsius conversion", "[thermo][temperature]") {
    // 32°F = 0°C
    celsius c32 = celsius(fahrenheit(32));
    REQUIRE(c32.count() == 0);

    // 212°F = 100°C
    celsius c212 = celsius(fahrenheit(212));
    REQUIRE(c212.count() == 100);

    // 0°F = -17.78°C -> -17 (truncated)
    celsius c0 = celsius(fahrenheit(0));
    REQUIRE(c0.count() == -17);

    // -40°F = -40°C (the scales meet)
    celsius cm40 = celsius(fahrenheit(-40));
    REQUIRE(cm40.count() == -40);
}

// =============================================================================
// Temperature arithmetic
// =============================================================================

// Compile-time tests
static_assert((celsius(20) + delta_celsius(5)).count() == 25);
static_assert((celsius(20) - delta_celsius(5)).count() == 15);
static_assert((delta_celsius(5) + celsius(20)).count() == 25);
static_assert((celsius(20) + celsius(10)).count() == 30);
static_assert((celsius(20) - celsius(10)).count() == 10);
static_assert(difference(celsius(25), celsius(20)).count() == 5);

// Runtime tests
TEST_CASE("temperature arithmetic with delta", "[thermo][temperature]") {
    celsius t = celsius(20);

    REQUIRE((t + delta_celsius(5)).count() == 25);
    REQUIRE((t - delta_celsius(5)).count() == 15);
    REQUIRE((delta_celsius(5) + t).count() == 25);
}

TEST_CASE("temperature arithmetic with temperature", "[thermo][temperature]") {
    REQUIRE((celsius(20) + celsius(10)).count() == 30);
    REQUIRE((celsius(20) - celsius(10)).count() == 10);
}

TEST_CASE("temperature difference", "[thermo][temperature]") {
    delta_celsius d = difference(celsius(25), celsius(20));
    REQUIRE(d.count() == 5);

    delta_celsius d2 = difference(celsius(20), celsius(25));
    REQUIRE(d2.count() == -5);
}

TEST_CASE("temperature increment/decrement", "[thermo][temperature]") {
    celsius t(10);

    ++t;
    REQUIRE(t.count() == 11);

    --t;
    REQUIRE(t.count() == 10);

    t++;
    REQUIRE(t.count() == 11);

    t--;
    REQUIRE(t.count() == 10);
}

TEST_CASE("temperature compound assignment", "[thermo][temperature]") {
    celsius t(10);

    t += delta_celsius(5);
    REQUIRE(t.count() == 15);

    t -= delta_celsius(3);
    REQUIRE(t.count() == 12);
}

// =============================================================================
// Comparison operators
// =============================================================================

// Compile-time tests
static_assert(celsius(20) == celsius(20));
static_assert(celsius(20) != celsius(21));
static_assert(celsius(20) < celsius(21));
static_assert(celsius(21) > celsius(20));
static_assert(celsius(20) <= celsius(21));
static_assert(celsius(21) >= celsius(20));
static_assert(millicelsius(celsius(1)) == millicelsius(1000));

// Runtime tests
TEST_CASE("temperature comparison", "[thermo][temperature]") {
    REQUIRE(celsius(20) == celsius(20));
    REQUIRE(celsius(20) != celsius(21));
    REQUIRE(celsius(20) < celsius(21));
    REQUIRE(celsius(21) > celsius(20));
    REQUIRE(celsius(20) <= celsius(20));
    REQUIRE(celsius(20) <= celsius(21));
    REQUIRE(celsius(21) >= celsius(21));
    REQUIRE(celsius(21) >= celsius(20));
}

// =============================================================================
// Literals
// =============================================================================

// Compile-time tests
static_assert((20_c).count() == 20);
static_assert((1000_mc).count() == 1000);
static_assert((273_k).count() == 273);
static_assert((273000_mk).count() == 273000);
static_assert((32_f).count() == 32);
static_assert((5_Δc).count() == 5);
static_assert((1000_Δmc).count() == 1000);
static_assert((5_Δk).count() == 5);
static_assert((1000_Δmk).count() == 1000);
static_assert((9_Δf).count() == 9);
static_assert((9000_Δmf).count() == 9000);

// Runtime tests
TEST_CASE("temperature literals", "[thermo][literals]") {
    REQUIRE((20_c).count() == 20);
    REQUIRE((1000_mc).count() == 1000);
    REQUIRE((273_k).count() == 273);
    REQUIRE((273000_mk).count() == 273000);
    REQUIRE((32_f).count() == 32);
}

TEST_CASE("delta literals", "[thermo][literals]") {
    REQUIRE((5_Δc).count() == 5);
    REQUIRE((1000_Δmc).count() == 1000);
    REQUIRE((5_Δk).count() == 5);
    REQUIRE((1000_Δmk).count() == 1000);
    REQUIRE((9_Δf).count() == 9);
    REQUIRE((9000_Δmf).count() == 9000);
}

// =============================================================================
// String formatting
// =============================================================================

// Runtime tests
TEST_CASE("to_string", "[thermo][string]") {
    REQUIRE(to_string(celsius(20)) == "20°C");
    REQUIRE(to_string(kelvin(273)) == "273K");
    REQUIRE(to_string(fahrenheit(32)) == "32°F");
    REQUIRE(to_string(millicelsius(1000)) == "1000m°C");
    REQUIRE(to_string(millikelvin(273150)) == "273150mK");
}

#if __has_include(<format>) && defined(__cpp_lib_format)
TEST_CASE("std::format delta", "[thermo][string][delta]") {
    REQUIRE(std::format("{}", delta_celsius(20)) == "20Δ°C");
    REQUIRE(std::format("{}", delta_decicelsius(200)) == "200Δd°C");
    REQUIRE(std::format("{}", delta_millicelsius(20000)) == "20000Δm°C");
    REQUIRE(std::format("{}", delta_fahrenheit(36)) == "36Δ°F");
    REQUIRE(std::format("{}", delta_decifahrenheit(360)) == "360Δd°F");
    REQUIRE(std::format("{}", delta_millifahrenheit(36000)) == "36000Δm°F");
}

TEST_CASE("std::format real temperature", "[thermo][string][real]") {
    // count() holds the degree value, so it formats directly (no precision suffix).
    REQUIRE(std::format("{}", celsius_real(22.5)) == "22.5°C");
    REQUIRE(std::format("{}", celsius_real(20.0)) == "20°C");
    REQUIRE(std::format("{}", kelvin_real(295.65)) == "295.65K");
    REQUIRE(std::format("{}", fahrenheit_real(72.5)) == "72.5°F");

    // The standard floating-point format spec is honored.
    REQUIRE(std::format("{:.2f}", celsius_real(22.5)) == "22.50°C");
    REQUIRE(std::format("{:.1f}", celsius_real(22.53)) == "22.5°C");

    // Intended use: cast an exact integer reading to a real-valued type for display.
    REQUIRE(std::format("{:.1f}", temperature_cast<celsius_real>(millicelsius(22500))) == "22.5°C");
    REQUIRE(std::format("{:.1f}", temperature_cast<fahrenheit_real>(millicelsius(22500))) == "72.5°F");
}
#endif

TEST_CASE("real-valued temperature conversion", "[thermo][real]") {
    // count() reads directly in scale degrees; same-scale casts are exact here.
    REQUIRE(celsius_real(millicelsius(22500)).count() == 22.5); // implicit
    REQUIRE(temperature_cast<celsius_real>(millicelsius(22500)).count() == 22.5);
    REQUIRE(temperature_cast<celsius_real>(decicelsius(225)).count() == 22.5);
}

// =============================================================================
// Min/max values
// =============================================================================

// Compile-time tests
static_assert(celsius::min().count() < 0);
static_assert(celsius::max().count() > 0);

// =============================================================================
// Rounding functions
// =============================================================================

// Compile-time tests for ceil
static_assert(thermo::ceil<delta_celsius>(delta_millicelsius(12345)).count() == 13);
static_assert(thermo::ceil<delta_celsius>(delta_millicelsius(12000)).count() == 12);
static_assert(thermo::ceil<delta_celsius>(delta_millicelsius(-12345)).count() == -12);
static_assert(thermo::ceil<delta_celsius>(delta_millicelsius(0)).count() == 0);

// Compile-time tests for floor
static_assert(thermo::floor<delta_celsius>(delta_millicelsius(12345)).count() == 12);
static_assert(thermo::floor<delta_celsius>(delta_millicelsius(12000)).count() == 12);
static_assert(thermo::floor<delta_celsius>(delta_millicelsius(-12345)).count() == -13);
static_assert(thermo::floor<delta_celsius>(delta_millicelsius(0)).count() == 0);

// Compile-time tests for trunc
static_assert(thermo::trunc<delta_celsius>(delta_millicelsius(12345)).count() == 12);
static_assert(thermo::trunc<delta_celsius>(delta_millicelsius(12000)).count() == 12);
static_assert(thermo::trunc<delta_celsius>(delta_millicelsius(-12345)).count() == -12);
static_assert(thermo::trunc<delta_celsius>(delta_millicelsius(0)).count() == 0);

// Compile-time tests for round
static_assert(thermo::round<delta_celsius>(delta_millicelsius(12345)).count() == 12);
static_assert(thermo::round<delta_celsius>(delta_millicelsius(12500)).count() == 13);
static_assert(thermo::round<delta_celsius>(delta_millicelsius(12600)).count() == 13);
static_assert(thermo::round<delta_celsius>(delta_millicelsius(-12345)).count() == -12);
static_assert(thermo::round<delta_celsius>(delta_millicelsius(-12500)).count() == -13);
static_assert(thermo::round<delta_celsius>(delta_millicelsius(-12600)).count() == -13);

// Runtime tests
TEST_CASE("delta ceil", "[thermo][delta][rounding]") {
    REQUIRE(thermo::ceil<delta_celsius>(delta_millicelsius(12345)).count() == 13);
    REQUIRE(thermo::ceil<delta_celsius>(delta_millicelsius(12000)).count() == 12);
    REQUIRE(thermo::ceil<delta_celsius>(delta_millicelsius(12001)).count() == 13);
    REQUIRE(thermo::ceil<delta_celsius>(delta_millicelsius(-12345)).count() == -12);
    REQUIRE(thermo::ceil<delta_celsius>(delta_millicelsius(-12000)).count() == -12);
    REQUIRE(thermo::ceil<delta_celsius>(delta_millicelsius(-12001)).count() == -12);
    REQUIRE(thermo::ceil<delta_celsius>(delta_millicelsius(0)).count() == 0);

    // Same precision should return identical value
    REQUIRE(thermo::ceil<delta_celsius>(delta_celsius(42)).count() == 42);

    // Finer precision should return lossless conversion
    REQUIRE(thermo::ceil<delta_millicelsius>(delta_celsius(5)).count() == 5000);
}

TEST_CASE("delta floor", "[thermo][delta][rounding]") {
    REQUIRE(thermo::floor<delta_celsius>(delta_millicelsius(12345)).count() == 12);
    REQUIRE(thermo::floor<delta_celsius>(delta_millicelsius(12000)).count() == 12);
    REQUIRE(thermo::floor<delta_celsius>(delta_millicelsius(12999)).count() == 12);
    REQUIRE(thermo::floor<delta_celsius>(delta_millicelsius(-12345)).count() == -13);
    REQUIRE(thermo::floor<delta_celsius>(delta_millicelsius(-12000)).count() == -12);
    REQUIRE(thermo::floor<delta_celsius>(delta_millicelsius(-12001)).count() == -13);
    REQUIRE(thermo::floor<delta_celsius>(delta_millicelsius(0)).count() == 0);

    // Same precision should return identical value
    REQUIRE(thermo::floor<delta_celsius>(delta_celsius(42)).count() == 42);

    // Finer precision should return lossless conversion
    REQUIRE(thermo::floor<delta_millicelsius>(delta_celsius(5)).count() == 5000);
}

TEST_CASE("delta trunc", "[thermo][delta][rounding]") {
    REQUIRE(thermo::trunc<delta_celsius>(delta_millicelsius(12345)).count() == 12);
    REQUIRE(thermo::trunc<delta_celsius>(delta_millicelsius(12000)).count() == 12);
    REQUIRE(thermo::trunc<delta_celsius>(delta_millicelsius(12999)).count() == 12);
    REQUIRE(thermo::trunc<delta_celsius>(delta_millicelsius(-12345)).count() == -12);
    REQUIRE(thermo::trunc<delta_celsius>(delta_millicelsius(-12000)).count() == -12);
    REQUIRE(thermo::trunc<delta_celsius>(delta_millicelsius(-12999)).count() == -12);
    REQUIRE(thermo::trunc<delta_celsius>(delta_millicelsius(0)).count() == 0);

    // Same precision should return identical value
    REQUIRE(thermo::trunc<delta_celsius>(delta_celsius(42)).count() == 42);

    // Finer precision should return lossless conversion
    REQUIRE(thermo::trunc<delta_millicelsius>(delta_celsius(5)).count() == 5000);
}

TEST_CASE("delta round", "[thermo][delta][rounding]") {
    // Basic rounding
    REQUIRE(thermo::round<delta_celsius>(delta_millicelsius(12345)).count() == 12);
    REQUIRE(thermo::round<delta_celsius>(delta_millicelsius(12499)).count() == 12);
    REQUIRE(thermo::round<delta_celsius>(delta_millicelsius(12500)).count() == 13); // Tie: round away from zero
    REQUIRE(thermo::round<delta_celsius>(delta_millicelsius(12501)).count() == 13);
    REQUIRE(thermo::round<delta_celsius>(delta_millicelsius(12999)).count() == 13);
    REQUIRE(thermo::round<delta_celsius>(delta_millicelsius(12000)).count() == 12);

    // Negative values
    REQUIRE(thermo::round<delta_celsius>(delta_millicelsius(-12345)).count() == -12);
    REQUIRE(thermo::round<delta_celsius>(delta_millicelsius(-12499)).count() == -12);
    REQUIRE(thermo::round<delta_celsius>(delta_millicelsius(-12500)).count() == -13); // Tie: round away from zero
    REQUIRE(thermo::round<delta_celsius>(delta_millicelsius(-12501)).count() == -13);
    REQUIRE(thermo::round<delta_celsius>(delta_millicelsius(-12999)).count() == -13);
    REQUIRE(thermo::round<delta_celsius>(delta_millicelsius(-12000)).count() == -12);

    // Zero
    REQUIRE(thermo::round<delta_celsius>(delta_millicelsius(0)).count() == 0);

    // Same precision should return identical value
    REQUIRE(thermo::round<delta_celsius>(delta_celsius(42)).count() == 42);

    // Finer precision should return lossless conversion
    REQUIRE(thermo::round<delta_millicelsius>(delta_celsius(5)).count() == 5000);
}
