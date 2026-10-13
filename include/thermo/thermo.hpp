#pragma once

/**
 * @file thermo.hpp
 * @brief Temperature types and arithmetic, modeled after std::chrono.
 */

#include <charconv>
#include <compare>
#include <concepts>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <ratio>
#include <string>
#include <system_error>
#include <version> // pulls in __cpp_lib_format so the detection below works in non-IDF builds
#ifndef CONFIG_THERMO_STD_FORMAT
#if __has_include(<format>) && defined(__cpp_lib_format)
#define CONFIG_THERMO_STD_FORMAT 1
#else
#define CONFIG_THERMO_STD_FORMAT 0
#endif
#endif

#if CONFIG_THERMO_STD_FORMAT
#include <format>
#endif
#include <assert.h>

// GCC 13 reports 202100L for -std=c++23, so accept that as C++23.
static_assert(__cplusplus >= 202100L, "thermo requires C++23 or later");

/**
 * @brief 1 if the compiler accepts non-ASCII literal suffixes (e.g. `_Δc`).
 *
 * GCC before 14 (including the ESP-IDF toolchains before GCC 14) cannot lex
 * extended characters in user-defined literal suffixes, so the `_Δ...` delta
 * literals are omitted there. Every `_Δ...` literal has an ASCII alternative
 * spelled `_delta_...` (`_delta_c`, `_delta_mc`, `_delta_k`, `_delta_mk`,
 * `_delta_f`, `_delta_mf`), which is always available.
 */
#if !defined(__GNUC__) || defined(__clang__) || __GNUC__ >= 14
#define THERMO_HAS_UTF8_LITERALS 1
#else
#define THERMO_HAS_UTF8_LITERALS 0
#endif

/**
 * @brief Temperature types and utilities.
 *
 * This library provides type-safe temperature handling with support for
 * multiple scales (Celsius, Kelvin, Fahrenheit) and precisions, following
 * the design of std::chrono. It distinguishes between absolute temperatures
 * and temperature differences (deltas).
 *
 * ## Formatting
 *
 * to_string() and std::format show a temperature or delta in its own unit by
 * default, whatever the format spec: `{}` prints the exact count and `{:.1f}`
 * the count as a double, each followed by the precision-qualified unit. A
 * single leading marker letter in the spec, or a tag argument to to_string(),
 * opts into a different unit:
 *
 * @code
 * std::format("{}", millicelsius(22534));        // "22534m°C"  own unit
 * std::format("{:.1f}", millicelsius(22534));    // "22534.0m°C"
 * std::format("{:b}", millicelsius(22534));      // "22.534°C"  base unit
 * std::format("{:b.1f}", millicelsius(22534));   // "22.5°C"
 * std::format("{:b.1f}", delta_millicelsius(1500)); // "1.5Δ°C"
 * to_string(millicelsius(22534), base_unit);     // "22.534°C"
 * @endcode
 *
 * The base unit is the unprefixed degree of the same scale (m°C -> °C,
 * mK -> K, m°F -> °F); temperatures are never converted to another scale.
 * The scaled form (`s`, see scaled) keeps the type's own unit: temperatures
 * are not rescaled to suit their magnitude.
 */
namespace thermo {

template<typename Rep, typename Precision = std::ratio<1>>
class delta;

/**
 * @brief Trait to detect delta specializations.
 * @tparam _T Type to check.
 */
template<typename _T>
struct is_delta : std::false_type {};

template<typename Rep, typename Precision>
struct is_delta<delta<Rep, Precision>> : std::true_type {};

/** @cond INTERNAL */
template<typename T, template<typename...> class Template>
struct _is_specialization_of : std::false_type {};

template<template<typename...> class Template, typename... Args>
struct _is_specialization_of<Template<Args...>, Template> : std::true_type {};

template<typename T, template<typename...> class Template>
inline constexpr bool _is_specialization_of_v = _is_specialization_of<T, Template>::value;

template<typename Rep>
concept not_delta = !_is_specialization_of_v<Rep, delta>;

template<typename Rep>
struct delta_values {
    /** @brief Returns a zero-length delta. */
    static constexpr Rep zero() noexcept { return Rep(0); }

    /** @brief Returns the maximum representable delta. */
    static constexpr Rep max() noexcept { return std::numeric_limits<Rep>::max(); }

    /** @brief Returns the minimum (most negative) representable delta. */
    static constexpr Rep min() noexcept { return std::numeric_limits<Rep>::lowest(); }
};

template<typename T>
struct _is_ratio : std::false_type {};

template<std::intmax_t Num, std::intmax_t Denom>
struct _is_ratio<std::ratio<Num, Denom>> : std::true_type {};

template<typename T>
struct treat_as_inexact : std::bool_constant<std::floating_point<T>> {};

template<typename T>
inline constexpr bool treat_as_inexact_v = treat_as_inexact<T>::value;

consteval intmax_t _gcd(intmax_t m, intmax_t n) noexcept {
    while (n != 0) {
        intmax_t rem = m % n;
        m = n;
        n = rem;
    }
    return m;
}

template<typename R1, typename R2>
inline constexpr intmax_t _safe_ratio_divide_den = [] {
    constexpr intmax_t g1 = _gcd(R1::num, R2::num);
    constexpr intmax_t g2 = _gcd(R1::den, R2::den);
    return (R1::den / g2) * (R2::num / g1);
}();

template<typename From, typename To>
concept _harmonic_precision = _safe_ratio_divide_den<From, To> == 1;
/** @endcond */

/**
 * @brief A temperature difference with a representation and precision.
 *
 * Similar to std::chrono::duration, delta represents the difference between
 * two temperatures. The precision is expressed as a std::ratio of one base
 * unit (Kelvin or Celsius degree).
 *
 * @tparam Rep       Arithmetic type for the tick count.
 * @tparam Precision A std::ratio representing the precision (default: 1).
 */
template<typename Rep, typename Precision>
class delta {
    static_assert(!is_delta<Rep>::value, "rep cannot be a thermo::delta");
    static_assert(_is_ratio<Precision>::value, "precision must be a specialization of std::ratio");
    static_assert(Precision::num > 0, "precision must be positive");

public:
    /** @brief The representation type. */
    using rep = Rep;
    /** @brief The precision as a std::ratio. */
    using precision = typename Precision::type;

    /** @brief Constructs a zero delta. */
    constexpr delta() = default;
    delta(const delta&) = default;

    /**
     * @brief Constructs from a tick count.
     *
     * Implicit for inexact types, explicit otherwise to prevent accidental
     * precision loss.
     *
     * @tparam Rep2 The source representation type.
     * @param r     The tick count.
     */
    template<typename Rep2>
        requires std::convertible_to<const Rep2&, rep> && (treat_as_inexact_v<rep> || !treat_as_inexact_v<Rep2>)
    constexpr explicit delta(const Rep2& r)
        : _r(static_cast<rep>(r)) {}

    /**
     * @brief Constructs from another delta with different representation or precision.
     *
     * Implicit when the conversion is lossless (target precision evenly
     * divides source precision). Explicit otherwise.
     *
     * @tparam Rep2       Source representation type.
     * @tparam Precision2 Source precision.
     * @param temp        The source delta.
     */
    template<typename Rep2, typename Precision2>
        requires std::convertible_to<const Rep2&, rep> &&
        (treat_as_inexact_v<rep> || (_harmonic_precision<Precision2, precision> && !treat_as_inexact_v<Rep2>))
    constexpr delta(const delta<Rep2, Precision2>& temp)
        : _r(delta_cast<delta>(temp).count()) {}

