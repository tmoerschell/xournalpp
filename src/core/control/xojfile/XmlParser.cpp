#include "control/xojfile/XmlParser.h"

#include <functional>     // for function, bind
#include <stdexcept>      // for runtime_error
#include <string>         // for stod, string
#include <string_view>    // for string_view
#include <unordered_map>  // for unordered_map
#include <utility>        // for move
#include <vector>         // for vector

#include <glib.h>              // for g_warning
#include <libxml/parser.h>     // for XML_PARSE_RECOVER
#include <libxml/tree.h>       // for oxml::NodeType::OPENING, X...
#include <libxml/xmlerror.h>   // for xmlError, xmlGetLas...
#include <libxml/xmlreader.h>  // for xmlReaderForIO, xml...

#include "control/pagetype/PageTypeHandler.h"  // for PageTypeHandler
#include "control/xojfile/InputStream.h"       // for InputStream
#include "control/xojfile/LoadHandler.h"       // for LoadHandler
#include "control/xojfile/XmlAttrs.h"          // for XmlAttrs
#include "control/xojfile/XmlParserHelper.h"   // for getAttrib...
#include "control/xojfile/XmlTags.h"           // for XmlTags
#include "model/PageType.h"                    // for PageType
#include "model/Point.h"                       // for Point
#include "model/Stroke.h"                      // for StrokeTool, StrokeCapStyle
#include "util/Assert.h"                       // for xoj_assert
#include "util/Color.h"                        // for Color
#include "util/EnumIndexedArray.h"             // for EnumIndexedArray
#include "util/i18n.h"                         // for FS, _F
#include "util/safe_casts.h"                   // for as_unsigned

#include "config-debug.h"  // for DEBUG_XML_PARSER
#include "filesystem.h"    // for path

#ifdef DEBUG_XML_PARSER
#include <iostream>  // for cout

#define DEBUG_PARSER(f) f
#else
#define DEBUG_PARSER(f)
#endif


static constexpr auto& TAG_NAMES = xoj::xml_tags::NAMES;
using TagType = xoj::xml_tags::Type;

using namespace std::placeholders;


template <typename To, typename From>
static std::unique_ptr<To> static_uptr_cast(std::unique_ptr<From> old) {
    return std::unique_ptr<To>(static_cast<To*>(old.release()));
}


static auto readCallback(InputStream& input, char* buffer, int len) -> int {
    return input.read(buffer, as_unsigned(len));
}

// called by the oxml::Reader destructor
static auto closeCallback(InputStream& input) -> int {
    input.close();
    return 0;
}


XmlParser::XmlParser(InputStream& input, LoadHandler* handler):
        reader(std::bind(readCallback, std::ref(input), _1, _2), std::bind(closeCallback, std::ref(input))),
        handler(handler),
        pdfFilenameParsed(false),
        tempTimestamp(0) {}

auto XmlParser::parse(const std::function<std::unique_ptr<oxml::Node>(XmlParser*, std::unique_ptr<oxml::Node>)>&
                              processNodeFunction) -> std::unique_ptr<oxml::Node> {
    auto bnode = reader.readNode();
    xoj_assert(bnode);
    size_t startDepth{};
    if (bnode->getType() != oxml::NodeType::OPENING) {
        // The first node isn't an opening node.
        return bnode;
    } else {
        startDepth = this->hierarchy.size();
    }

    while (bnode->getType() != oxml::NodeType::END) {
        auto depth = hierarchy.size();
        if (bnode->getType() == oxml::NodeType::CLOSING) {
            depth--;
        }
        if (depth >= startDepth) {
            DEBUG_PARSER(debugPrintNode(bnode.get()));
            // The node processing functions always perform a read operation at
            // the end. Some do because they call parse(), so all must comply.
            bnode = processNodeFunction(this, std::move(bnode));
        } else {
            // We reached a node at a lower depth as our start depth.
            return bnode;
        }
    }

    return bnode;
}


