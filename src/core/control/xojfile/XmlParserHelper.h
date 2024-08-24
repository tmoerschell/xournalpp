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

#include <algorithm>
#include <istream>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>

#include <glib.h>
#include <libxml/xmlreader.h>

#include "util/Color.h"
#include "util/EnumIndexedArray.h"
#include "util/Util.h"
#include "util/serdesstream.h"

class LineStyle;
class XmlParser;


namespace XmlParserHelper {

using AttributeMap = std::vector<std::pair<std::string_view, std::string_view>>;

// generic templates
template <typename T>
std::optional<T> getAttrib(std::string_view name, const AttributeMap& attributeMap);
template <typename T>
T getAttribMandatory(std::string_view name, const AttributeMap& attributeMap, const T& defaultValue = {},
                     bool warn = true);
// specializations
template <>
std::optional<std::string_view> getAttrib<std::string_view>(std::string_view name, const AttributeMap& attributeMap);
template <>
std::string_view getAttribMandatory<std::string_view>(std::string_view name, const AttributeMap& attributeMap,
                                                      const std::string_view& defaultValue, bool warn);

// "color" attribute
Color getAttribColorMandatory(const AttributeMap& attributeMap, const Color& defaultValue, bool bg = false);
// Attempt to match string with background-specific color "translations"
std::optional<Color> parseBgColor(std::string_view str);
// Parse str as a RGBA hex color code
std::optional<Color> parseColorCode(std::string_view str);
// Attempt to match string with predefined color names
std::optional<Color> parsePredefinedColor(std::string_view str);

// Decode C-string of Base64 encoded data into a string of binary data
std::string decodeBase64(const char* base64data);

// custom types for easier parsing using stream operators
enum class Domain : size_t { ABSOLUTE, ATTACH, CLONE, ENUMERATOR_COUNT };
constexpr EnumIndexedArray<const char*, Domain> DOMAIN_NAMES = {"absolute", "attach", "clone"};

};  // namespace XmlParserHelper


// stream operator overloads for LoadHandler::Domain

std::ostream& operator<<(std::ostream& stream, const XmlParserHelper::Domain domain);
std::istream& operator>>(std::istream& stream, XmlParserHelper::Domain& domain);

// stream operator overloads for LineStyle

std::ostream& operator<<(std::ostream& stream, const LineStyle& style);
std::istream& operator>>(std::istream& stream, LineStyle& style);


// implementations of template functions

template <typename T>
auto XmlParserHelper::getAttrib(std::string_view name, const AttributeMap& attributeMap) -> std::optional<T> {
    auto it = std::find_if(attributeMap.begin(), attributeMap.end(),
                           [&name](const std::pair<std::string_view, std::string_view>& p) { return p.first == name; });
    if (it != attributeMap.end()) {
        auto stream = serdes_stream<std::istringstream>(std::string(it->second));
        T value{};
        stream >> value;
        if (!stream.fail()) {
            if (!stream.eof()) {
                g_warning("XML parser: Attribute \"%s\" was not entirely parsed", std::string(name).c_str());
            }
            return value;
        } else {
            g_warning("XML parser: Attribute \"%s\" could not be parsed as %s, the value is \"%s\"",
                      std::string(name).c_str(), Util::demangledTypeName(value).c_str(),
                      std::string(it->second).c_str());
            return {};
        }
    } else {
        return {};
    }

    return {};
}

template <typename T>
auto XmlParserHelper::getAttribMandatory(std::string_view name, const AttributeMap& attributeMap, const T& defaultValue,
                                         bool warn) -> T {
    auto optionalInt = getAttrib<T>(name, attributeMap);
    if (optionalInt) {
        return *optionalInt;
    } else {
        if (warn) {
            auto stream = serdes_stream<std::ostringstream>();
            stream << defaultValue;
            g_warning("XML parser: Mandatory attribute \"%s\" not found. Using default value \"%s\"",
                      std::string(name).c_str(), stream.str().c_str());
        }
        return defaultValue;
    }
}