    ~delta() = default;
    delta& operator=(const delta&) = default;

    /** @brief Returns the tick count. */
    constexpr rep count() const { return _r; }

    constexpr delta<typename std::common_type<rep>::type, precision> operator+() const {
        return delta<typename std::common_type<rep>::type, precision>(_r);
    }

    constexpr delta<typename std::common_type<rep>::type, precision> operator-() const {
        return delta<typename std::common_type<rep>::type, precision>(-_r);
    }

    constexpr delta& operator++() {
        ++_r;
        return *this;
    }

    constexpr delta operator++(int) { return delta(_r++); }

    constexpr delta& operator--() {
        --_r;
        return *this;
    }

    constexpr delta operator--(int) { return delta(_r--); }

    constexpr delta& operator+=(const delta& d) {
        _r += d.count();
        return *this;
    }

    constexpr delta& operator-=(const delta& d) {
        _r -= d.count();
        return *this;
    }

    constexpr delta& operator*=(const rep& r) {
        _r *= r;
        return *this;
    }

    constexpr delta& operator/=(const rep& r) {
        _r /= r;
        return *this;
    }

    constexpr delta& operator%=(const rep& r)
        requires(!treat_as_inexact_v<rep>)
    {
        _r %= r;
        return *this;
    }

    constexpr delta& operator%=(const delta& d)
        requires(!treat_as_inexact_v<rep>)
    {
        _r %= d.count();
        return *this;
    }

    /** @brief Returns a zero-length delta. */
    static constexpr delta zero() noexcept { return delta(delta_values<rep>::zero()); }

    /** @brief Returns the minimum (most negative) representable delta. */
    static constexpr delta min() noexcept { return delta(delta_values<rep>::min()); }

