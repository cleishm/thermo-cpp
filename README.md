# thermo

A type-safe, header-only C++23 library for temperature handling, modeled after `std::chrono`.

## Features

- **Type-safe temperatures** with distinct types for Celsius, Kelvin, and Fahrenheit
- **Configurable precision** (degree, decidegree, millidegree)
- **Automatic conversions** between scales and precisions
- **Lossless implicit conversions** (lossy conversions require explicit casts)
- **Temperature deltas** distinct from absolute temperatures
- **User-defined literals** for concise notation
- **`std::format` support** (when available)
- **Fully constexpr** - all operations can be evaluated at compile time

## Requirements

- C++23 compiler (GCC 13+, Clang 17+, MSVC 19.36+)
- **MSVC users:** The following compiler flags are required:
  - `/std:c++latest` - Enable C++23 features
  - `/Zc:__cplusplus` - Set `__cplusplus` macro correctly
  - `/utf-8` - Treat source files as UTF-8 (for degree symbols)

## Installation

### CMake FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(
    thermo
    GIT_REPOSITORY https://github.com/cleishm/thermo-cpp.git
    GIT_TAG v1.0.0
)
FetchContent_MakeAvailable(thermo)

target_link_libraries(your_target PRIVATE thermo::thermo)
```

### vcpkg

```bash
vcpkg install thermo
```

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

// Temperature deltas
5_Δc      // delta_celsius(5)
5000_Δmc  // delta_millicelsius(5000)
9_Δf      // delta_fahrenheit(9) = delta_celsius(5)
```

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

When using MSVC, you must pass the required compiler flags:

```bash
cmake -B build -DTHERMO_BUILD_TESTS=ON -DCMAKE_CXX_FLAGS="/std:c++latest /Zc:__cplusplus /utf-8 /EHsc"
cmake --build build --config Release
ctest --test-dir build -C Release
```

## License

MIT License - see [LICENSE](LICENSE) for details.