auto XmlParser::processRootNode(std::unique_ptr<oxml::Node> bnode) -> std::unique_ptr<oxml::Node> {
    if (this->handler->isParsingComplete()) {
        g_warning("Extraneous data after end of document: ignoring node");
    }
    const auto nodeType = bnode->getType();
    switch (nodeType) {
        case oxml::NodeType::OPENING: {
            xoj_assert(this->hierarchy.empty());

            auto [node, tagType] = openTag(std::move(bnode));

            // The root tag should not be empty
            if (node->isEmpty()) {
                throw std::runtime_error(_("Error parsing XML file: the document root tag is empty"));
            }

            switch (tagType) {
                case TagType::XOURNAL:
                    parseXournalTag(node->getAttributes());
                    break;
                case TagType::MRWRITER:
                    parseMrWriterTag(node->getAttributes());
                    break;
                default:
                    // Print a warning, but attempt parsing the document anyway
                    g_warning("XML parser: Unexpected root tag: \"%s\"", reader.nullTerminate(node->getName()));
                    break;
            }

            return parse(&XmlParser::processDocumentChildNode);
        }
        case oxml::NodeType::CLOSING: {
            // Parsing is done: we have arrived at the closing node
            this->handler->finalizeDocument();
            closeTag(std::move(bnode));
            return reader.readNode();
        }
        default:
            g_warning("XML parser: Ignoring unexpected %s node at document root", oxml::NODE_TYPE_NAMES[nodeType]);
            return reader.readNode();
    }
}

auto XmlParser::processDocumentChildNode(std::unique_ptr<oxml::Node> bnode) -> std::unique_ptr<oxml::Node> {
    xoj_assert(!this->hierarchy.empty());

    const auto nodeType = bnode->getType();
    switch (nodeType) {
        case oxml::NodeType::OPENING: {
            xoj_assert(this->hierarchy.top() == TagType::XOURNAL || this->hierarchy.top() == TagType::MRWRITER ||
                       this->hierarchy.top() == TagType::UNKNOWN);

            auto [node, tagType] = openTag(std::move(bnode));

            switch (tagType) {
                case TagType::TITLE:
                case TagType::PREVIEW:
                    // Ignore these tags, we don't need them.
                    break;
                case TagType::PAGE: {
                    // When parsing the page, the reader will move to the attributes,
                    // which are never empty. Check for empty page first.
                    const bool isEmptyPage = node->isEmpty();
                    parsePageTag(node->getAttributes());
                    if (isEmptyPage) {
                        g_warning("XML parser: Found empty page");
                        this->handler->finalizePage();
                        break;
                    }
                    return parse(&XmlParser::processPageChildNode);
                }
                case TagType::AUDIO:
                    parseAudioTag(node->getAttributes());
                    break;
                default:
                    g_warning("XML parser: Ignoring unexpected tag in document: \"%s\"",
                              reader.nullTerminate(node->getName()));
                    break;
            }

            return reader.readNode();
        }
        case oxml::NodeType::TEXT: {
            // ignore text from tags above (title or preview), print a warning otherwise
            if (this->hierarchy.top() != TagType::TITLE && this->hierarchy.top() != TagType::PREVIEW) {
                g_warning("XML parser: Ignoring unexpected text under tag \"%s\"", TAG_NAMES[this->hierarchy.top()]);
            }
            return reader.readNode();
        }
        case oxml::NodeType::CLOSING: {
            if (this->hierarchy.top() == TagType::PAGE) {
                this->handler->finalizePage();
            }
            closeTag(std::move(bnode));
            return reader.readNode();
        }
        default:
            g_warning("XML parser: Ignoring unexpected %s node in document", oxml::NODE_TYPE_NAMES[nodeType]);
            return reader.readNode();
    }
}