    /** @brief Returns the maximum representable delta. */
    static constexpr delta max() noexcept { return delta(delta_values<rep>::max()); }

private:
    rep _r{};
};

/**
 * @brief Converts a delta to a different precision or representation.
 *
 * @tparam ToDelta The target delta type.
 * @tparam Rep     Source representation type.
 * @tparam Precision Source precision.
 * @param d        The delta to convert.
 *
 * @return The converted delta.
 */
template<typename ToDelta, typename Rep, typename Precision>
constexpr ToDelta delta_cast(const delta<Rep, Precision>& d) {
    if constexpr (std::is_same_v<ToDelta, delta<Rep, Precision>>) {
        return d;
    } else {
        using to_rep = typename ToDelta::rep;
        using to_precision = typename ToDelta::precision;
        using cf = std::ratio_divide<Precision, to_precision>;
        using cr = std::common_type_t<to_rep, Rep, intmax_t>;

        if constexpr (cf::den == 1 && cf::num == 1) {
            return ToDelta(static_cast<to_rep>(d.count()));
        } else if constexpr (cf::den == 1) {
            return ToDelta(static_cast<to_rep>(static_cast<cr>(d.count()) * static_cast<cr>(cf::num)));
        } else if constexpr (cf::num == 1) {
            return ToDelta(static_cast<to_rep>(static_cast<cr>(d.count()) / static_cast<cr>(cf::den)));
        } else {
            return ToDelta(
                static_cast<to_rep>(static_cast<cr>(d.count()) * static_cast<cr>(cf::num) / static_cast<cr>(cf::den))
            );
        }
    }
}

/**
 * @brief Rounds a delta up to the nearest representable value in the target precision.
 *
 * Returns the smallest value of type ToDelta that is greater than or equal to d.
 * When converting to the same or finer precision, returns the exact conversion.
 *
 * @tparam ToDelta Target delta type.
 * @tparam Rep     Source representation type.
 * @tparam Precision Source precision.
 * @param d        The delta to round.
 * @return The rounded delta.
 */
template<typename ToDelta, typename Rep, typename Precision>
constexpr ToDelta ceil(const delta<Rep, Precision>& d) {
    ToDelta result = delta_cast<ToDelta>(d);
    if (result < d) {
        return ToDelta(result.count() + 1);
    }
    return result;
}

/**
 * @brief Rounds a delta down to the nearest representable value in the target precision.
 *
 * Returns the largest value of type ToDelta that is less than or equal to d.
 * When converting to the same or finer precision, returns the exact conversion.
 *
 * @tparam ToDelta Target delta type.
 * @tparam Rep     Source representation type.
 * @tparam Precision Source precision.
 * @param d        The delta to round.
 * @return The rounded delta.
 */
template<typename ToDelta, typename Rep, typename Precision>
constexpr ToDelta floor(const delta<Rep, Precision>& d) {
    ToDelta result = delta_cast<ToDelta>(d);
    if (result > d) {
        return ToDelta(result.count() - 1);
    }
    return result;
}

/**
 * @brief Rounds a delta toward zero to the nearest representable value in the target precision.
 *
 * Returns the value of type ToDelta that is closest to zero.
 * When converting to the same or finer precision, returns the exact conversion.
 *
 * @tparam ToDelta Target delta type.
 * @tparam Rep     Source representation type.
 * @tparam Precision Source precision.
 * @param d        The delta to round.
 * @return The rounded delta.
 */
template<typename ToDelta, typename Rep, typename Precision>
constexpr ToDelta trunc(const delta<Rep, Precision>& d) {
    return delta_cast<ToDelta>(d);
}

/**
 * @brief Rounds a delta to the nearest representable value in the target precision.
 *
 * Returns the value of type ToDelta that is nearest to d. When exactly halfway
 * between two values, rounds away from zero.
 * When converting to the same or finer precision, returns the exact conversion.
 *
 * @tparam ToDelta Target delta type.
 * @tparam Rep     Source representation type.
 * @tparam Precision Source precision.
 * @param d        The delta to round.
 * @return The rounded delta.
 */
template<typename ToDelta, typename Rep, typename Precision>
constexpr ToDelta round(const delta<Rep, Precision>& d) {
    ToDelta result = delta_cast<ToDelta>(d);
    if (result == d) {
        return result;
    }

    // Calculate the midpoint between result and the next value
    ToDelta next = (result < d) ? ToDelta(result.count() + 1) : ToDelta(result.count() - 1);

    // Convert both to source type for comparison
    auto result_in_source = delta_cast<delta<Rep, Precision>>(result);
    auto next_in_source = delta_cast<delta<Rep, Precision>>(next);

    // Calculate distances
    auto dist_to_result = (d > result_in_source) ? d - result_in_source : result_in_source - d;
    auto dist_to_next = (d > next_in_source) ? d - next_in_source : next_in_source - d;

    // If closer to next, or exactly halfway and away from zero, use next
    if (dist_to_next < dist_to_result) {
        return next;
    } else if (dist_to_next == dist_to_result) {
        // Tie: round away from zero
        if (d.count() >= 0) {
            return (next > result) ? next : result;
        } else {
            return (next < result) ? next : result;
        }
    }

    return result;
}

/** @brief Returns the sum of two deltas. */
template<typename Rep1, typename Precision1, typename Rep2, typename Precision2>
constexpr auto operator+(const delta<Rep1, Precision1>& lhs, const delta<Rep2, Precision2>& rhs)
    -> std::common_type_t<delta<Rep1, Precision1>, delta<Rep2, Precision2>> {
    using cd = std::common_type_t<delta<Rep1, Precision1>, delta<Rep2, Precision2>>;
    return cd(cd(lhs).count() + cd(rhs).count());
}

/** @brief Returns the difference of two deltas. */
template<typename Rep1, typename Precision1, typename Rep2, typename Precision2>
constexpr auto operator-(const delta<Rep1, Precision1>& lhs, const delta<Rep2, Precision2>& rhs)
    -> std::common_type_t<delta<Rep1, Precision1>, delta<Rep2, Precision2>> {
    using cd = std::common_type_t<delta<Rep1, Precision1>, delta<Rep2, Precision2>>;
    return cd(cd(lhs).count() - cd(rhs).count());
}

/** @brief Multiplies a delta by a scalar. */
template<typename Rep1, typename Precision, typename Rep2>
    requires not_delta<Rep2> && std::convertible_to<const Rep2&, std::common_type_t<Rep1, Rep2>>
constexpr auto operator*(const delta<Rep1, Precision>& d, const Rep2& r)
    -> delta<std::common_type_t<Rep1, Rep2>, Precision> {
    using cd = delta<std::common_type_t<Rep1, Rep2>, Precision>;
    return cd(cd(d).count() * r);
}

/** @brief Multiplies a scalar by a delta. */
template<typename Rep1, typename Rep2, typename Precision>
    requires not_delta<Rep1> && std::convertible_to<const Rep1&, std::common_type_t<Rep1, Rep2>>
constexpr auto operator*(const Rep1& r, const delta<Rep2, Precision>& d)
    -> delta<std::common_type_t<Rep1, Rep2>, Precision> {
    return d * r;
}

/** @brief Divides a delta by a scalar. */
template<typename Rep1, typename Precision, typename Rep2>
    requires not_delta<Rep2> && std::convertible_to<const Rep2&, std::common_type_t<Rep1, Rep2>>
constexpr auto operator/(const delta<Rep1, Precision>& d, const Rep2& s)
    -> delta<std::common_type_t<Rep1, Rep2>, Precision> {
    using cd = delta<std::common_type_t<Rep1, Rep2>, Precision>;
    return cd(cd(d).count() / s);
}

/** @brief Divides two deltas, returning a scalar. */
template<typename Rep1, typename Precision1, typename Rep2, typename Precision2>
constexpr auto operator/(const delta<Rep1, Precision1>& lhs, const delta<Rep2, Precision2>& rhs)
    -> std::common_type_t<Rep1, Rep2> {
    using cd = std::common_type_t<delta<Rep1, Precision1>, delta<Rep2, Precision2>>;
    return cd(lhs).count() / cd(rhs).count();
}

/** @brief Returns the remainder of dividing a delta by a scalar. */
template<typename Rep1, typename Precision, typename Rep2>
    requires not_delta<Rep2> && std::convertible_to<const Rep2&, std::common_type_t<Rep1, Rep2>> &&
    (!treat_as_inexact_v<Rep1> && !treat_as_inexact_v<Rep2>)
constexpr auto operator%(const delta<Rep1, Precision>& d, const Rep2& s)
    -> delta<std::common_type_t<Rep1, Rep2>, Precision> {
    using cd = delta<std::common_type_t<Rep1, Rep2>, Precision>;
    return cd(cd(d).count() % s);
}

/** @brief Returns the remainder of dividing two deltas. */
template<typename Rep1, typename Precision1, typename Rep2, typename Precision2>
    requires(!treat_as_inexact_v<Rep1> && !treat_as_inexact_v<Rep2>)
constexpr auto operator%(const delta<Rep1, Precision1>& lhs, const delta<Rep2, Precision2>& rhs)
    -> std::common_type_t<delta<Rep1, Precision1>, delta<Rep2, Precision2>> {
    using cd = std::common_type_t<delta<Rep1, Precision1>, delta<Rep2, Precision2>>;
    return cd(cd(lhs).count() % cd(rhs).count());
}

template<typename Rep1, typename Precision1, typename Rep2, typename Precision2>
constexpr bool operator==(const delta<Rep1, Precision1>& lhs, const delta<Rep2, Precision2>& rhs) {
    using ct = std::common_type_t<delta<Rep1, Precision1>, delta<Rep2, Precision2>>;
    return ct(lhs).count() == ct(rhs).count();
}

template<typename Rep1, typename Precision1, typename Rep2, typename Precision2>
    requires std::three_way_comparable<std::common_type_t<Rep1, Rep2>>
constexpr auto operator<=>(const delta<Rep1, Precision1>& lhs, const delta<Rep2, Precision2>& rhs) {
    using ct = std::common_type_t<delta<Rep1, Precision1>, delta<Rep2, Precision2>>;
    return ct(lhs).count() <=> ct(rhs).count();
}

/** @brief Delta with 1 degree precision (Celsius/Kelvin). */
using delta_celsius = delta<int64_t>;
/** @brief Delta with 0.1 degree precision (Celsius/Kelvin). */
using delta_decicelsius = delta<int64_t, std::deci>;
/** @brief Delta with 0.001 degree precision (Celsius/Kelvin). */
using delta_millicelsius = delta<int64_t, std::milli>;
/** @brief Delta with 1 degree precision (Celsius/Kelvin). */
using delta_kelvin = delta<int64_t>;
/** @brief Delta with 0.1 degree precision (Celsius/Kelvin). */
using delta_decikelvin = delta<int64_t, std::deci>;
/** @brief Delta with 0.001 degree precision (Celsius/Kelvin). */
using delta_millikelvin = delta<int64_t, std::milli>;
/** @brief Delta with 1°F precision. */
using delta_fahrenheit = delta<int64_t, std::ratio<5, 9>>;
/** @brief Delta with 0.1°F precision. */
using delta_decifahrenheit = delta<int64_t, std::ratio<5, 90>>;
/** @brief Delta with 0.001°F precision. */
using delta_millifahrenheit = delta<int64_t, std::ratio<5, 9000>>;

/** @cond INTERNAL */
// SI prefix for a degrees-per-count ratio; nullptr when unmapped.
template<typename Ratio>
struct _si_prefix {
    static constexpr const char* value = nullptr;
};
template<>
struct _si_prefix<std::ratio<1>> {
    static constexpr const char* value = "";
};
template<>
struct _si_prefix<std::ratio<1, 10>> {
    static constexpr const char* value = "d";
};
template<>
struct _si_prefix<std::ratio<1, 1000>> {
    static constexpr const char* value = "m";
};
template<typename Ratio>
inline constexpr const char* _si_prefix_v = _si_prefix<typename Ratio::type>::value;

// Display family (degree size + unit suffix) for a delta precision, keyed on
// NORMALIZED ratios. Undefined primary template: unmapped precisions have no
// determinable unit and are not formattable.
template<typename Precision>
struct _delta_unit;
struct _celsius_delta_unit {
    using degree = std::ratio<1>;
    static constexpr const char* suffix = "°C";
};
struct _fahrenheit_delta_unit {
    using degree = std::ratio<5, 9>;
    static constexpr const char* suffix = "°F";
};
template<>
struct _delta_unit<std::ratio<1>> : _celsius_delta_unit {};
template<>
struct _delta_unit<std::ratio<1, 10>> : _celsius_delta_unit {};
template<>
struct _delta_unit<std::ratio<1, 1000>> : _celsius_delta_unit {};
template<>
struct _delta_unit<std::ratio<5, 9>> : _fahrenheit_delta_unit {};
template<>
struct _delta_unit<std::ratio<1, 18>> : _fahrenheit_delta_unit {}; // 5/90 normalized
template<>
struct _delta_unit<std::ratio<1, 1800>> : _fahrenheit_delta_unit {}; // 5/9000 normalized

// Appends a NUL-terminated string to a format output iterator.
template<typename OutputIt>
constexpr OutputIt _format_append(OutputIt out, const char* s) {
    for (; *s != '\0'; ++s) {
        *out++ = *s;
    }
    return out;
}

// The shortest fixed-notation representation of v that round-trips: never
// scientific notation, and no trailing zeros (22.5 -> "22.5", 1e6 ->
// "1000000"). Used wherever a floating-point value is rendered without a
// format spec, so to_string and std::format agree.
template<std::floating_point T>
std::string _fixed_string(T v) {
    std::string s(32, '\0');
    for (;;) {
        const auto result = std::to_chars(s.data(), s.data() + s.size(), v, std::chars_format::fixed);
        if (result.ec == std::errc()) {
            s.resize(static_cast<std::size_t>(result.ptr - s.data()));
            return s;
        }
        s.resize(s.size() * 2);
    }
}

// A count rendered without a format spec: integers exactly, floating-point
// counts in shortest round-trip fixed notation.
template<typename Rep>
std::string _count_string(const Rep& r) {
    if constexpr (std::floating_point<Rep>) {
        return _fixed_string(r);
    } else {
        return std::to_string(r);
    }
}

// How a delta or temperature type is displayed: the delta marker ("Δ" or
// ""), the unit suffix of its scale, the precision in degrees of that unit
// (dpc), and the SI prefix for that precision (nullptr when it has none).
template<typename T>
struct _display_traits;

template<typename Rep, typename Precision>
struct _display_traits<delta<Rep, Precision>> {
private:
    using _precision = typename delta<Rep, Precision>::precision;
    using _unit = _delta_unit<_precision>; // hard error if unmapped

public:
    using dpc = typename std::ratio_divide<_precision, typename _unit::degree>::type;
    static constexpr const char* mark = "Δ";
    static constexpr const char* unit = _unit::suffix;
    static constexpr const char* prefix = _si_prefix_v<dpc>;
};

// A value and SI prefix to display, followed by the type's mark and unit.
struct _display {
    double value;
    const char* prefix;
};

// The count in unprefixed degrees of the type's own scale, as a double.
template<typename Traits, typename Rep>
constexpr double _base_value(const Rep& count) {
    using dpc = typename Traits::dpc;
    double v = static_cast<double>(count);
    if constexpr (dpc::num != 1) {
        v *= static_cast<double>(dpc::num);
    }
    if constexpr (dpc::den != 1) {
        v /= static_cast<double>(dpc::den);
    }
    return v;
}

// The base-unit form: the value in unprefixed degrees of the same scale.
template<typename Traits, typename Rep>
constexpr _display _base_unit_display(const Rep& count) {
    return {_base_value<Traits>(count), ""};
}

// The scaled form. Temperatures are not rescaled, so this is the type's own
// unit: the count (as a double) with the precision's prefix. A precision with
// no SI prefix has no own unit label and is shown in unprefixed degrees.
template<typename Traits, typename Rep>
constexpr _display _scaled_display(const Rep& count) {
    if constexpr (Traits::prefix != nullptr) {
        return {static_cast<double>(count), Traits::prefix};
    } else {
        return _base_unit_display<Traits>(count);
    }
}

// Renders a display value with no format spec, followed by its unit.
template<typename Traits>
std::string _display_string(const _display& d) {
    return _fixed_string(d.value) + Traits::mark + d.prefix + Traits::unit;
}

// Renders an exact count with the type's own precision-qualified unit.
template<typename Traits, typename Rep>
std::string _own_unit_string(const Rep& count) {
    static_assert(Traits::prefix != nullptr, "thermo: precision has no SI prefix");
    return _count_string(count) + Traits::mark + Traits::prefix + Traits::unit;
}

#if CONFIG_THERMO_STD_FORMAT
// Reports an unusable format spec from a formatter's parse().
//
// std::format constant-evaluates parse() to check the format string, so
// throwing there makes a bad spec a compile error rather than a runtime fault.
// Where exceptions are unavailable, calling a non-constexpr function fails that
// same constant evaluation and so reports the error at compile time too; a
// runtime parse (std::vformat with a runtime format string) has no way to
// report it and terminates.
[[noreturn]] inline void _format_error(const char* what) {
#if defined(__cpp_exceptions) && __cpp_exceptions
    throw std::format_error(what);
#else
    (void)what;
    std::abort();
#endif
}

// The std::formatter implementation shared by deltas and temperatures; see
// the formatter specializations for the spec grammar.
template<typename Traits>
struct _formatter {
private:
    enum class _unit_form { own, base, scaled };

