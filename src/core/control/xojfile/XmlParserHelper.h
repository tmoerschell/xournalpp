/*
 * Xournal++
 *
 * Helper methods to parse .xoj / .xopp documents
 *
 * @author Xournal++ Team
 * https://github.com/xournalpp/xournalpp
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <charconv>
#include <istream>
#include <optional>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <vector>

#include <glib.h>

#include "util/Assert.h"
#include "util/Color.h"
#include "util/StringUtils.h"
#include "util/Util.h"
#include "util/serdesstream.h"
#include "util/utf8_view.h"

#include "filesystem.h"

class LineStyle;
class XmlParser;


namespace XmlParserHelper {

class AttributeMap {
public:
    /**
     * Build an attribute map for the current node. Names and values parameters
     * should be directly forwarded from the GMarkup callback.
     */
    AttributeMap(const char** attributeNames, const char** attributeValues);

    /**
     * Look up `name` in the attribute map
     * @return The matching value, or std::nullopt if it was not found.
     */
    std::optional<std::u8string_view> operator[](const std::u8string_view name) const;

private:
    std::vector<std::u8string_view> names;
    std::vector<std::u8string_view> values;
};

// generic templates

/**
 * Look up an attribute and parse it as T if it was found.
 */
template <typename T>
std::optional<T> getAttrib(std::u8string_view name, const AttributeMap& attributeMap);
/**
 * Look up an attribute and parse it as T. If the attribute was not found, use a default and optionally print a warning.
 */
template <typename T>
T getAttribMandatory(std::u8string_view name, const AttributeMap& attributeMap, const T& defaultValue = {},
                     bool warn = true);
// specializations
template <>
std::optional<std::u8string_view> getAttrib<std::u8string_view>(std::u8string_view name,
                                                                const AttributeMap& attributeMap);
template <>
std::optional<LineStyle> getAttrib<LineStyle>(std::u8string_view name, const AttributeMap& attributeMap);

// "color" attribute
Color getAttribColorMandatory(const AttributeMap& attributeMap, const Color& defaultValue, bool bg = false);
// Attempt to match string with background-specific color "translations"
std::optional<Color> parseBgColor(std::u8string_view sv);
// Parse str as a RGBA hex color code
std::optional<Color> parseColorCode(std::u8string_view sv);
// Attempt to match string with predefined color names
std::optional<Color> parsePredefinedColor(std::u8string_view sv);

// Decode C-string of Base64 encoded data into a string of binary data
std::string decodeBase64(std::u8string_view base64data);

namespace detail {

// SFINAE logic for checking named enums
template <typename, typename = void>
struct has_names: std::false_type {};

template <typename T>
struct has_names<T, std::void_t<decltype(T::NAMES)>>: std::true_type {};

template <typename T>
constexpr bool has_names_v = has_names<T>::value;


template <typename, typename = void>
struct has_value_enum: std::false_type {};

template <typename T>
struct has_value_enum<T, std::void_t<typename T::Value>> {
    static constexpr bool value = std::is_enum_v<typename T::Value>;
};

template <typename T>
constexpr bool has_value_enum_v = has_value_enum<T>::value;

template <typename T>
constexpr bool always_false = false;

// Check for floating-point std::from_chars support
#ifndef HAS_FLOAT_FROM_CHARS
#if defined(_GLIBCXX_RELEASE)
// libstdc++ supports floating-point from_chars since GCC 11
#define HAS_FLOAT_FROM_CHARS (_GLIBCXX_RELEASE >= 11)
#elif defined(_LIBCPP_VERSION)
// libc++ supports floating-point from chars since version 20.1.0
#define HAS_FLOAT_FROM_CHARS (_LIBCPP_VERSION >= 200100)
#elif defined(_MSC_VER)
// MSVC STL supports float from_chars since VS 2017 15.7
#define HAS_FLOAT_FROM_CHARS (_MSC_VER >= 1914)
#else
// Unknown standard library
#define HAS_FLOAT_FROM_CHARS 0
#endif
#endif  // HAS_FLOAT_FROM_CHARS

// Exception type for incompletely parsed attributes
template <typename T>
class IncompleteParseError: public std::runtime_error {
public:
    /**
     * Creates an exception for incompletely parsed data
     * @param value The value that could be parsed from the input
     */
    IncompleteParseError(T value): std::runtime_error("Parsing did not consume full input"), value_(std::move(value)) {}