auto XmlParser::processPageChildNode(std::unique_ptr<oxml::Node> bnode) -> std::unique_ptr<oxml::Node> {
    xoj_assert(!this->hierarchy.empty());

    const auto nodeType = bnode->getType();
    switch (nodeType) {
        case oxml::NodeType::OPENING: {
            xoj_assert(this->hierarchy.top() == TagType::PAGE || this->hierarchy.top() == TagType::UNKNOWN);

            auto [node, tagType] = openTag(std::move(bnode));

            switch (tagType) {
                case TagType::BACKGROUND:
                    parseBackgroundTag(node->getAttributes());
                    break;
                case TagType::LAYER: {
                    const bool isEmptyLayer = node->isEmpty();
                    parseLayerTag(node->getAttributes());
                    if (isEmptyLayer) {
                        // Don't warn: it's normal to have an empty layer in an empty page
                        this->handler->finalizeLayer();
                        break;
                    }
                    return parse(&XmlParser::processLayerChildNode);
                }
                default:
                    g_warning("XML parser: Ignoring unexpected tag in page: \"%s\"",
                              reader.nullTerminate(node->getName()));
                    break;
            }
            return reader.readNode();
        }
        case oxml::NodeType::CLOSING:
            if (this->hierarchy.top() == TagType::LAYER) {
                this->handler->finalizeLayer();
            }
            closeTag(std::move(bnode));
            return reader.readNode();
        default:
            g_warning("XML parser: Ignoring unexpected %s node in page", oxml::NODE_TYPE_NAMES[nodeType]);
            return reader.readNode();
    }
}

auto XmlParser::processLayerChildNode(std::unique_ptr<oxml::Node> bnode) -> std::unique_ptr<oxml::Node> {
    xoj_assert(!this->hierarchy.empty());

    const auto nodeType = bnode->getType();
    switch (nodeType) {
        case oxml::NodeType::OPENING: {
            xoj_assert(this->hierarchy.top() == TagType::LAYER || this->hierarchy.top() == TagType::UNKNOWN);

            auto [node, tagType] = openTag(std::move(bnode));

            switch (tagType) {
                case TagType::TIMESTAMP:
                    parseTimestampTag(node->getAttributes());
                    break;
                case TagType::STROKE: {
                    const bool isEmptyStroke = node->isEmpty();
                    parseStrokeTag(node->getAttributes());
                    if (isEmptyStroke) {
                        g_warning("XML parser: Found empty stroke");
                        this->handler->finalizeStroke();
                    }
                    break;
                }
                case TagType::TEXT: {
                    const bool isEmptyText = node->isEmpty();
                    parseTextTag(node->getAttributes());
                    if (isEmptyText) {
                        g_warning("XML parser: Found empty text");
                        this->handler->finalizeText();
                    }
                    break;
                }
                case TagType::IMAGE: {
                    const bool isEmptyImage = node->isEmpty();
                    parseImageTag(node->getAttributes());
                    if (isEmptyImage) {
                        g_warning("XML parser: Found empty image");
                        this->handler->finalizeImage();
                        break;
                    }
                    // An image may have an attachment. If it doesn't, parse()
                    // will return right away
                    return parse(&XmlParser::processAttachment);
                }
                case TagType::TEXIMAGE: {
                    const bool isEmptyTexImage = node->isEmpty();
                    parseTexImageTag(node->getAttributes());
                    if (isEmptyTexImage) {
                        g_warning("XML parser: Found empty TEX image");
                        this->handler->finalizeTexImage();
                    }
                    // An image may have an attachment. If it doesn't, parse()
                    // will return right away
                    return parse(&XmlParser::processAttachment);
                }
                default:
                    g_warning("XML parser: Ignoring unexpected tag in layer: \"%s\"",
                              reader.nullTerminate(node->getName()));
                    break;
            }
            return reader.readNode();
        }
        case oxml::NodeType::TEXT: {
            auto node = static_uptr_cast<oxml::TextNode>(std::move(bnode));
            switch (this->hierarchy.top()) {
                case TagType::STROKE:
                    parseStrokeText(node->getText());
                    break;
                case TagType::TEXT:
                    parseTextText(node->getText());
                    break;
                case TagType::IMAGE:
                    parseImageText(node->getText());
                    break;
                case TagType::TEXIMAGE:
                    parseTexImageText(node->getText());
                    break;
                default:
                    g_warning("XML parser: Ignoring unexpected text under tag \"%s\"",
                              TAG_NAMES[this->hierarchy.top()]);
                    break;
            }
            return reader.readNode();
        }
        case oxml::NodeType::CLOSING: {
            switch (this->hierarchy.top()) {
                case TagType::STROKE:
                    this->handler->finalizeStroke();
                    break;
                case TagType::TEXT:
                    this->handler->finalizeText();
                    break;
                case TagType::IMAGE:
                    this->handler->finalizeImage();
                    break;
                case TagType::TEXIMAGE:
                    this->handler->finalizeTexImage();
                    break;
                default:
                    break;
            }
            closeTag(std::move(bnode));
            return reader.readNode();
        }
        default:
            g_warning("XML parser: Ignoring unexpected %s node in layer", oxml::NODE_TYPE_NAMES[nodeType]);
            return reader.readNode();
    }
}