    std::formatter<double> _num;
    bool _has_spec = false;
    _unit_form _form = _unit_form::own;

public:
    constexpr auto parse(std::format_parse_context& ctx) {
        auto it = ctx.begin();
        if (it != ctx.end() && (*it == 'b' || *it == 's')) {
            _form = *it == 'b' ? _unit_form::base : _unit_form::scaled;
            ctx.advance_to(++it);
        }
        if (it == ctx.end() || *it == '}') {
            if constexpr (Traits::prefix == nullptr) {
                if (_form == _unit_form::own) {
                    _format_error("thermo: precision has no SI prefix; use an explicit format spec");
                }
            }
            return it;
        }
        _has_spec = true;
        return _num.parse(ctx);
    }

    template<typename Rep, typename FormatContext>
    auto format_count(const Rep& count, FormatContext& ctx) const {
        if constexpr (Traits::prefix != nullptr) { // otherwise rejected by parse()
            if (_form == _unit_form::own && !_has_spec) {
                auto out = ctx.out();
                if constexpr (std::floating_point<Rep>) {
                    out = _format_append(out, _fixed_string(count).c_str());
                } else {
                    out = std::format_to(out, "{}", count);
                }
                out = _format_append(out, Traits::mark);
                out = _format_append(out, Traits::prefix);
                return _format_append(out, Traits::unit);
            }
        }
        // With a spec the own unit is the scaled form; see _scaled_display.
        const _display d =
            _form == _unit_form::base ? _base_unit_display<Traits>(count) : _scaled_display<Traits>(count);
        auto out = _has_spec ? _num.format(d.value, ctx) : _format_append(ctx.out(), _fixed_string(d.value).c_str());
        out = _format_append(out, Traits::mark);
        out = _format_append(out, d.prefix);
        return _format_append(out, Traits::unit);
    }
};
#endif
/** @endcond */

/**
 * @brief Tag type selecting the base-unit form, see base_unit.
 */
struct base_unit_t {
    explicit base_unit_t() = default;
};

/**
 * @brief Selects the base-unit form in to_string().
 *
 * The value is shown in the unprefixed degree of its own scale (°C, K or °F,
 * with deltas keeping their Δ marker): the count times the precision, rendered
 * in the shortest fixed notation that round-trips (never scientific), exactly
 * as by the `b` format marker. Temperatures are never converted to another
 * scale.
 *
 * @code
 * to_string(millicelsius(22534), base_unit);       // "22.534°C"
 * to_string(decikelvin(2955), base_unit);          // "295.5K"
 * to_string(millifahrenheit(-40250), base_unit);   // "-40.25°F"
 * to_string(delta_millicelsius(1500), base_unit);  // "1.5Δ°C"
 * @endcode
 */
inline constexpr base_unit_t base_unit{};

/**
 * @brief Tag type selecting the scaled form, see scaled.
 */
struct scaled_t {
    explicit scaled_t() = default;
};

/**
 * @brief Selects the scaled form in to_string().
 *
 * Provided for parity with the sibling unit libraries. Temperatures are not
 * rescaled to suit their magnitude, so the scaled form is the type's own unit:
 * the count (as a double) in the shortest fixed notation that round-trips,
 * with the precision-qualified unit, exactly as by the `s` format marker. A
 * precision with no SI prefix (and so no unit label of its own) is shown in
 * the base unit.
 *
 * @code
 * to_string(millicelsius(22534), scaled);       // "22534m°C"
 * to_string(delta_millicelsius(1500), scaled);  // "1500Δm°C"
 * @endcode
 */
inline constexpr scaled_t scaled{};

/**
 * @brief Renders a delta as its exact count with a precision-qualified unit,
 * e.g. `to_string(delta_millicelsius(1500)) == "1500Δm°C"`.
 *
 * Floating-point counts are rendered in the shortest fixed notation that
 * round-trips, as by `{}` in std::format.
 */
template<typename Rep, typename Precision>
std::string to_string(const delta<Rep, Precision>& d) {
    return _own_unit_string<_display_traits<delta<Rep, Precision>>>(d.count());
}

/**
 * @brief Renders a delta in its base unit (Δ°C or Δ°F), see base_unit,
 * e.g. `to_string(delta_millicelsius(1500), base_unit) == "1.5Δ°C"`.
 */
template<typename Rep, typename Precision>
std::string to_string(const delta<Rep, Precision>& d, base_unit_t) {
    using traits = _display_traits<delta<Rep, Precision>>;
    return _display_string<traits>(_base_unit_display<traits>(d.count()));
}

/**
 * @brief Renders a delta in its scaled form (its own unit), see scaled,
 * e.g. `to_string(delta_millicelsius(1500), scaled) == "1500Δm°C"`.
 */
template<typename Rep, typename Precision>
std::string to_string(const delta<Rep, Precision>& d, scaled_t) {
    using traits = _display_traits<delta<Rep, Precision>>;
    return _display_string<traits>(_scaled_display<traits>(d.count()));
}

// ============================================================================
// Temperature Scales and Absolute Temperatures
// ============================================================================

/** @cond INTERNAL */
/**
 * @brief Celsius temperature scale.
 *
 * 0°C = 273.15 K (offset from absolute zero).
 */
struct celsius_scale {
    using offset = std::ratio<27315, 100>;
    using degree = std::ratio<1>;
    static constexpr const char* suffix = "°C";
};

/**
 * @brief Kelvin temperature scale.
 *
 * 0 K = absolute zero.
 */
struct kelvin_scale {
    using offset = std::ratio<0>;
    using degree = std::ratio<1>;
    static constexpr const char* suffix = "K";
};

/**
 * @brief Fahrenheit temperature scale.
 *
 * 0°F = 255.37 K (approximately).
 */
struct fahrenheit_scale {
    using offset = std::ratio<45967, 180>;
    using degree = std::ratio<5, 9>;
    static constexpr const char* suffix = "°F";
};

/**
 * @brief True when a scale's zero point is not absolute zero.
 *
 * Negating an absolute temperature only makes sense on a "relative" scale
 * (e.g. Celsius, Fahrenheit), where the zero point sits above absolute zero.
 * On an absolute scale (Kelvin, offset == 0) a negated temperature always
 * falls below absolute zero, so unary minus is disabled there.
 */
template<typename Scale>
inline constexpr bool _is_relative_scale = std::ratio_not_equal_v<typename Scale::offset, std::ratio<0>>;
/** @endcond */

template<typename Scale, typename Delta = delta<int64_t>>
class temperature;

/** @cond INTERNAL */
template<typename Scale1, typename Delta1, typename Scale2, typename Delta2>
struct _lossless_temperature_conversion_impl {
    using from_prec = typename Delta1::precision;
    using to_prec = typename Delta2::precision;
    using from_off = typename Scale1::offset;
    using to_off = typename Scale2::offset;

