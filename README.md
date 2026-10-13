# thermo

A type-safe, header-only C++23 library for temperature handling, modeled after `std::chrono`.

📚 **[Full API Documentation](https://cleishm.github.io/thermo-cpp/)**

## Features

- **Type-safe temperatures** with distinct types for Celsius, Kelvin, and Fahrenheit
- **Configurable precision** (degree, decidegree, millidegree) plus real-valued (`double`) types for display and computation
- **Automatic conversions** between scales and precisions
- **Lossless implicit conversions** (lossy conversions require explicit casts)
- **Temperature deltas** distinct from absolute temperatures
- **User-defined literals** for concise notation
- **`std::format` support** (when available)
- **Fully constexpr** - all operations can be evaluated at compile time

## Requirements

- C++23 compiler (GCC 13+, Clang 17+, MSVC 19.36+)
- **MSVC users:** The CMake target adds `/utf-8` and `/Zc:__cplusplus` automatically. Use `/std:c++latest` to enable C++23 features. Projects that do not use the CMake target (e.g. a plain include path) must pass these flags themselves.
- **GCC 13:** GCC before 14 cannot lex non-ASCII user-defined literal suffixes,
  so the `_Δ…` delta literals (`5_Δc`, `5000_Δmc`, …) are unavailable there
  (`THERMO_HAS_UTF8_LITERALS` is `0`). Every `_Δ…` literal has an ASCII
  alternative spelled `_delta_…` (`5_delta_c`, `5000_delta_mc`, …), which is
  always available.

## Installation

### CMake FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(
    thermo
    GIT_REPOSITORY https://github.com/cleishm/thermo-cpp.git
    GIT_TAG v2.2.1
)
FetchContent_MakeAvailable(thermo)

target_link_libraries(your_target PRIVATE thermo::thermo)
```

### vcpkg

```bash
vcpkg install cleishm-thermo-cpp
```

The `cleishm-thermo-cpp` port lives in the upstream
[microsoft/vcpkg](https://github.com/microsoft/vcpkg) registry and is
community-maintained, so it may lag the latest release.

```cmake
find_package(thermo CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE thermo::thermo)
```

### Manual

Copy the `include/thermo/` directory to your project.

## Usage

```cpp
#include <thermo/thermo>  // or <thermo/thermo.hpp>

using namespace thermo;
using namespace thermo_literals;

// Absolute temperatures
celsius room_temp{20};
kelvin absolute = room_temp;  // implicit conversion to higher precision
fahrenheit f = fahrenheit(room_temp);  // explicit cross-scale conversion

// Using literals
auto boiling = 100_c;
auto freezing = 32_f;
auto absolute_zero = 0_k;

// Precision control
millicelsius precise{25500};  // 25.5°C
celsius coarse = celsius(precise);  // explicit lossy conversion (truncates to 25°C)
millicelsius back = coarse;  // implicit lossless conversion (25000 m°C)

// Temperature deltas (differences)
auto delta = 5_Δc;  // 5 degree change
auto new_temp = room_temp + delta;  // 25°C

// Computing differences
delta_celsius diff = difference(boiling, room_temp);  // 80°C difference

// Comparisons work across precisions
if (millicelsius{20000} == celsius{20}) {
    // true - same temperature, different precision
}

// String conversion
std::string s = to_string(room_temp);  // "20°C"

// Formatting: always in the type's own unit, unless a marker opts out.
millicelsius reading{22534};
std::string raw  = std::format("{}", reading);      // "22534m°C" (exact stored value)
std::string spec = std::format("{:.1f}", reading);  // "22534.0m°C" (same unit)
std::string disp = std::format("{:b.1f}", reading); // "22.5°C" (base unit)
```

## Temperature Types

### Absolute Temperatures

| Type | Scale | Precision |
|------|-------|-----------|
| `celsius` | Celsius | 1°C |
| `decicelsius` | Celsius | 0.1°C |
| `millicelsius` | Celsius | 0.001°C |
| `kelvin` | Kelvin | 1 K |
| `decikelvin` | Kelvin | 0.1 K |
| `millikelvin` | Kelvin | 0.001 K |
| `fahrenheit` | Fahrenheit | 1°F |
| `decifahrenheit` | Fahrenheit | 0.1°F |
| `millifahrenheit` | Fahrenheit | 0.001°F |

See Formatting below for how these render as strings.

### Temperature Deltas

| Type | Precision |
|------|-----------|
| `delta_celsius` / `delta_kelvin` | 1° |
| `delta_decicelsius` / `delta_decikelvin` | 0.1° |
| `delta_millicelsius` / `delta_millikelvin` | 0.001° |
| `delta_fahrenheit` | 1°F (= 5/9°C) |
| `delta_decifahrenheit` | 0.1°F |
| `delta_millifahrenheit` | 0.001°F |

### Literals

```cpp
using namespace thermo_literals;

// Absolute temperatures
20_c      // celsius(20)
200_dc    // decicelsius(200) = 20.0°C
20000_mc  // millicelsius(20000) = 20.000°C
293_k     // kelvin(293)
68_f      // fahrenheit(68)