auto XmlParser::processAttachment(std::unique_ptr<oxml::Node> bnode) -> std::unique_ptr<oxml::Node> {
    xoj_assert(!this->hierarchy.empty());

    const auto nodeType = bnode->getType();
    switch (nodeType) {
        case oxml::NodeType::OPENING: {
            xoj_assert(this->hierarchy.top() == TagType::IMAGE || this->hierarchy.top() == TagType::TEXIMAGE ||
                       this->hierarchy.top() == TagType::UNKNOWN);

            auto [node, tagType] = openTag(std::move(bnode));

            switch (tagType) {
                case TagType::ATTACHMENT:
                    parseAttachmentTag(node->getAttributes());
                    break;
                default:
                    g_warning("XML parser: Ignoring unexpected tag in image or TEX image: \"%s\"",
                              reader.nullTerminate(node->getName()));
                    break;
            }
            return reader.readNode();
        }
        case oxml::NodeType::CLOSING:
            closeTag(std::move(bnode));
            return reader.readNode();
        default:
            g_warning("XML parser: Ignoring unexpected %s node in image or TEX image", oxml::NODE_TYPE_NAMES[nodeType]);
            return reader.readNode();
    }
}


void XmlParser::parseXournalTag(const XmlParserHelper::AttributeMap& attributes) {
    std::string creator;
    const auto optCreator = XmlParserHelper::getAttrib<std::string_view>(xoj::xml_attrs::CREATOR_STR, attributes);
    if (optCreator) {
        creator = *optCreator;
    } else {
        // Compatibility: the creator attribute exists since 7017b71. Before that, only a version string was written
        const auto optVersion = XmlParserHelper::getAttrib<std::string_view>(xoj::xml_attrs::VERSION_STR, attributes);
        if (optVersion) {
            creator = "Xournal " + std::string(*optVersion);
        } else {
            creator = "Unknown";
        }
    }

    const auto fileversion = XmlParserHelper::getAttribMandatory<int>(xoj::xml_attrs::FILEVERSION_STR, attributes, 1);

    this->handler->addXournal(std::move(creator), fileversion);
}

void XmlParser::parseMrWriterTag(const XmlParserHelper::AttributeMap& attributes) {
    std::string creator;
    auto optVersion = XmlParserHelper::getAttrib<std::string_view>(xoj::xml_attrs::VERSION_STR, attributes);
    if (optVersion) {
        creator = "MrWriter " + std::string(*optVersion);
    } else {
        creator = "Unknown";
    }

    this->handler->addMrWriter(std::move(creator));
}

void XmlParser::parsePageTag(const XmlParserHelper::AttributeMap& attributes) {
    const auto width = XmlParserHelper::getAttribMandatory<double>(xoj::xml_attrs::WIDTH_STR, attributes);
    const auto height = XmlParserHelper::getAttribMandatory<double>(xoj::xml_attrs::HEIGHT_STR, attributes);

    this->handler->addPage(width, height);
}

void XmlParser::parseAudioTag(const XmlParserHelper::AttributeMap& attributes) {
    auto filename = XmlParserHelper::getAttribMandatory<fs::path>(xoj::xml_attrs::AUDIO_FILENAME_STR, attributes);

    this->handler->addAudioAttachment(std::move(filename));
}