    using prec_ratio = std::ratio_divide<from_prec, to_prec>;
    using offset_diff = std::ratio_subtract<from_off, to_off>;
    using offset_in_target_units = std::ratio_divide<offset_diff, to_prec>;

    static constexpr bool value = prec_ratio::den == 1 && offset_in_target_units::den == 1;
};

template<typename Scale1, typename Delta1, typename Scale2, typename Delta2>
concept _lossless_temperature_conversion = _lossless_temperature_conversion_impl<Scale1, Delta1, Scale2, Delta2>::value;

/**
 * @brief Trait to detect temperature specializations.
 * @tparam T Type to check.
 */
template<typename T>
struct is_temperature : std::false_type {};

template<typename Scale, typename Delta>
struct is_temperature<temperature<Scale, Delta>> : std::true_type {};

template<typename T>
inline constexpr bool is_temperature_v = is_temperature<T>::value;
/** @endcond */

/**
 * @brief Converts a temperature to a different scale or precision.
 *
 * @tparam ToTemp The target temperature type.
 * @tparam Scale  Source scale.
 * @tparam Delta  Source delta type.
 * @param t       The temperature to convert.
 *
 * @return The converted temperature.
 */
template<typename ToTemp, typename Scale, typename Delta>
constexpr ToTemp temperature_cast(const temperature<Scale, Delta>& t);

/**
 * @brief An absolute temperature on a given scale.
 *
 * Similar to std::chrono::time_point, temperature represents an absolute
 * point on a temperature scale. The scale defines the zero point (offset
 * from absolute zero), and the delta type defines the precision.
 *
 * @tparam Scale The temperature scale (celsius_scale, kelvin_scale, fahrenheit_scale).
 * @tparam Delta The delta type for precision (default: delta<int64_t>).
 */
template<typename Scale, typename Delta>
class temperature {
    static_assert(is_delta<Delta>::value, "Delta must be a thermo::delta type");

public:
    /** @brief The temperature scale. */
    using scale = Scale;
    /** @brief The delta type. */
    using delta_type = Delta;
    /** @brief The representation type. */
    using rep = typename Delta::rep;

    /** @brief Constructs a temperature at the scale's zero point. */
    constexpr temperature() = default;
    temperature(const temperature&) = default;

    /**
     * @brief Constructs from a tick count.
     * @tparam Rep2 The source representation type.
     * @param r     The tick count in scale units.
     */
    template<typename Rep2>
        requires std::convertible_to<const Rep2&, rep> && (treat_as_inexact_v<rep> || !treat_as_inexact_v<Rep2>)
    constexpr explicit temperature(const Rep2& r)
        : _d(static_cast<rep>(r)) {}

    /**
     * @brief Constructs from a delta.
     * @param d The delta from the scale's zero point.
     */
    constexpr explicit temperature(const Delta& d)
        : _d(d) {}

    // Same-scale, different precision - implicit when lossless
    template<typename Delta2>
        requires(!std::is_same_v<Delta, Delta2>) &&
        (treat_as_inexact_v<rep> ||
         (_harmonic_precision<typename Delta2::precision, typename Delta::precision> &&
          !treat_as_inexact_v<typename Delta2::rep>))
    constexpr temperature(const temperature<Scale, Delta2>& t)
        : _d(delta_cast<Delta>(Delta2(t.count()))) {}

    // Same-scale, different precision - explicit when lossy
    template<typename Delta2>
        requires(!std::is_same_v<Delta, Delta2>) && (!treat_as_inexact_v<rep>) &&
        (!_harmonic_precision<typename Delta2::precision, typename Delta::precision>)
    constexpr explicit temperature(const temperature<Scale, Delta2>& t)
        : _d(delta_cast<Delta>(Delta2(t.count()))) {}