    // Retrieve the partially parsed value
    const T& value() const noexcept { return value_; }

private:
    T value_;
};

// Parse named enums
template <typename T>
T parse_enum(std::u8string_view sv) {
    static_assert(has_names_v<T>, "T must define a static T::NAMES array to perform lookup");
    static_assert(has_value_enum_v<T>, "T must define an underlying enum type T::Value");

    // Look up value in names array
    const auto it = std::find(T::NAMES.begin(), T::NAMES.end(), sv);
    if (it == T::NAMES.end()) {
        throw std::domain_error("unknown value");
    }

    return static_cast<typename T::Value>(std::distance(T::NAMES.begin(), it));
}

// Parse numeric types
template <typename T>
T parse_numeric(std::u8string_view sv) {
    static_assert(std::is_arithmetic_v<T>, "T must be numeric");

    T value{};
    if constexpr (std::is_integral_v<T> || HAS_FLOAT_FROM_CHARS) {
        auto [ptr, ec] = std::from_chars(char_cast(sv.begin()), char_cast(sv.end()), value);
        if (ec != std::errc{}) {
            throw std::domain_error(std::make_error_condition(ec).message());
        }
        if (ptr != char_cast(sv.end())) {
            throw IncompleteParseError(value);
        }
    } else {
        // Fallback for missing floating point from_chars implementation
        // Attributes originate from GMarkup and are nul-terminated
        xoj_assert(*sv.end() == '\0');
        char* end = nullptr;
        value = static_cast<T>(g_ascii_strtod(char_cast(sv.begin()), &end));
        if (end != char_cast(sv.end())) {
            if (end == char_cast(sv.begin())) {
                throw std::domain_error("g_ascii_strtod failed");
            } else {
                throw IncompleteParseError(value);
            }
        }
    }
    return value;
}
};  // namespace detail

};  // namespace XmlParserHelper


// stream operator overloads for LineStyle

std::ostream& operator<<(std::ostream& stream, const LineStyle& style);
std::istream& operator>>(std::istream& stream, LineStyle& style);


// implementations of template functions

template <typename T>
auto XmlParserHelper::getAttrib(std::u8string_view name, const AttributeMap& attributeMap) -> std::optional<T> {
    auto optionalSV = attributeMap[name];  // mildly expensive operation: string search in array.
                                           // Use the operator[] only once and store the result.
    if (optionalSV) {
        try {
            // Choose appropriate parsing strategy
            if constexpr (std::is_constructible_v<T, std::u8string_view>) {
                return T{*optionalSV};  // Type is directly constructible from an std::u8string_view, e.g. fs::path
            } else if constexpr (detail::has_names_v<T> && detail::has_value_enum_v<T>) {
                return detail::parse_enum<T>(*optionalSV);
            } else if constexpr (std::is_arithmetic_v<T>) {
                return detail::parse_numeric<T>(*optionalSV);
            } else {
                static_assert(detail::always_false<T>, "No parser defined for this type");
            }
        } catch (const std::domain_error& e) {
            g_warning("XML parser: Attribute \"" SV_FMT "\" could not be parsed as %s: %s. The value is \"" SV_FMT "\"",
                      U8SV_ARG(name), Util::demangledTypeName<T>().c_str(), e.what(), U8SV_ARG(*optionalSV));
            return std::nullopt;
        } catch (const detail::IncompleteParseError<T>& e) {
            g_warning("XML parser: Attribute \"" SV_FMT "\" was not entirely parsed as %s. The value is \"" SV_FMT "\"",
                      U8SV_ARG(name), Util::demangledTypeName<T>().c_str(), U8SV_ARG(*optionalSV));
            return e.value();
        }
    } else {
        return std::nullopt;
    }
}

template <typename T>
auto XmlParserHelper::getAttribMandatory(std::u8string_view name, const AttributeMap& attributeMap,
                                         const T& defaultValue, bool warn) -> T {
    auto optionalInt = getAttrib<T>(name, attributeMap);
    if (optionalInt) {
        return *optionalInt;
    } else {
        if (warn) {
            std::u8string defaultValueStr;
            if constexpr (std::same_as<T, std::u8string_view>) {
                defaultValueStr = defaultValue;
            } else if constexpr (detail::has_names_v<T> && detail::has_value_enum_v<T>) {
                xoj_assert(defaultValue < T::NAMES.size());
                defaultValueStr = T::NAMES[defaultValue];
            } else {
                auto stream = serdes_stream<std::ostringstream>();
                stream << defaultValue;
                defaultValueStr = xoj::util::utf8(stream.str()).str();
            }
            g_warning("XML parser: Mandatory attribute \"" SV_FMT "\" not found. Using default value \"%s\"",
                      U8SV_ARG(name), char_cast(defaultValueStr.c_str()));
        }
        return defaultValue;
    }
}
