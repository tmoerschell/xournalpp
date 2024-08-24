/*
 * Xournal++
 *
 * Parses the uncompressed XML of .xoj / .xopp documents
 *
 * @author Xournal++ Team
 * https://github.com/xournalpp/xournalpp
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <cstdlib>
#include <functional>
#include <stack>
#include <string>
#include <vector>

#include <libxml/xmlreader.h>

#include "control/xojfile/InputStream.h"
#include "control/xojfile/XmlParserHelper.h"
#include "control/xojfile/XmlTags.h"
#include "control/xojfile/oxml.h"

#include "config-debug.h"
#include "filesystem.h"

class LoadHandler;


class XmlParser {
public:
    XmlParser(InputStream& input, LoadHandler* handler);

    /**
     * @brief Parse the XML input and forward data to the handler's appropriate add*,
     * addText* and finalize* functions
     *
     * Loops over all elements at the current depth level and calls processNodeFunction
     * at each node. Returns when the current element is closed or the EOF is reached.
     * If the function returns before EOF is reached, the reader points to a not yet
     * processed closing node.
     *
     * If the first operation does not return a start element node, the function
     * exits immediately.
     *
     * @param processNodeFunction should be able to process any child nodes of
     * the current element. It should call parse() again with an appropriate
     * node processing funcion when expecting grandchildren. Otherwise, it
     * should return xmlTextReaderRead().
     *
     * @return The result of the last read operation.
     */
    std::unique_ptr<oxml::Node> parse(
            const std::function<std::unique_ptr<oxml::Node>(XmlParser*, std::unique_ptr<oxml::Node> bnode)>&
                    processNodeFunction = &XmlParser::processRootNode);

private:
    std::unique_ptr<oxml::Node> processRootNode(std::unique_ptr<oxml::Node> bnode);
    std::unique_ptr<oxml::Node> processDocumentChildNode(std::unique_ptr<oxml::Node> bnode);
    std::unique_ptr<oxml::Node> processPageChildNode(std::unique_ptr<oxml::Node> bnode);
    std::unique_ptr<oxml::Node> processLayerChildNode(std::unique_ptr<oxml::Node> bnode);
    std::unique_ptr<oxml::Node> processAttachment(std::unique_ptr<oxml::Node> bnode);

    void parseXournalTag(const XmlParserHelper::AttributeMap& attributes);
    void parseMrWriterTag(const XmlParserHelper::AttributeMap& attributes);
    void parsePageTag(const XmlParserHelper::AttributeMap& attributes);
    void parseAudioTag(const XmlParserHelper::AttributeMap& attributes);
    void parseBackgroundTag(const XmlParserHelper::AttributeMap& attributes);
    void parseBgSolid(const XmlParserHelper::AttributeMap& attributes);
    void parseBgPixmap(const XmlParserHelper::AttributeMap& attributes);
    void parseBgPdf(const XmlParserHelper::AttributeMap& attributes);
    void parseLayerTag(const XmlParserHelper::AttributeMap& attributes);
    void parseTimestampTag(const XmlParserHelper::AttributeMap& attributes);
    void parseStrokeTag(const XmlParserHelper::AttributeMap& attributes);
    void parseStrokeText(std::string_view text);
    void parseTextTag(const XmlParserHelper::AttributeMap& attributes);
    void parseTextText(std::string_view text);
    void parseImageTag(const XmlParserHelper::AttributeMap& attributes);
    void parseImageText(std::string_view text);
    void parseTexImageTag(const XmlParserHelper::AttributeMap& attributes);
    void parseTexImageText(std::string_view text);
    void parseAttachmentTag(const XmlParserHelper::AttributeMap& attributes);


    /**
     * Add the current node's tag to the hierarchy stack and return it
     */
    std::pair<std::unique_ptr<oxml::OpeningNode>, xoj::xml_tags::Type> openTag(std::unique_ptr<oxml::Node> bnode);
    /**
     * Remove the specified tag from the hierarchy stack. This function also
     * checks the document integrity together with `openTag()`: each opening
     * tag matches exactly one closing tag of the same name.
     * @exception Throws a `std::runtime_error` if the document structure is not
     *            sound.
     */
    void closeTag(std::unique_ptr<oxml::Node> bnode);

    xoj::xml_tags::Type tagNameToType(std::string_view name) const;

#ifdef DEBUG_XML_PARSER
    void debugPrintNode(oxml::Node* bnode);
    void debugPrintAttributes();
#endif


    struct textReaderDeleter {
        void operator()(xmlTextReader* ptr) { xmlFreeTextReader(ptr); }
    };
    using xmlTextReaderWrapper = std::unique_ptr<xmlTextReader, textReaderDeleter>;


    oxml::Reader reader;
    LoadHandler* handler;

    std::stack<xoj::xml_tags::Type> hierarchy;

    bool pdfFilenameParsed;

    size_t tempTimestamp;
    fs::path tempFilename;

    std::vector<double> pressureBuffer;
};