    // Cross-scale/precision conversion - implicit when lossless
    template<typename Scale2, typename Delta2>
        requires(!std::is_same_v<temperature, temperature<Scale2, Delta2>>) &&
        _lossless_temperature_conversion<Scale2, Delta2, Scale, Delta> &&
        (treat_as_inexact_v<rep> || !treat_as_inexact_v<typename Delta2::rep>)
    constexpr temperature(const temperature<Scale2, Delta2>& t)
        : temperature(temperature_cast<temperature>(t)) {}

    // Cross-scale/precision conversion - explicit when lossy
    template<typename Scale2, typename Delta2>
        requires(!std::is_same_v<temperature, temperature<Scale2, Delta2>>) &&
        (!_lossless_temperature_conversion<Scale2, Delta2, Scale, Delta>)
    constexpr explicit temperature(const temperature<Scale2, Delta2>& t)
        : temperature(temperature_cast<temperature>(t)) {}

    ~temperature() = default;
    temperature& operator=(const temperature&) = default;

    /** @brief Returns the tick count. */
    constexpr rep count() const { return _d.count(); }

    constexpr temperature& operator++() {
        ++_d;
        return *this;
    }

    constexpr temperature operator++(int) { return temperature(_d++); }

    constexpr temperature& operator--() {
        --_d;
        return *this;
    }

    constexpr temperature operator--(int) { return temperature(_d--); }

    /**
     * @brief Negates the temperature (e.g. to write @c -123_dc as @c -(123_dc)).
     *
     * Only available on relative scales such as Celsius and Fahrenheit; on an
     * absolute scale (Kelvin) negation would fall below absolute zero and is a
     * compile error.
     */
    constexpr temperature operator-() const
        requires _is_relative_scale<Scale>
    {
        return temperature(-_d);
    }

    template<typename Rep2, typename Precision2>
    constexpr temperature& operator+=(const delta<Rep2, Precision2>& d) {
        _d += d;
        return *this;
    }

    template<typename Rep2, typename Precision2>
    constexpr temperature& operator-=(const delta<Rep2, Precision2>& d) {
        _d -= d;
        return *this;
    }

    /** @brief Returns the minimum representable temperature. */
    static constexpr temperature min() noexcept { return temperature(Delta::min()); }

    /** @brief Returns the maximum representable temperature. */
    static constexpr temperature max() noexcept { return temperature(Delta::max()); }

private:
    Delta _d{};
};

template<typename ToTemp, typename Scale, typename Delta>
constexpr ToTemp temperature_cast(const temperature<Scale, Delta>& t) {
    using ToScale = typename ToTemp::scale;
    using ToDelta = typename ToTemp::delta_type;
    using to_rep = typename ToDelta::rep;

    if constexpr (std::is_same_v<Scale, ToScale> && std::is_same_v<Delta, ToDelta>) {
        return t;
    } else if constexpr (std::is_same_v<Scale, ToScale>) {
        return ToTemp(delta_cast<ToDelta>(Delta(t.count())));
    } else {
        using from_prec = typename Delta::precision;
        using from_off = typename Scale::offset;
        using to_prec = typename ToDelta::precision;
        using to_off = typename ToScale::offset;

        using offset_diff = std::ratio_subtract<from_off, to_off>;
        using common_rep = std::common_type_t<typename Delta::rep, to_rep, intmax_t>;

        common_rep from_val = static_cast<common_rep>(t.count());

        using prec_ratio = std::ratio_divide<from_prec, to_prec>;
        using offset_ratio = std::ratio_divide<offset_diff, to_prec>;

        if constexpr (treat_as_inexact_v<common_rep>) {
            constexpr double pr = static_cast<double>(prec_ratio::num) / prec_ratio::den;
            constexpr double or_ = static_cast<double>(offset_ratio::num) / offset_ratio::den;
            common_rep result = from_val * pr + or_;
            return ToTemp(ToDelta(static_cast<to_rep>(result)));
        } else {
            constexpr intmax_t combined_den =
                static_cast<intmax_t>(prec_ratio::den) * static_cast<intmax_t>(offset_ratio::den);
            common_rep numerator =
                from_val * static_cast<common_rep>(prec_ratio::num) * static_cast<common_rep>(offset_ratio::den) +
                static_cast<common_rep>(offset_ratio::num) * static_cast<common_rep>(prec_ratio::den);
            common_rep result = numerator / static_cast<common_rep>(combined_den);
            return ToTemp(ToDelta(static_cast<to_rep>(result)));
        }
    }
}

/** @brief Adds two temperatures on the same scale. */
template<typename Scale, typename Delta1, typename Delta2>
constexpr auto operator+(const temperature<Scale, Delta1>& lhs, const temperature<Scale, Delta2>& rhs)
    -> temperature<Scale, std::common_type_t<Delta1, Delta2>> {
    using cd = std::common_type_t<Delta1, Delta2>;
    return temperature<Scale, cd>(cd(lhs.count()) + cd(rhs.count()));
}

/** @brief Subtracts two temperatures on the same scale. */
template<typename Scale, typename Delta1, typename Delta2>
constexpr auto operator-(const temperature<Scale, Delta1>& lhs, const temperature<Scale, Delta2>& rhs)
    -> temperature<Scale, std::common_type_t<Delta1, Delta2>> {
    using cd = std::common_type_t<Delta1, Delta2>;
    return temperature<Scale, cd>(cd(lhs.count()) - cd(rhs.count()));
}

/**
 * @brief Returns the difference between two temperatures as a delta.
 *
 * @param lhs First temperature.
 * @param rhs Second temperature.
 *
 * @return lhs - rhs as a delta.
 */
template<typename Scale, typename Delta1, typename Delta2>
constexpr auto difference(const temperature<Scale, Delta1>& lhs, const temperature<Scale, Delta2>& rhs)
    -> std::common_type_t<Delta1, Delta2> {
    using cd = std::common_type_t<Delta1, Delta2>;
    return cd(lhs.count() - rhs.count());
}

/** @brief Adds a delta to a temperature. */
template<typename Scale, typename Delta1, typename Rep2, typename Precision2>
constexpr auto operator+(const temperature<Scale, Delta1>& t, const delta<Rep2, Precision2>& d)
    -> temperature<Scale, std::common_type_t<Delta1, delta<Rep2, Precision2>>> {
    using result_delta = std::common_type_t<Delta1, delta<Rep2, Precision2>>;
    return temperature<Scale, result_delta>(result_delta(t.count()) + result_delta(d));
}

/** @brief Adds a temperature to a delta. */
template<typename Rep1, typename Precision1, typename Scale, typename Delta2>
constexpr auto operator+(const delta<Rep1, Precision1>& d, const temperature<Scale, Delta2>& t)
    -> temperature<Scale, std::common_type_t<delta<Rep1, Precision1>, Delta2>> {
    return t + d;
}

/** @brief Subtracts a delta from a temperature. */
template<typename Scale, typename Delta1, typename Rep2, typename Precision2>
constexpr auto operator-(const temperature<Scale, Delta1>& t, const delta<Rep2, Precision2>& d)
    -> temperature<Scale, std::common_type_t<Delta1, delta<Rep2, Precision2>>> {
    using result_delta = std::common_type_t<Delta1, delta<Rep2, Precision2>>;
    return temperature<Scale, result_delta>(result_delta(t.count()) - result_delta(d));
}

template<typename Scale, typename Delta1, typename Delta2>
constexpr bool operator==(const temperature<Scale, Delta1>& lhs, const temperature<Scale, Delta2>& rhs) {
    return lhs.count() == rhs.count();
}

template<typename Scale, typename Delta1, typename Delta2>
    requires std::three_way_comparable<std::common_type_t<typename Delta1::rep, typename Delta2::rep>>
constexpr auto operator<=>(const temperature<Scale, Delta1>& lhs, const temperature<Scale, Delta2>& rhs) {
    return lhs.count() <=> rhs.count();
}

/** @brief Celsius with 1 degree precision. */
using celsius = temperature<celsius_scale>;
/** @brief Celsius with 0.1 degree precision. */
using decicelsius = temperature<celsius_scale, delta<int64_t, std::deci>>;
/** @brief Celsius with 0.001 degree precision. */
using millicelsius = temperature<celsius_scale, delta<int64_t, std::milli>>;
/** @brief Kelvin with 1 degree precision. */
using kelvin = temperature<kelvin_scale>;
/** @brief Kelvin with 0.1 degree precision. */
using decikelvin = temperature<kelvin_scale, delta<int64_t, std::deci>>;
/** @brief Kelvin with 0.001 degree precision. */
using millikelvin = temperature<kelvin_scale, delta<int64_t, std::milli>>;
/** @brief Fahrenheit with 1 degree precision. */
using fahrenheit = temperature<fahrenheit_scale, delta<int64_t, std::ratio<5, 9>>>;
/** @brief Fahrenheit with 0.1 degree precision. */
using decifahrenheit = temperature<fahrenheit_scale, delta<int64_t, std::ratio<5, 90>>>;
/** @brief Fahrenheit with 0.001 degree precision. */
using millifahrenheit = temperature<fahrenheit_scale, delta<int64_t, std::ratio<5, 9000>>>;

/** @cond INTERNAL */
template<typename Scale, typename Rep, typename Precision>
struct _display_traits<temperature<Scale, delta<Rep, Precision>>> {
    using dpc = typename std::ratio_divide<typename delta<Rep, Precision>::precision, typename Scale::degree>::type;
    static constexpr const char* mark = "";
    static constexpr const char* unit = Scale::suffix;
    static constexpr const char* prefix = _si_prefix_v<dpc>;
};
/** @endcond */

/**
 * @brief Renders a temperature as its exact count with a precision-qualified
 * unit, e.g. `to_string(millicelsius(22500)) == "22500m°C"`.
 *
 * Floating-point counts are rendered in the shortest fixed notation that
 * round-trips, as by `{}` in std::format.
 */
template<typename Scale, typename Rep, typename Precision>
std::string to_string(const temperature<Scale, delta<Rep, Precision>>& t) {
    return _own_unit_string<_display_traits<temperature<Scale, delta<Rep, Precision>>>>(t.count());
}

/**
 * @brief Renders a temperature in the unprefixed degree of its own scale
 * (°C, K or °F), see base_unit,
 * e.g. `to_string(millicelsius(22534), base_unit) == "22.534°C"`.
 */
template<typename Scale, typename Rep, typename Precision>
std::string to_string(const temperature<Scale, delta<Rep, Precision>>& t, base_unit_t) {
    using traits = _display_traits<temperature<Scale, delta<Rep, Precision>>>;
    return _display_string<traits>(_base_unit_display<traits>(t.count()));
}

/**
 * @brief Renders a temperature in its scaled form (its own unit), see scaled,
 * e.g. `to_string(millicelsius(22534), scaled) == "22534m°C"`.
 */
template<typename Scale, typename Rep, typename Precision>
std::string to_string(const temperature<Scale, delta<Rep, Precision>>& t, scaled_t) {
    using traits = _display_traits<temperature<Scale, delta<Rep, Precision>>>;
    return _display_string<traits>(_scaled_display<traits>(t.count()));
}

} // namespace thermo

