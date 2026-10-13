// Unit tests for thermo library
// Uses Catch2 test framework with compile-time static_asserts

#include <catch2/catch_test_macros.hpp>
#include <thermo/thermo>
#include <type_traits>

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

TEST_CASE("millifahrenheit conversion", "[thermo][temperature]") {
    // 0.001°F precision: 1°F = 1000 counts
    REQUIRE(temperature_cast<millifahrenheit>(fahrenheit(1)).count() == 1000);
    millifahrenheit mf = fahrenheit(1); // finer precision, implicit
    REQUIRE(mf.count() == 1000);
    REQUIRE(temperature_cast<millifahrenheit>(fahrenheit(72)).count() == 72000);

    // Cross-scale: 0°C = 32°F = 32000 millifahrenheit
    REQUIRE(temperature_cast<millifahrenheit>(celsius(0)).count() == 32000);

    // 72.5°F in decifahrenheit -> millifahrenheit
    REQUIRE(millifahrenheit(decifahrenheit(725)).count() == 72500);

    // Deltas
    REQUIRE(delta_millifahrenheit(delta_fahrenheit(1)).count() == 1000);
    REQUIRE(delta_millifahrenheit(9000) == delta_celsius(5));
    REQUIRE(delta_millifahrenheit(9000) == delta_millicelsius(5000));
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
// ASCII delta literals: always defined, same types as the _Δ forms.
static_assert(std::is_same_v<decltype(5_delta_c), delta_celsius>);
static_assert(std::is_same_v<decltype(5000_delta_mc), delta_millicelsius>);
static_assert(std::is_same_v<decltype(5_delta_k), delta_kelvin>);
static_assert(std::is_same_v<decltype(5000_delta_mk), delta_millikelvin>);
static_assert(std::is_same_v<decltype(9_delta_f), delta_fahrenheit>);
static_assert(std::is_same_v<decltype(9000_delta_mf), delta_millifahrenheit>);
static_assert((5_delta_c).count() == 5);
static_assert((5000_delta_mc).count() == 5000);
static_assert((5_delta_k).count() == 5);
static_assert((5000_delta_mk).count() == 5000);
static_assert((9_delta_f).count() == 9);
static_assert((9000_delta_mf).count() == 9000);
static_assert(9_delta_f == delta_celsius(5));
#if THERMO_HAS_UTF8_LITERALS
static_assert((5_Δc).count() == 5);
static_assert((1000_Δmc).count() == 1000);
static_assert((5_Δk).count() == 5);
static_assert((1000_Δmk).count() == 1000);
static_assert((9_Δf).count() == 9);
static_assert((9000_Δmf).count() == 9000);
#endif

// Runtime tests
TEST_CASE("temperature literals", "[thermo][literals]") {
    REQUIRE((20_c).count() == 20);
    REQUIRE((1000_mc).count() == 1000);
    REQUIRE((273_k).count() == 273);
    REQUIRE((273000_mk).count() == 273000);
    REQUIRE((32_f).count() == 32);
}

TEST_CASE("delta literals", "[thermo][literals]") {
    // ASCII spellings, always available.
    REQUIRE((5_delta_c).count() == 5);
    REQUIRE((5000_delta_mc).count() == 5000);
    REQUIRE((5_delta_k).count() == 5);
    REQUIRE((5000_delta_mk).count() == 5000);
    REQUIRE((9_delta_f).count() == 9);
    REQUIRE((9000_delta_mf).count() == 9000);
#if THERMO_HAS_UTF8_LITERALS
    REQUIRE((5_Δc).count() == 5);
    REQUIRE((1000_Δmc).count() == 1000);
    REQUIRE((5_Δk).count() == 5);
    REQUIRE((1000_Δmk).count() == 1000);
    REQUIRE((9_Δf).count() == 9);
    REQUIRE((9000_Δmf).count() == 9000);
    REQUIRE(5_Δc == 5_delta_c);
    REQUIRE(1000_Δmc == 1000_delta_mc);
    REQUIRE(5_Δk == 5_delta_k);
    REQUIRE(1000_Δmk == 1000_delta_mk);
    REQUIRE(9_Δf == 9_delta_f);
    REQUIRE(9000_Δmf == 9000_delta_mf);
    static_assert(std::is_same_v<decltype(5_Δc), decltype(5_delta_c)>);
    static_assert(std::is_same_v<decltype(9000_Δmf), decltype(9000_delta_mf)>);
#endif
}

// =============================================================================
// Negation (enables negative literals, e.g. -123_dc == -(123_dc))
// =============================================================================

template<typename T>
concept has_unary_minus = requires(T t) { -t; };

// Available on relative scales (zero above absolute zero)...
static_assert(has_unary_minus<celsius>);
static_assert(has_unary_minus<decicelsius>);
static_assert(has_unary_minus<fahrenheit>);
// ...but not on the absolute scale, where a negated value is below absolute zero.
static_assert(!has_unary_minus<kelvin>);
static_assert(!has_unary_minus<decikelvin>);
static_assert(!has_unary_minus<millikelvin>);

// Compile-time tests
static_assert((-123_dc).count() == -123);
static_assert((-40_f).count() == -40);
static_assert((-20_c).count() == -20);
static_assert(-(-5_c) == 5_c);

// Runtime tests
TEST_CASE("temperature negation", "[thermo][temperature]") {
    REQUIRE((-123_dc).count() == -123);
    REQUIRE((-40_f).count() == -40);
    REQUIRE((-20_c).count() == -20);
    REQUIRE((-(-5_c)).count() == 5);
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
    REQUIRE(to_string(decicelsius(200)) == "200d°C");
    REQUIRE(to_string(decikelvin(2955)) == "2955dK");
    REQUIRE(to_string(decifahrenheit(725)) == "725d°F");
    REQUIRE(to_string(millifahrenheit(72500)) == "72500m°F");
    REQUIRE(to_string(delta_celsius(5)) == "5Δ°C");
    REQUIRE(to_string(delta_millifahrenheit(36000)) == "36000Δm°F");
    REQUIRE(to_string(millicelsius(-1250)) == "-1250m°C");
}

TEST_CASE("to_string floating-point counts", "[thermo][string][real]") {
    using celsius_d = temperature<celsius_scale, delta<double>>;
    using delta_millicelsius_d = delta<double, std::milli>;
    // Shortest round-trip fixed notation, never std::to_string's six decimals.
    REQUIRE(to_string(celsius_d(22.5)) == "22.5°C");
    REQUIRE(to_string(celsius_d(1000000.0)) == "1000000°C");
    REQUIRE(to_string(celsius_d(-0.125)) == "-0.125°C");
    REQUIRE(to_string(delta_millicelsius_d(1500.25)) == "1500.25Δm°C");
}

TEST_CASE("to_string base_unit", "[thermo][string][marker]") {
    REQUIRE(to_string(millicelsius(22534), base_unit) == "22.534°C");
    REQUIRE(to_string(celsius(22), base_unit) == "22°C");
    REQUIRE(to_string(decikelvin(2955), base_unit) == "295.5K");
    REQUIRE(to_string(millikelvin(24), base_unit) == "0.024K");
    REQUIRE(to_string(millifahrenheit(-40250), base_unit) == "-40.25°F");
    REQUIRE(to_string(decifahrenheit(725), base_unit) == "72.5°F");
    REQUIRE(to_string(millicelsius(0), base_unit) == "0°C");
    REQUIRE(to_string(delta_millicelsius(1500), base_unit) == "1.5Δ°C");
    REQUIRE(to_string(delta_millikelvin(-1500), base_unit) == "-1.5Δ°C");
    REQUIRE(to_string(delta_millifahrenheit(9000), base_unit) == "9Δ°F");
}

TEST_CASE("to_string scaled", "[thermo][string][marker]") {
    // Temperatures are not rescaled: the scaled form is the type's own unit.
    REQUIRE(to_string(millicelsius(22534), scaled) == "22534m°C");
    REQUIRE(to_string(celsius(22), scaled) == "22°C");
    REQUIRE(to_string(decikelvin(2955), scaled) == "2955dK");
    REQUIRE(to_string(millifahrenheit(-40250), scaled) == "-40250m°F");
    REQUIRE(to_string(millicelsius(0), scaled) == "0m°C");
    REQUIRE(to_string(delta_millicelsius(1500), scaled) == "1500Δm°C");
    REQUIRE(to_string(delta_decifahrenheit(-95), scaled) == "-95Δd°F");
}

#if CONFIG_THERMO_STD_FORMAT
TEST_CASE("std::format delta", "[thermo][string][delta]") {
    REQUIRE(std::format("{}", delta_celsius(20)) == "20Δ°C");
    REQUIRE(std::format("{}", delta_decicelsius(200)) == "200Δd°C");
    REQUIRE(std::format("{}", delta_millicelsius(20000)) == "20000Δm°C");
    REQUIRE(std::format("{}", delta_fahrenheit(36)) == "36Δ°F");
    REQUIRE(std::format("{}", delta_decifahrenheit(360)) == "360Δd°F");
    REQUIRE(std::format("{}", delta_millifahrenheit(36000)) == "36000Δm°F");
}

TEST_CASE("std::format temperature", "[thermo][string][temperature]") {
    // Empty spec prints the exact stored count with a precision-qualified unit.
    REQUIRE(std::format("{}", celsius(20)) == "20°C");
    REQUIRE(std::format("{}", decicelsius(200)) == "200d°C");
    REQUIRE(std::format("{}", millicelsius(22500)) == "22500m°C");
    REQUIRE(std::format("{}", kelvin(273)) == "273K");
    REQUIRE(std::format("{}", decikelvin(2955)) == "2955dK");
    REQUIRE(std::format("{}", millikelvin(273150)) == "273150mK");
    REQUIRE(std::format("{}", fahrenheit(72)) == "72°F");
    REQUIRE(std::format("{}", decifahrenheit(725)) == "725d°F");
    REQUIRE(std::format("{}", millifahrenheit(72500)) == "72500m°F");
}

TEST_CASE("std::format with spec renders in the type's own unit", "[thermo][string]") {
    // A spec changes only the number formatting, never the unit: the count is
    // rendered as a double with the same unit as "{}".
    REQUIRE(std::format("{:.1f}", millicelsius(22534)) == "22534.0m°C");
    REQUIRE(std::format("{:g}", millicelsius(22500)) == "22500m°C");
    REQUIRE(std::format("{:.2f}", decicelsius(225)) == "225.00d°C");
    REQUIRE(std::format("{:.1f}", celsius(22)) == "22.0°C");
    REQUIRE(std::format("{:.2f}", millikelvin(295650)) == "295650.00mK");
    REQUIRE(std::format("{:.1f}", decikelvin(2955)) == "2955.0dK");
    REQUIRE(std::format("{:.1f}", fahrenheit(72)) == "72.0°F");
    REQUIRE(std::format("{:.1f}", decifahrenheit(725)) == "725.0d°F");
    REQUIRE(std::format("{:.2f}", millifahrenheit(72530)) == "72530.00m°F");
    REQUIRE(std::format("{:.1f}", millicelsius(-1250)) == "-1250.0m°C");

    // Width and alignment pass through to the numeric part.
    REQUIRE(std::format("{:9.1f}", millicelsius(22500)) == "  22500.0m°C");
    REQUIRE(std::format("{:*<8}", celsius(22)) == "22******°C");

    // Deltas render the same way, keeping the Δ marker.
    REQUIRE(std::format("{:.1f}", delta_millicelsius(1500)) == "1500.0Δm°C");
    REQUIRE(std::format("{:.1f}", delta_decicelsius(15)) == "15.0Δd°C");
    REQUIRE(std::format("{:.1f}", delta_millifahrenheit(9000)) == "9000.0Δm°F");
    REQUIRE(std::format("{:.1f}", delta_fahrenheit(36)) == "36.0Δ°F");
}

TEST_CASE("std::format base-unit marker", "[thermo][string][marker]") {
    // 'b' renders in the unprefixed degree of the same scale (no scale change).
    REQUIRE(std::format("{:b.1f}", millicelsius(22534)) == "22.5°C");
    REQUIRE(std::format("{:b.2f}", decicelsius(225)) == "22.50°C");
    REQUIRE(std::format("{:b.1f}", celsius(22)) == "22.0°C");
    REQUIRE(std::format("{:b.2f}", millikelvin(295650)) == "295.65K");
    REQUIRE(std::format("{:b.1f}", decikelvin(2955)) == "295.5K");
    REQUIRE(std::format("{:b.1f}", decifahrenheit(725)) == "72.5°F");
    REQUIRE(std::format("{:b.2f}", millifahrenheit(72530)) == "72.53°F");
    REQUIRE(std::format("{:b.1f}", delta_millicelsius(1500)) == "1.5Δ°C");
    REQUIRE(std::format("{:b.1f}", delta_millifahrenheit(9000)) == "9.0Δ°F");

    // With no trailing spec: shortest round-trip fixed notation.
    REQUIRE(std::format("{:b}", millicelsius(22534)) == "22.534°C");
    REQUIRE(std::format("{:b}", celsius(22)) == "22°C");
    REQUIRE(std::format("{:b}", millikelvin(24)) == "0.024K");
    REQUIRE(std::format("{:b}", decifahrenheit(725)) == "72.5°F");
    REQUIRE(std::format("{:b}", delta_millicelsius(1500)) == "1.5Δ°C");

    // Negative values and zero.
    REQUIRE(std::format("{:b}", millicelsius(-1250)) == "-1.25°C");
    REQUIRE(std::format("{:b.1f}", millifahrenheit(-40250)) == "-40.2°F");
    REQUIRE(std::format("{:b}", delta_millicelsius(-1500)) == "-1.5Δ°C");
    REQUIRE(std::format("{:b}", millicelsius(0)) == "0°C");
    REQUIRE(std::format("{:b.1f}", delta_decicelsius(0)) == "0.0Δ°C");

    // Width and fill after the marker apply to the numeric part.
    REQUIRE(std::format("{:b8.2f}", millicelsius(22534)) == "   22.53°C");
    REQUIRE(std::format("{:b*>8.2f}", millicelsius(22534)) == "***22.53°C");
    REQUIRE(std::format("{:b<6}", celsius(22)) == "22    °C");
}

TEST_CASE("std::format scaled marker", "[thermo][string][marker]") {
    // 's' keeps the type's own unit: temperatures are not rescaled.
    REQUIRE(std::format("{:s.1f}", millicelsius(22534)) == "22534.0m°C");
    REQUIRE(std::format("{:s.1f}", decikelvin(2955)) == "2955.0dK");
    REQUIRE(std::format("{:s.1f}", fahrenheit(72)) == "72.0°F");
    REQUIRE(std::format("{:s.1f}", delta_millicelsius(1500)) == "1500.0Δm°C");

    // With no trailing spec: the count as a double in shortest fixed notation.
    REQUIRE(std::format("{:s}", millicelsius(22534)) == "22534m°C");
    REQUIRE(std::format("{:s}", millifahrenheit(72561)) == "72561m°F");
    REQUIRE(std::format("{:s}", delta_millikelvin(1500)) == "1500Δm°C");

    // Negative values and zero.
    REQUIRE(std::format("{:s}", millicelsius(-1250)) == "-1250m°C");
    REQUIRE(std::format("{:s.1f}", delta_decifahrenheit(-95)) == "-95.0Δd°F");
    REQUIRE(std::format("{:s}", millicelsius(0)) == "0m°C");
    REQUIRE(std::format("{:s}", celsius(0)) == "0°C");

    // Width and fill after the marker apply to the numeric part.
    REQUIRE(std::format("{:s>8}", celsius(22)) == "      22°C");
    REQUIRE(std::format("{:s0>8.1f}", millicelsius(22534)) == "022534.0m°C");
}

TEST_CASE("std::format of a precision with no SI prefix", "[thermo][string][marker]") {
    // No unit label of its own: any spec renders in the base unit (as before).
    using centicelsius = temperature<celsius_scale, delta<int64_t, std::centi>>;
    REQUIRE(std::format("{:.2f}", centicelsius(2250)) == "22.50°C");
    REQUIRE(std::format("{:b}", centicelsius(2250)) == "22.5°C");
    REQUIRE(std::format("{:s}", centicelsius(2250)) == "22.5°C");
    REQUIRE(std::format("{:s.1f}", centicelsius(-2250)) == "-22.5°C");
    REQUIRE(to_string(centicelsius(2250), base_unit) == "22.5°C");
    REQUIRE(to_string(centicelsius(2250), scaled) == "22.5°C");
}

TEST_CASE("std::format with no spec agrees with to_string", "[thermo][string]") {
    using celsius_d = temperature<celsius_scale, delta<double>>;
    using millikelvin_d = temperature<kelvin_scale, delta<double, std::milli>>;
    using delta_millicelsius_d = delta<double, std::milli>;

    const auto agree = [](const auto& q) {
        REQUIRE(std::format("{}", q) == to_string(q));
        REQUIRE(std::format("{:b}", q) == to_string(q, base_unit));
        REQUIRE(std::format("{:s}", q) == to_string(q, scaled));
    };
    agree(celsius(22));
    agree(millicelsius(22534));
    agree(millicelsius(-1250));
    agree(decikelvin(2955));
    agree(millifahrenheit(72561));
    agree(delta_millicelsius(1500));
    agree(delta_decifahrenheit(-95));
    agree(celsius_d(22.5));
    agree(celsius_d(1000000.0));
    agree(celsius_d(0.000001));
    agree(millikelvin_d(24.25));
    agree(delta_millicelsius_d(-0.5));

    // Floating-point counts use shortest round-trip fixed notation, never
    // scientific notation.
    REQUIRE(std::format("{}", celsius_d(1000000.0)) == "1000000°C");
    REQUIRE(std::format("{}", celsius_d(0.000001)) == "0.000001°C");
    REQUIRE(std::format("{}", celsius_d(22.5)) == "22.5°C");
    REQUIRE(std::format("{}", millikelvin_d(24.25)) == "24.25mK");
    REQUIRE(std::format("{:b}", millikelvin_d(24.25)) == "0.02425K");
    REQUIRE(std::format("{}", delta_millicelsius_d(-0.5)) == "-0.5Δm°C");

    // An explicit spec is unaffected.
    REQUIRE(std::format("{:.3e}", celsius_d(1000000.0)) == "1.000e+06°C");
    REQUIRE(std::format("{:b.2f}", millikelvin_d(24.25)) == "0.02K");
}

#endif

TEST_CASE("floating-point representation conversion", "[thermo][real]") {
    // Inexact reps accept lossy conversions implicitly; count() reads in
    // precision steps, so with the default precision that is scale degrees.
    using celsius_d = temperature<celsius_scale, delta<double>>;
    REQUIRE(celsius_d(millicelsius(22500)).count() == 22.5); // implicit
    REQUIRE(temperature_cast<celsius_d>(millicelsius(22500)).count() == 22.5);
    REQUIRE(temperature_cast<celsius_d>(decicelsius(225)).count() == 22.5);
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