void XmlParser::parseBackgroundTag(const XmlParserHelper::AttributeMap& attributes) {
    auto name = XmlParserHelper::getAttrib<std::string_view>(xoj::xml_attrs::NAME_STR, attributes);
    const auto optType = XmlParserHelper::getAttrib<std::string_view>(xoj::xml_attrs::TYPE_STR, attributes);

    if (name) {
        this->handler->setBgName(std::string(*name));
    }
    if (optType) {
        if (*optType == "solid") {
            parseBgSolid(attributes);
        } else if (*optType == "pixmap") {
            parseBgPixmap(attributes);
        } else if (*optType == "pdf") {
            parseBgPdf(attributes);
        } else {
            g_warning("XML parser: Ignoring unknown background type \"%s\"", std::string(*optType).c_str());
        }
    } else {
        // It's not possible to assume a default type as other attributes have to be set in fuction of this. Not setting
        // a background will leave the default-constructed one.
        g_warning("XML parser: Attribute \"type\" not found in background tag. Ignoring tag.");
    }
}

void XmlParser::parseBgSolid(const XmlParserHelper::AttributeMap& attributes) {
    const auto optStyle = XmlParserHelper::getAttrib<std::string_view>(xoj::xml_attrs::STYLE_STR, attributes);
    const auto config =
            XmlParserHelper::getAttribMandatory<std::string_view>(xoj::xml_attrs::CONFIG_STR, attributes, "", false);
    PageType bg;
    if (optStyle) {
        bg.format = PageTypeHandler::getPageTypeFormatForString(std::string(*optStyle));
    }
    bg.config = config;

    const auto color = XmlParserHelper::getAttribColorMandatory(attributes, Colors::white, true);

    this->handler->setBgSolid(bg, color);
}

void XmlParser::parseBgPixmap(const XmlParserHelper::AttributeMap& attributes) {
    const auto domain = XmlParserHelper::getAttribMandatory<XmlParserHelper::Domain>(
            xoj::xml_attrs::DOMAIN_STR, attributes, XmlParserHelper::Domain::ABSOLUTE);

    if (domain != XmlParserHelper::Domain::CLONE) {
        const fs::path filename =
                XmlParserHelper::getAttribMandatory<std::string_view>(xoj::xml_attrs::FILENAME_STR, attributes);
        this->handler->setBgPixmap(domain == XmlParserHelper::Domain::ATTACH, filename);
    } else {
        // In case of a cloned background image, filename contains the page
        // number from which the image is cloned.
        const auto pageNr = XmlParserHelper::getAttribMandatory<size_t>(xoj::xml_attrs::FILENAME_STR, attributes);
        this->handler->setBgPixmapCloned(pageNr);
    }
}

void XmlParser::parseBgPdf(const XmlParserHelper::AttributeMap& attributes) {
    if (!this->pdfFilenameParsed) {
        auto domain = XmlParserHelper::getAttribMandatory<XmlParserHelper::Domain>(
                xoj::xml_attrs::DOMAIN_STR, attributes, XmlParserHelper::Domain::ABSOLUTE);
        if (domain == XmlParserHelper::Domain::CLONE) {
            g_warning("XML parser: Domain \"clone\" is invalid for PDF backgrounds. Using \"absolute\" instead");
            domain = XmlParserHelper::Domain::ABSOLUTE;
        }

        const fs::path filename =
                XmlParserHelper::getAttribMandatory<std::string_view>(xoj::xml_attrs::FILENAME_STR, attributes);

        if (!filename.empty()) {
            this->pdfFilenameParsed = true;
            this->handler->loadBgPdf(domain == XmlParserHelper::Domain::ATTACH, filename);
        } else {
            g_warning("XML parser: PDF background filename is empty");
        }
    }

    const auto pageno = XmlParserHelper::getAttribMandatory<size_t>(xoj::xml_attrs::PAGE_NUMBER_STR, attributes, 1) - 1;

    this->handler->setBgPdf(pageno);
}

void XmlParser::parseLayerTag(const XmlParserHelper::AttributeMap& attributes) {
    const auto name = XmlParserHelper::getAttrib<std::string_view>(xoj::xml_attrs::NAME_STR, attributes);

    this->handler->addLayer(name);
}

void XmlParser::parseTimestampTag(const XmlParserHelper::AttributeMap& attributes) {
    // Compatibility: timestamps for audio elements are stored in the attributes since 6b43baf

    if (!this->tempFilename.empty()) {
        g_warning("XML parser: Discarding unused audio timestamp element. Filename: %s",
                  this->tempFilename.u8string().c_str());
    }

    this->tempFilename =
            XmlParserHelper::getAttribMandatory<std::string_view>(xoj::xml_attrs::AUDIO_FILENAME_STR, attributes);
    this->tempTimestamp = XmlParserHelper::getAttribMandatory<size_t>(xoj::xml_attrs::TIMESTAMP_STR, attributes);
}