namespace std {

template<typename Rep1, typename Precision1, typename Rep2, typename Precision2>
struct common_type<thermo::delta<Rep1, Precision1>, thermo::delta<Rep2, Precision2>> {
private:
    using common_precision = std::ratio<
        thermo::_gcd(Precision1::num, Precision2::num),
        (Precision1::den / thermo::_gcd(Precision1::den, Precision2::den)) * Precision2::den>;

public:
    using type = thermo::delta<std::common_type_t<Rep1, Rep2>, common_precision>;
};

template<typename Scale, typename Delta1, typename Delta2>
struct common_type<thermo::temperature<Scale, Delta1>, thermo::temperature<Scale, Delta2>> {
    using type = thermo::temperature<Scale, std::common_type_t<Delta1, Delta2>>;
};

#if CONFIG_THERMO_STD_FORMAT
// "{}" prints the exact stored count with a precision-qualified unit
// (millicelsius(22500) -> "22500m°C"; floating-point counts in the shortest
// fixed notation that round-trips, as to_string does: never scientific). A
// non-empty spec controls only how the number is rendered (as a double),
// never the unit, so both forms use the same unit:
// std::format("{:.1f}", millicelsius(22534)) == "22534.0m°C".
//
// The unit changes only when the spec opts in with a single leading marker
// letter; the rest of the spec is then the std::formatter<double> spec:
// - 'b' (base unit, see thermo::base_unit) shows the value in the unprefixed
//   degree of the same scale (°C, K or °F; never another scale):
//   std::format("{:b.1f}", millicelsius(22534)) == "22.5°C".
// - 's' (scaled, see thermo::scaled) keeps the type's own unit, since
//   temperatures are not rescaled: std::format("{:s.1f}", millicelsius(22534))
//   == "22534.0m°C".
// With 'b' or 's' and nothing after the marker, the value is rendered in the
// shortest fixed notation that round-trips (never scientific), exactly as by
// to_string(t, thermo::base_unit) or to_string(t, thermo::scaled):
// std::format("{:b}", millicelsius(22534)) == "22.534°C".
//
// A spec starting with 'b' or 's' is always a marker, so these letters cannot
// be the fill character of a spec that starts with a fill ("{:b>10.1f}" is the
// marker followed by ">10.1f").
//
// A precision with no SI prefix has no unit label of its own: "{}" is a
// format error, and any other spec renders in the base unit.
template<typename Scale, typename Rep, typename Precision>
struct formatter<thermo::temperature<Scale, thermo::delta<Rep, Precision>>>
    : thermo::_formatter<thermo::_display_traits<thermo::temperature<Scale, thermo::delta<Rep, Precision>>>> {
    template<typename FormatContext>
    auto format(const thermo::temperature<Scale, thermo::delta<Rep, Precision>>& t, FormatContext& ctx) const {
        return this->format_count(t.count(), ctx);
    }
};

// As for temperatures, with the Δ marker before the unit:
// std::format("{}", delta_millicelsius(1500)) == "1500Δm°C",
// std::format("{:.1f}", delta_millicelsius(1500)) == "1500.0Δm°C" and
// std::format("{:b.1f}", delta_millicelsius(1500)) == "1.5Δ°C". Celsius and
// kelvin deltas share types and display as Δ°C.
template<typename Rep, typename Precision>
struct formatter<thermo::delta<Rep, Precision>>
    : thermo::_formatter<thermo::_display_traits<thermo::delta<Rep, Precision>>> {
    template<typename FormatContext>
    auto format(const thermo::delta<Rep, Precision>& d, FormatContext& ctx) const {
        return this->format_count(d.count(), ctx);
    }
};
#endif

} // namespace std

