/*
 * Xournal++
 *
 * Enum and contexpr names for the tags used in .xoj and .xopp files
 *
 * @author Xournal++ Team
 * https://github.com/xournalpp/xournalpp
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <string_view>

#include "util/EnumIndexedArray.h"

namespace xoj::xml_tags {

enum class Type : size_t {
    UNKNOWN,
    XOURNAL,
    MRWRITER,
    TITLE,
    PREVIEW,
    PAGE,
    AUDIO,
    BACKGROUND,
    LAYER,
    TIMESTAMP,
    STROKE,
    TEXT,
    IMAGE,
    TEXIMAGE,
    ATTACHMENT,

    // This must be the last value
    ENUMERATOR_COUNT
};

using namespace std::literals::string_view_literals;
// Names corresponding to the xoj::xml_tags::Type enum. They must imperatively correspond to the order of the enum!
constexpr EnumIndexedArray<std::string_view, Type> NAMES = {
        "[unknown]"sv, "xournal"sv,   "MrWriter"sv, "title"sv, "preview"sv, "page"sv,     "audio"sv,     "background"sv,
        "layer"sv,     "timestamp"sv, "stroke"sv,   "text"sv,  "image"sv,   "teximage"sv, "attachment"sv};

}  // namespace xoj::xml_tags