void XmlParser::parseStrokeTag(const XmlParserHelper::AttributeMap& attributes) {
    // tool
    const auto tool =
            XmlParserHelper::getAttribMandatory<StrokeTool>(xoj::xml_attrs::TOOL_STR, attributes, StrokeTool::PEN);
    // color
    const auto color = XmlParserHelper::getAttribColorMandatory(attributes, Colors::black);

    // width
    auto widthStr = XmlParserHelper::getAttribMandatory<std::string_view>(xoj::xml_attrs::WIDTH_STR, attributes, "1");
    // Use g_ascii_strtod instead of streams beacuse it is about twice as fast
    reader.nullTerminate(widthStr);
    const char* itPtr = widthStr.data();
    char* endPtr = nullptr;
    const double width = g_ascii_strtod(itPtr, &endPtr);

    // pressures
    auto pressureStr = XmlParserHelper::getAttrib<std::string_view>(xoj::xml_attrs::PRESSURES_STR, attributes);
    if (pressureStr) {
        // MrWriter writes pressures in a separate field
        reader.nullTerminate(*pressureStr);
        itPtr = pressureStr->data();
    } else {
        // Xournal and Xournal++ use the width field
        itPtr = endPtr;
    }
    while (*itPtr != 0) {
        const double pressure = g_ascii_strtod(itPtr, &endPtr);
        if (endPtr == itPtr) {
            // Parsing failed
            g_warning("XML parser: A pressure point could not be parsed as double. Remaining points: \"%s\"", itPtr);
            break;
        }
        this->pressureBuffer.emplace_back(pressure);
        itPtr = endPtr;
    }

    // fill
    const auto fill = XmlParserHelper::getAttribMandatory<int>(xoj::xml_attrs::FILL_STR, attributes, -1, false);

    // cap stype
    const auto capStyle = XmlParserHelper::getAttribMandatory<StrokeCapStyle>(xoj::xml_attrs::CAPSTYLE_STR, attributes,
                                                                              StrokeCapStyle::ROUND, false);

    // line style
    const auto lineStyle = XmlParserHelper::getAttrib<LineStyle>(xoj::xml_attrs::STYLE_STR, attributes);

    // audio filename and timestamp
    const auto optFilename =
            XmlParserHelper::getAttrib<std::string_view>(xoj::xml_attrs::AUDIO_FILENAME_STR, attributes);
    if (optFilename && !optFilename->empty()) {
        if (!this->tempFilename.empty()) {
            g_warning("XML parser: Discarding audio timestamp element, because stroke tag contains \"fn\" attribute");
        }
        this->tempFilename = *optFilename;
        this->tempTimestamp =
                XmlParserHelper::getAttribMandatory<size_t>(xoj::xml_attrs::TIMESTAMP_STR, attributes, 0UL);
    }

    // forward data to handler
    this->handler->addStroke(tool, color, width, fill, capStyle, lineStyle, std::move(this->tempFilename),
                             this->tempTimestamp);

    // Reset timestamp, filename was already moved from
    this->tempTimestamp = 0;
}

void XmlParser::parseStrokeText(std::string_view text) {
    std::vector<Point> pointVector;
    pointVector.reserve(this->pressureBuffer.size());

    // Use g_ascii_strtod instead of streams beacuse it is about twice as fast
    const char* itPtr = text.data();
    char* endPtr = nullptr;
    reader.nullTerminate(text);
    while (*itPtr != 0) {
        const double x = g_ascii_strtod(itPtr, &endPtr);
        itPtr = endPtr;
        // Note: should the first call to g_ascii_strtod have failed, the second one will be given the same input
        //       and fail in the same way. We only need to check for an error once.
        const double y = g_ascii_strtod(itPtr, &endPtr);
        if (endPtr == itPtr) {
            // Parsing failed
            g_warning("XML parser: A stroke coordinate could not be parsed as double. Remaining data: \"%s\"", itPtr);
            break;
        }
        pointVector.emplace_back(x, y);
        itPtr = endPtr;
    }

    this->handler->setStrokePoints(std::move(pointVector), std::move(this->pressureBuffer));
}