/**
 * @brief User-defined literals for temperature types.
 */
namespace thermo_literals {
/** @cond INTERNAL */
namespace detail {

template<unsigned long long Value, unsigned long long Power>
struct pow10 {
    static constexpr unsigned long long value = 10 * pow10<Value, Power - 1>::value;
};

template<unsigned long long Value>
struct pow10<Value, 0> {
    static constexpr unsigned long long value = Value;
};

template<char... Digits>
struct parse_int;

template<char D, char... Rest>
struct parse_int<D, Rest...> {
    static_assert(D >= '0' && D <= '9', "invalid digit");
    static constexpr unsigned long long value = pow10<D - '0', sizeof...(Rest)>::value + parse_int<Rest...>::value;
};

template<char D>
struct parse_int<D> {
    static_assert(D >= '0' && D <= '9', "invalid digit");
    static constexpr unsigned long long value = D - '0';
};

template<typename Delta, char... Digits>
constexpr Delta check_overflow() {
    using parsed = parse_int<Digits...>;
    constexpr typename Delta::rep repval = parsed::value;
    static_assert(
        repval >= 0 && static_cast<unsigned long long>(repval) == parsed::value,
        "literal value cannot be represented by delta type"
    );
    return Delta(repval);
}

} // namespace detail
/** @endcond */

/** @brief Literal for celsius (e.g., 25_c). */
template<char... Digits>
constexpr thermo::celsius operator""_c() {
    return thermo::celsius(detail::check_overflow<thermo::delta_celsius, Digits...>());
}

/** @brief Literal for decicelsius (e.g., 250_dc). */
template<char... Digits>
constexpr thermo::decicelsius operator""_dc() {
    return thermo::decicelsius(detail::check_overflow<thermo::delta_decicelsius, Digits...>());
}

/** @brief Literal for millicelsius (e.g., 25000_mc). */
template<char... Digits>
constexpr thermo::millicelsius operator""_mc() {
    return thermo::millicelsius(detail::check_overflow<thermo::delta_millicelsius, Digits...>());
}

/** @brief Literal for kelvin (e.g., 298_k). */
template<char... Digits>
constexpr thermo::kelvin operator""_k() {
    return thermo::kelvin(detail::check_overflow<thermo::delta_kelvin, Digits...>());
}

/** @brief Literal for decikelvin (e.g., 2980_dk). */
template<char... Digits>
constexpr thermo::decikelvin operator""_dk() {
    return thermo::decikelvin(detail::check_overflow<thermo::delta_decikelvin, Digits...>());
}

/** @brief Literal for millikelvin (e.g., 298000_mk). */
template<char... Digits>
constexpr thermo::millikelvin operator""_mk() {
    return thermo::millikelvin(detail::check_overflow<thermo::delta_millikelvin, Digits...>());
}

/** @brief Literal for fahrenheit (e.g., 77_f). */
template<char... Digits>
constexpr thermo::fahrenheit operator""_f() {
    return thermo::fahrenheit(detail::check_overflow<thermo::delta_fahrenheit, Digits...>());
}

/** @brief Literal for decifahrenheit (e.g., 770_df). */
template<char... Digits>
constexpr thermo::decifahrenheit operator""_df() {
    return thermo::decifahrenheit(detail::check_overflow<thermo::delta_decifahrenheit, Digits...>());
}

/** @brief Literal for millifahrenheit (e.g., 77000_mf). */
template<char... Digits>
constexpr thermo::millifahrenheit operator""_mf() {
    return thermo::millifahrenheit(detail::check_overflow<thermo::delta_millifahrenheit, Digits...>());
}

/** @brief Literal for delta_celsius (e.g., 5_delta_c); ASCII spelling of `_Δc`. */
template<char... Digits>
constexpr thermo::delta_celsius operator""_delta_c() {
    return detail::check_overflow<thermo::delta_celsius, Digits...>();
}

/** @brief Literal for delta_millicelsius (e.g., 5000_delta_mc); ASCII spelling of `_Δmc`. */
template<char... Digits>
constexpr thermo::delta_millicelsius operator""_delta_mc() {
    return detail::check_overflow<thermo::delta_millicelsius, Digits...>();
}

/** @brief Literal for delta_kelvin (e.g., 5_delta_k); ASCII spelling of `_Δk`. */
template<char... Digits>
constexpr thermo::delta_kelvin operator""_delta_k() {
    return detail::check_overflow<thermo::delta_kelvin, Digits...>();
}

/** @brief Literal for delta_millikelvin (e.g., 5000_delta_mk); ASCII spelling of `_Δmk`. */
template<char... Digits>
constexpr thermo::delta_millikelvin operator""_delta_mk() {
    return detail::check_overflow<thermo::delta_millikelvin, Digits...>();
}

/** @brief Literal for delta_fahrenheit (e.g., 9_delta_f); ASCII spelling of `_Δf`. */
template<char... Digits>
constexpr thermo::delta_fahrenheit operator""_delta_f() {
    return detail::check_overflow<thermo::delta_fahrenheit, Digits...>();
}

/** @brief Literal for delta_millifahrenheit (e.g., 9000_delta_mf); ASCII spelling of `_Δmf`. */
template<char... Digits>
constexpr thermo::delta_millifahrenheit operator""_delta_mf() {
    return detail::check_overflow<thermo::delta_millifahrenheit, Digits...>();
}

#if THERMO_HAS_UTF8_LITERALS
/** @brief Literal for delta_celsius (e.g., 5_Δc). Requires THERMO_HAS_UTF8_LITERALS; see `_delta_c`. */
template<char... Digits>
constexpr thermo::delta_celsius operator""_Δc() {
    return detail::check_overflow<thermo::delta_celsius, Digits...>();
}

/** @brief Literal for delta_millicelsius (e.g., 5000_Δmc). Requires THERMO_HAS_UTF8_LITERALS; see `_delta_mc`. */
template<char... Digits>
constexpr thermo::delta_millicelsius operator""_Δmc() {
    return detail::check_overflow<thermo::delta_millicelsius, Digits...>();
}

/** @brief Literal for delta_kelvin (e.g., 5_Δk). Requires THERMO_HAS_UTF8_LITERALS; see `_delta_k`. */
template<char... Digits>
constexpr thermo::delta_kelvin operator""_Δk() {
    return detail::check_overflow<thermo::delta_kelvin, Digits...>();
}

/** @brief Literal for delta_millikelvin (e.g., 5000_Δmk). Requires THERMO_HAS_UTF8_LITERALS; see `_delta_mk`. */
template<char... Digits>
constexpr thermo::delta_millikelvin operator""_Δmk() {
    return detail::check_overflow<thermo::delta_millikelvin, Digits...>();
}

/** @brief Literal for delta_fahrenheit (e.g., 9_Δf). Requires THERMO_HAS_UTF8_LITERALS; see `_delta_f`. */
template<char... Digits>
constexpr thermo::delta_fahrenheit operator""_Δf() {
    return detail::check_overflow<thermo::delta_fahrenheit, Digits...>();
}

/** @brief Literal for delta_millifahrenheit (e.g., 9000_Δmf). Requires THERMO_HAS_UTF8_LITERALS; see `_delta_mf`. */
template<char... Digits>
constexpr thermo::delta_millifahrenheit operator""_Δmf() {
    return detail::check_overflow<thermo::delta_millifahrenheit, Digits...>();
}
#endif

} // namespace thermo_literals