// Negative absolute temperatures (unary minus on the literal)
-123_dc   // decicelsius(-123) = -12.3°C
-40_f     // fahrenheit(-40)
// -25_k  // ill-formed: negating a Kelvin temperature falls below absolute zero

// Temperature deltas
5_Δc      // delta_celsius(5)
5000_Δmc  // delta_millicelsius(5000)
9_Δf      // delta_fahrenheit(9) = delta_celsius(5)

// ASCII spellings of the delta literals (always available, including on GCC 13)
5_delta_c      // delta_celsius(5), same as 5_Δc
5000_delta_mc  // delta_millicelsius(5000), same as 5000_Δmc
5_delta_k      // delta_kelvin(5), same as 5_Δk
5000_delta_mk  // delta_millikelvin(5000), same as 5000_Δmk
9_delta_f      // delta_fahrenheit(9), same as 9_Δf
9000_delta_mf  // delta_millifahrenheit(9000), same as 9000_Δmf
```

The `_Δ…` literals require `THERMO_HAS_UTF8_LITERALS` (GCC 14+, Clang, MSVC);
the `_delta_…` forms are their ASCII alternatives and are always defined.

Unary minus is available on relative scales (Celsius, Fahrenheit) so that
negative literals like `-123_dc` work — the sign is applied to the value the
literal produces, i.e. `-(123_dc)`. It is deliberately *not* available on the
absolute Kelvin scale, where a negated temperature would fall below absolute
zero; `-25_k` is a compile error. Deltas are negatable on every scale.

## Formatting

`to_string()` and `std::format` render a temperature or delta in **its own
unit** — the scale's degree with the precision's SI prefix, and a `Δ` marker
for deltas. `{}` prints the exact stored count; any other format spec changes
only how the number is rendered (as a `double`), never the unit:

```cpp
std::format("{}", millicelsius(22534));          // "22534m°C"
std::format("{:.1f}", millicelsius(22534));      // "22534.0m°C"
std::format("{:>9.1f}", decikelvin(2955));       // "   2955.0dK"
std::format("{:.1f}", delta_millicelsius(1500)); // "1500.0Δm°C"
to_string(millicelsius(22534));                  // "22534m°C"
```

A single leading **marker** letter in the spec opts into another unit; the rest
of the spec is a normal floating-point format spec:

| Marker | Unit | Example |
|--------|------|---------|
| `b` | Base unit: the unprefixed degree of the same scale (°C, K or °F; never converted to another scale) | `{:b.1f}` of `millicelsius(22534)` → `22.5°C` |
| `s` | Scaled: temperatures are not rescaled, so this is the type's own unit | `{:s.1f}` of `millicelsius(22534)` → `22534.0m°C` |

With nothing after the marker (`{:b}`, `{:s}`), the value is rendered in the
shortest fixed notation that round-trips — never scientific notation. The same
forms are available without `<format>` through tag arguments to `to_string()`:

```cpp
std::format("{:b}", millicelsius(22534));             // "22.534°C"
std::format("{:b.1f}", delta_millifahrenheit(9000));  // "9.0Δ°F"
std::format("{:b*>8.2f}", millicelsius(22534));       // "***22.53°C"
to_string(millicelsius(22534), base_unit);            // "22.534°C"
to_string(millicelsius(22534), scaled);               // "22534m°C"
```

Because a spec starting with `b` or `s` is always a marker, those letters cannot
be used as the fill character at the start of a spec (`{:b>8}` is the `b` marker
followed by `>8`).

Floating-point representations (e.g. `temperature<celsius_scale, delta<double>>`)
render with no spec in the same shortest round-trip fixed notation, so
`to_string` and `{}` agree: `22.5°C`, `1000000°C`.

Temperature precisions without an SI prefix (e.g.
`temperature<celsius_scale, delta<int64_t, std::centi>>`) have no unit label of
their own: `{}` is a format error, and any other spec or marker renders
the value in the base unit.

## Conversion Rules

Conversions follow the same philosophy as `std::chrono`:

- **Implicit conversions** are allowed when lossless (e.g., `celsius` → `millicelsius`)
- **Explicit conversions** are required when lossy (e.g., `millicelsius` → `celsius`)
- **Cross-scale conversions** consider both precision and scale offset

```cpp
// Implicit (lossless)
millicelsius mc = celsius{20};       // OK: 20°C → 20000 m°C
millikelvin mk = millicelsius{0};    // OK: 0 m°C → 273150 mK

// Explicit required (lossy)
celsius c = celsius(millicelsius{1500});  // 1500 m°C → 1°C (truncates)
kelvin k = kelvin(celsius{0});            // 0°C → 273 K (truncates 273.15)

// Use temperature_cast for explicit conversions
auto k2 = temperature_cast<kelvin>(celsius{100});
```

## Building Tests

```bash
cmake -B build -DTHERMO_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build
```

### Building with MSVC

The CMake target adds `/utf-8` and `/Zc:__cplusplus` automatically for MSVC. Pass `/std:c++latest` yourself, and add the other flags if you do not use the CMake target:

```bash
cmake -B build -DTHERMO_BUILD_TESTS=ON -DCMAKE_CXX_FLAGS="/std:c++latest /EHsc"
cmake --build build --config Release
ctest --test-dir build -C Release
```

## License

MIT License - see [LICENSE](LICENSE) for details.