void XmlParser::parseTextTag(const XmlParserHelper::AttributeMap& attributes) {
    auto font = XmlParserHelper::getAttribMandatory<std::string_view>(xoj::xml_attrs::FONT_STR, attributes, "Sans");
    const auto size = XmlParserHelper::getAttribMandatory<double>(xoj::xml_attrs::SIZE_STR, attributes, 12);
    const auto x = XmlParserHelper::getAttribMandatory<double>(xoj::xml_attrs::X_COORD_STR, attributes);
    const auto y = XmlParserHelper::getAttribMandatory<double>(xoj::xml_attrs::Y_COORD_STR, attributes);
    const auto color = XmlParserHelper::getAttribColorMandatory(attributes, Colors::black);

    // audio filename and timestamp
    const auto optFilename =
            XmlParserHelper::getAttrib<std::string_view>(xoj::xml_attrs::AUDIO_FILENAME_STR, attributes);
    if (optFilename && !optFilename->empty()) {
        if (!this->tempFilename.empty()) {
            g_warning("XML parser: Discarding audio timestamp element, because text tag contains \"fn\" attribute");
        }
        this->tempFilename = *optFilename;
        this->tempTimestamp =
                XmlParserHelper::getAttribMandatory<size_t>(xoj::xml_attrs::TIMESTAMP_STR, attributes, 0UL);
    }

    this->handler->addText(std::string(font), size, x, y, color, std::move(tempFilename), tempTimestamp);

    this->tempTimestamp = 0;
}

void XmlParser::parseTextText(std::string_view text) { this->handler->setTextContents(std::string(text)); }

void XmlParser::parseImageTag(const XmlParserHelper::AttributeMap& attributes) {
    const auto left = XmlParserHelper::getAttribMandatory<double>(xoj::xml_attrs::LEFT_POS_STR, attributes);
    const auto top = XmlParserHelper::getAttribMandatory<double>(xoj::xml_attrs::TOP_POS_STR, attributes);
    const auto right = XmlParserHelper::getAttribMandatory<double>(xoj::xml_attrs::RIGHT_POS_STR, attributes);
    const auto bottom = XmlParserHelper::getAttribMandatory<double>(xoj::xml_attrs::BOTTOM_POS_STR, attributes);

    this->handler->addImage(left, top, right, bottom);
}

void XmlParser::parseImageText(std::string_view text) {
    reader.nullTerminate(text);
    std::string imageData = XmlParserHelper::decodeBase64(text.data());
    this->handler->setImageData(std::move(imageData));
}

void XmlParser::parseTexImageTag(const XmlParserHelper::AttributeMap& attributes) {
    const auto left = XmlParserHelper::getAttribMandatory<double>(xoj::xml_attrs::LEFT_POS_STR, attributes);
    const auto top = XmlParserHelper::getAttribMandatory<double>(xoj::xml_attrs::TOP_POS_STR, attributes);
    const auto right = XmlParserHelper::getAttribMandatory<double>(xoj::xml_attrs::RIGHT_POS_STR, attributes);
    const auto bottom = XmlParserHelper::getAttribMandatory<double>(xoj::xml_attrs::BOTTOM_POS_STR, attributes);

    auto text = XmlParserHelper::getAttribMandatory<std::string_view>(xoj::xml_attrs::TEXT_STR, attributes);

    // Attribute "texlength" found in eralier parsers was a workaround from 098a67b to bdd0ec2

    this->handler->addTexImage(left, top, right, bottom, std::string(text));
}

void XmlParser::parseTexImageText(std::string_view text) {
    reader.nullTerminate(text);
    std::string imageData = XmlParserHelper::decodeBase64(text.data());
    this->handler->setTexImageData(std::move(imageData));
}

void XmlParser::parseAttachmentTag(const XmlParserHelper::AttributeMap& attributes) {
    const auto path = XmlParserHelper::getAttribMandatory<fs::path>(xoj::xml_attrs::PATH_STR, attributes);

    switch (this->hierarchy.top()) {
        case TagType::IMAGE:
            this->handler->setImageAttachment(path);
            break;
        case TagType::TEXIMAGE:
            this->handler->setTexImageAttachment(path);
            break;
        default:
            break;
    }
}


