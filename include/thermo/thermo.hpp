#pragma once

/**
 * @file thermo
 * @brief Temperature types and arithmetic, modeled after std::chrono.
 */

#include <compare>
#include <concepts>
#include <cstdint>
#include <limits>
#include <ratio>
#include <string>
#if __has_include(<format>) && defined(__cpp_lib_format)
#include <format>
#endif
#include <assert.h>

static_assert(__cplusplus >= 202302L, "thermo requires C++23 or later");

/**
 * @brief Temperature types and utilities.
 *
 * This library provides type-safe temperature handling with support for
 * multiple scales (Celsius, Kelvin, Fahrenheit) and precisions, following
 * the design of std::chrono. It distinguishes between absolute temperatures
 * and temperature differences (deltas).
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
using delta_millifahrenheit = delta<int64_t, std::ratio<5, 900>>;

inline std::string to_string(delta<int64_t> d) {
    return std::to_string(d.count()) + "Δ°C";
}

inline std::string to_string(delta<int64_t, std::deci> d) {
    return std::to_string(d.count()) + "Δd°C";
}

inline std::string to_string(delta<int64_t, std::milli> d) {
    return std::to_string(d.count()) + "Δm°C";
}

inline std::string to_string(delta_fahrenheit d) {
    return std::to_string(d.count()) + "Δ°F";
}

inline std::string to_string(delta_decifahrenheit d) {
    return std::to_string(d.count()) + "Δd°F";
}

inline std::string to_string(delta_millifahrenheit d) {
    return std::to_string(d.count()) + "Δm°F";
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
    static constexpr const char* suffix = "°C";
};

/**
 * @brief Kelvin temperature scale.
 *
 * 0 K = absolute zero.
 */
struct kelvin_scale {
    using offset = std::ratio<0>;
    static constexpr const char* suffix = "K";
};

/**
 * @brief Fahrenheit temperature scale.
 *
 * 0°F = 255.37 K (approximately).
 */
struct fahrenheit_scale {
    using offset = std::ratio<45967, 180>;
    static constexpr const char* suffix = "°F";
};
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
using millifahrenheit = temperature<fahrenheit_scale, delta<int64_t, std::ratio<5, 900>>>;

inline std::string to_string(celsius t) {
    return std::to_string(t.count()) + "°C";
}

inline std::string to_string(decicelsius t) {
    return std::to_string(t.count()) + "d°C";
}

inline std::string to_string(millicelsius t) {
    return std::to_string(t.count()) + "m°C";
}

inline std::string to_string(kelvin t) {
    return std::to_string(t.count()) + "K";
}

inline std::string to_string(decikelvin t) {
    return std::to_string(t.count()) + "dK";
}

inline std::string to_string(millikelvin t) {
    return std::to_string(t.count()) + "mK";
}

inline std::string to_string(fahrenheit t) {
    return std::to_string(t.count()) + "°F";
}

inline std::string to_string(decifahrenheit t) {
    return std::to_string(t.count()) + "d°F";
}

inline std::string to_string(millifahrenheit t) {
    return std::to_string(t.count()) + "m°F";
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

#if __has_include(<format>) && defined(__cpp_lib_format)
template<>
struct formatter<thermo::celsius> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    auto format(thermo::celsius t, format_context& ctx) const { return std::format_to(ctx.out(), "{}°C", t.count()); }
};

template<>
struct formatter<thermo::decicelsius> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    auto format(thermo::decicelsius t, format_context& ctx) const {
        return std::format_to(ctx.out(), "{}d°C", t.count());
    }
};

template<>
struct formatter<thermo::millicelsius> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    auto format(thermo::millicelsius t, format_context& ctx) const {
        return std::format_to(ctx.out(), "{}m°C", t.count());
    }
};

template<>
struct formatter<thermo::kelvin> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    auto format(thermo::kelvin t, format_context& ctx) const { return std::format_to(ctx.out(), "{}K", t.count()); }
};

