/*
 * Xournal++
 *
 * contexpr names of the attributes used in .xoj and .xopp files
 *
 * @author Xournal++ Team
 * https://github.com/xournalpp/xournalpp
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <vector>

#include "control/xojfile/XmlTags.h"
#include "util/EnumIndexedArray.h"


// Names of attributes
namespace xoj::xml_attrs {

static const EnumIndexedArray<std::vector<const char*>, xoj::xml_tags::Type> NAMES = {
        std::vector<const char*>{},                                     // <unknown>
        std::vector<const char*>{"creator", "version", "fileversion"},  // xournal
        std::vector<const char*>{"version"},                            // MrWriter
        std::vector<const char*>{},                                     // title
        std::vector<const char*>{},                                     // preview
        std::vector<const char*>{"width", "height"},                    // page
        std::vector<const char*>{"fn"},                                 // audio
        std::vector<const char*>{"name", "type", "style", "config", "color", "domain", "filename",
                                 "pageno"},    // background
        std::vector<const char*>{"name"},      // layer
        std::vector<const char*>{"fn", "ts"},  // timestamp
        std::vector<const char*>{"tool", "pressures", "color", "wudth", "fill", "capStyle", "style", "fn",
                                 "ts"},                                           // stroke
        std::vector<const char*>{"font", "size", "x", "y", "color", "fn", "ts"},  // text
        std::vector<const char*>{"left", "top", "right", "bottom"},               // image
        std::vector<const char*>{"text", "left", "top", "right", "bottom"},       // teximage
        std::vector<const char*>{"path"},                                         // attachment
};

enum class Type : size_t {
    CREATOR,
    VERSION,
    FILEVERSION,
    NAME,
    STYLE,
    CONFIG,
    COLOR,
    DOMAIN,
    FILENAME,
    PAGE_NUMBER,
    AUDIO_FILENAME,
    AUDIO_TIMESTAMP,
    TOOL,
    PRESSURES,
    FILL,
    CAPSTYLE,
    FONT,
    SIZE,
    X_COORD,
    Y_COORD,
    LEFT_POS,
    TOP_POS,
    RIGHT_POS,
    BOTTOM_POS,
    TEXT,
    PATH,

    ENUMERATOR_COUNT
};

// xournal
constexpr auto CREATOR_STR = "creator";
constexpr auto VERSION_STR = "version";  // also in MrWriter
constexpr auto FILEVERSION_STR = "fileversion";

// page
constexpr auto WIDTH_STR = "width";  // also in stroke
constexpr auto HEIGHT_STR = "height";

// background
constexpr auto NAME_STR = "name";  // also in layer
constexpr auto TYPE_STR = "type";
constexpr auto STYLE_STR = "style";  // also in stroke
constexpr auto CONFIG_STR = "config";
constexpr auto COLOR_STR = "color";  // also in stroke and text
constexpr auto DOMAIN_STR = "domain";
constexpr auto FILENAME_STR = "filename";
constexpr auto PAGE_NUMBER_STR = "pageno";

// timestamp
constexpr auto AUDIO_FILENAME_STR = "fn";  // also in stroke, text and audio
constexpr auto TIMESTAMP_STR = "ts";       // also in stroke and text

// stroke
constexpr auto TOOL_STR = "tool";
constexpr auto PRESSURES_STR = "pressures";
constexpr auto FILL_STR = "fill";
constexpr auto CAPSTYLE_STR = "capStyle";

// text
constexpr auto FONT_STR = "font";
constexpr auto SIZE_STR = "size";
constexpr auto X_COORD_STR = "x";
constexpr auto Y_COORD_STR = "y";

// image
constexpr auto LEFT_POS_STR = "left";      // also in teximage
constexpr auto TOP_POS_STR = "top";        // also in teximage
constexpr auto RIGHT_POS_STR = "right";    // also in teximage
constexpr auto BOTTOM_POS_STR = "bottom";  // also in teximage

// teximage
constexpr auto TEXT_STR = "text";

// attachment
constexpr auto PATH_STR = "path";

}  // namespace xoj::xml_attrs