auto XmlParser::openTag(std::unique_ptr<oxml::Node> bnode) -> std::pair<std::unique_ptr<oxml::OpeningNode>, TagType> {
    xoj_assert(bnode->getType() == oxml::NodeType::OPENING);
    auto node = static_uptr_cast<oxml::OpeningNode>(std::move(bnode));

    const TagType type = tagNameToType(node->getName());
    // Add a level to the hierarchy only if the element isn't "empty" (which
    // means there is no closing element)
    if (!node->isEmpty()) {
        this->hierarchy.push(type);
    }
    return std::make_pair(std::move(node), type);
}

void XmlParser::closeTag(std::unique_ptr<oxml::Node> bnode) {
    xoj_assert(bnode->getType() == oxml::NodeType::CLOSING);
    auto node = static_uptr_cast<oxml::ClosingNode>(std::move(bnode));

    const TagType type = tagNameToType(node->getName());
    // Check that the document structure is not messed up
    if (this->hierarchy.empty()) {
        throw std::runtime_error(
                FS(_F("Error parsing XML file: found closing tag \"{1}\" at document root") % node->getName()));
    }
    if (this->hierarchy.top() != type) {
        throw std::runtime_error(
                FS(_F("Error parsing XML file: closing tag \"{1}\" does not correspond to last open element \"{2}\"") %
                   TAG_NAMES[type] % TAG_NAMES[this->hierarchy.top()]));
    }

    // Go up one level in the hierarchy
    this->hierarchy.pop();
}

auto XmlParser::tagNameToType(std::string_view name) const -> TagType {
    using namespace std::literals;

    if ("MrWriter"sv == name)
        return TagType::MRWRITER;
    if ("attachment"sv == name)
        return TagType::ATTACHMENT;
    if ("audio"sv == name)
        return TagType::AUDIO;
    if ("background"sv == name)
        return TagType::BACKGROUND;
    if ("image"sv == name)
        return TagType::IMAGE;
    if ("layer"sv == name)
        return TagType::LAYER;
    if ("page"sv == name)
        return TagType::PAGE;
    if ("preview"sv == name)
        return TagType::PREVIEW;
    if ("stroke"sv == name)
        return TagType::STROKE;
    if ("teximage"sv == name)
        return TagType::TEXIMAGE;
    if ("text"sv == name)
        return TagType::TEXT;
    if ("timestamp"sv == name)
        return TagType::TIMESTAMP;
    if ("title"sv == name)
        return TagType::TITLE;
    if ("xournal"sv == name)
        return TagType::XOURNAL;
    return TagType::UNKNOWN;
}

#ifdef DEBUG_XML_PARSER
void XmlParser::debugPrintNode(oxml::Node* bnode) {
    std::cout << std::dec << std::boolalpha << "Depth: "
              << (bnode->getType() == oxml::NodeType::CLOSING ? this->hierarchy.size() - 1 : this->hierarchy.size())
              << "  Type: " << oxml::NODE_TYPE_NAMES[bnode->getType()];

    switch (bnode->getType()) {
        case oxml::NodeType::OPENING: {
            auto node = static_cast<oxml::OpeningNode*>(bnode);
            std::cout << "  Name: \"" << node->getName() << "\"  Empty: " << node->isEmpty() << '\n';
            if (!node->getAttributes().empty()) {
                for (const auto& [name, value]: node->getAttributes()) {
                    std::cout << " [" << name << "] = \"" << value << "\";";
                }
                std::cout << '\n';
            }
            break;
        }
        case oxml::NodeType::TEXT: {
            auto node = static_cast<oxml::TextNode*>(bnode);
            std::cout << "\n  Value: \"" << node->getText() << "\"\n";
            break;
        }
        case oxml::NodeType::CLOSING: {
            auto node = static_cast<oxml::ClosingNode*>(bnode);
            std::cout << "  Name: \"" << node->getName() << "\"\n";
            break;
        }
        default:
            std::cout << '\n';
            break;
    }
}
#endif
