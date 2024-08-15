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

#include <cstring>
#include <istream>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <unordered_map>

#include <glib.h>
#include <libxml/xmlreader.h>

#include "util/Color.h"
#include "util/EnumIndexedArray.h"
#include "util/Util.h"
#include "util/serdesstream.h"

class LineStyle;
class XmlParser;


namespace XmlParserHelper {

// generic templates
template <typename T>
std::optional<T> getAttrib(const char* name, xmlTextReaderPtr reader);
template <typename T>
T getAttribMandatory(const char* name, xmlTextReaderPtr reader, const T& defaultValue = {}, bool warn = true);
// specializations
template <>
std::optional<std::string> getAttrib<std::string>(const char* name, xmlTextReaderPtr reader);
template <>
std::string getAttribMandatory<std::string>(const char* name, xmlTextReaderPtr reader,
                                            const std::string& defaultValue, bool warn);

// "color" attribute
Color getAttribColorMandatory(xmlTextReaderPtr reader, const Color& defaultValue, bool bg = false);
// Attempt to match string with background-specific color "translations"
std::optional<Color> parseBgColor(const std::string& str);
// Parse str as a RGBA hex color code
std::optional<Color> parseColorCode(const std::string& str);
// Attempt to match string with predefined color names
std::optional<Color> parsePredefinedColor(const std::string& str);

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
auto XmlParserHelper::getAttrib(const char* name, xmlTextReaderPtr reader) -> std::optional<T> {
    if (xmlTextReaderMoveToFirstAttribute(reader) == 1) {
        do {
            const auto attrName = reinterpret_cast<const char*>(xmlTextReaderConstName(reader));
            if (!strcmp(name, attrName)) {
                const auto value_str = reinterpret_cast<const char*>(xmlTextReaderConstValue(reader));
                auto stream = serdes_stream<std::istringstream>(value_str);
                T value{};
                stream >> value;
                if (!stream.fail()) {
                    if (!stream.eof()) {
                        g_warning("XML parser: Attribute \"%s\" was not entirely parsed", name);
                    }
                    return value;
                } else {
                    g_warning("XML parser: Attribute \"%s\" could not be parsed as %s, the value is \"%s\"",
                              name, Util::demangledTypeName(value).c_str(), value_str);
                    return {};
                }
            }
        } while (xmlTextReaderMoveToNextAttribute(reader));
    }

    return {};
}

template <typename T>
auto XmlParserHelper::getAttribMandatory(const char* name, xmlTextReaderPtr reader, const T& defaultValue,
                                         bool warn) -> T {
    auto optionalInt = getAttrib<T>(name, reader);
    if (optionalInt) {
        return *optionalInt;
    } else {
        if (warn) {
            auto stream = serdes_stream<std::ostringstream>();
            stream << defaultValue;
            g_warning("XML parser: Mandatory attribute \"%s\" not found. Using default value \"%s\"", name,
                      stream.str().c_str());
        }
        return defaultValue;
    }
}