template<>
struct formatter<thermo::decikelvin> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    auto format(thermo::decikelvin t, format_context& ctx) const {
        return std::format_to(ctx.out(), "{}dK", t.count());
    }
};

template<>
struct formatter<thermo::millikelvin> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    auto format(thermo::millikelvin t, format_context& ctx) const {
        return std::format_to(ctx.out(), "{}mK", t.count());
    }
};

template<>
struct formatter<thermo::fahrenheit> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    auto format(thermo::fahrenheit t, format_context& ctx) const {
        return std::format_to(ctx.out(), "{}°F", t.count());
    }
};

template<>
struct formatter<thermo::decifahrenheit> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    auto format(thermo::decifahrenheit t, format_context& ctx) const {
        return std::format_to(ctx.out(), "{}d°F", t.count());
    }
};

template<>
struct formatter<thermo::millifahrenheit> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    auto format(thermo::millifahrenheit t, format_context& ctx) const {
        return std::format_to(ctx.out(), "{}m°F", t.count());
    }
};

template<>
struct formatter<thermo::delta<int64_t>> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    auto format(thermo::delta<int64_t> d, format_context& ctx) const {
        return std::format_to(ctx.out(), "{}Δ°C", d.count());
    }
};

template<>
struct formatter<thermo::delta<int64_t, std::deci>> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    auto format(thermo::delta<int64_t, std::deci> d, format_context& ctx) const {
        return std::format_to(ctx.out(), "{}Δd°C", d.count());
    }
};

template<>
struct formatter<thermo::delta<int64_t, std::milli>> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    auto format(thermo::delta<int64_t, std::milli> d, format_context& ctx) const {
        return std::format_to(ctx.out(), "{}Δm°C", d.count());
    }
};

template<>
struct formatter<thermo::delta_fahrenheit> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    auto format(thermo::delta_fahrenheit d, format_context& ctx) const {
        return std::format_to(ctx.out(), "{}Δ°F", d.count());
    }
};

template<>
struct formatter<thermo::delta_decifahrenheit> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    auto format(thermo::delta_decifahrenheit d, format_context& ctx) const {
        return std::format_to(ctx.out(), "{}Δd°F", d.count());
    }
};

template<>
struct formatter<thermo::delta_millifahrenheit> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    auto format(thermo::delta_millifahrenheit d, format_context& ctx) const {
        return std::format_to(ctx.out(), "{}Δm°F", d.count());
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

/** @brief Literal for delta_celsius (e.g., 5_Δc). */
template<char... Digits>
constexpr thermo::delta_celsius operator""_Δc() {
    return detail::check_overflow<thermo::delta_celsius, Digits...>();
}

/** @brief Literal for delta_millicelsius (e.g., 5000_Δmc). */
template<char... Digits>
constexpr thermo::delta_millicelsius operator""_Δmc() {
    return detail::check_overflow<thermo::delta_millicelsius, Digits...>();
}

/** @brief Literal for delta_kelvin (e.g., 5_Δk). */
template<char... Digits>
constexpr thermo::delta_kelvin operator""_Δk() {
    return detail::check_overflow<thermo::delta_kelvin, Digits...>();
}

/** @brief Literal for delta_millikelvin (e.g., 5000_Δmk). */
template<char... Digits>
constexpr thermo::delta_millikelvin operator""_Δmk() {
    return detail::check_overflow<thermo::delta_millikelvin, Digits...>();
}

/** @brief Literal for delta_fahrenheit (e.g., 9_Δf). */
template<char... Digits>
constexpr thermo::delta_fahrenheit operator""_Δf() {
    return detail::check_overflow<thermo::delta_fahrenheit, Digits...>();
}

/** @brief Literal for delta_millifahrenheit (e.g., 9000_Δmf). */
template<char... Digits>
constexpr thermo::delta_millifahrenheit operator""_Δmf() {
    return detail::check_overflow<thermo::delta_millifahrenheit, Digits...>();
}

} // namespace thermo_literals
