#include "oxml.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstring>
#include <memory>

#include <glib-2.0/glib.h>

#include "util/Assert.h"

using namespace oxml;
static constexpr std::size_t INITIAL_BUFFER_SIZE = 1 << 10;  // 1 kB

// Adjusts the string view by shifting its data pointer by the specified distance
static void shiftStringView(std::string_view& sv, ptrdiff_t distance) {
    if (!sv.empty()) {
        sv = {sv.data() + distance, sv.size()};
    }
}

void OpeningNode::shiftData(ptrdiff_t distance) {
    shiftStringView(this->name, distance);
    for (auto& [attr, value]: this->attributes) {
        shiftStringView(attr, distance);
        shiftStringView(value, distance);
    }
}
void ClosingNode::shiftData(ptrdiff_t distance) { shiftStringView(this->name, distance); }
void TextNode::shiftData(ptrdiff_t distance) { shiftStringView(this->text, distance); }


Reader::Reader(std::function<int(char*, int)> readCallback, std::function<void(void)> closeCallback):
        readCallback(readCallback),
        closeCallback(closeCallback),
        buffer(INITIAL_BUFFER_SIZE),
        dataStart(this->buffer.begin()),
        currentPos(this->buffer.begin()),
        dataEnd(this->buffer.begin()),
        hasMoreData(true),
        lastNodeWasOpening(false),
        currentNode(nullptr),
        readingOffset(0),
        firstOffset(this->buffer.end()) {}

Reader::~Reader() { this->closeCallback(); }

ptrdiff_t Reader::addressShift(const char* oldDataStart) {
    const auto shift = &(*this->dataStart) - oldDataStart;
    if ((oldDataStart != &(*this->dataStart)) && this->currentNode) {
        this->currentNode->shiftData(shift);
        if (!this->tempAttrName.empty()) {
            shiftStringView(this->tempAttrName, shift);
        }
    }
    return shift;
}

ptrdiff_t Reader::refillBuffer() {
    if (!this->currentNode) {
        // dataStart points to the beginning of the data we want to keep. Here,
        // we do not need to keep any data before the current position.
        this->dataStart = this->currentPos;
    }
    const auto oldDataStart = &(*this->dataStart);
    if (this->dataStart != this->buffer.begin() && this->dataStart != this->dataEnd) {
        // Move the data to the start of the buffer
        const auto shift = this->buffer.begin() - this->dataStart;  // a negative number as we shift to the left
        std::memmove(this->buffer.data(), &(*this->dataStart), this->dataEnd - this->dataStart);
        this->dataEnd += shift;
        this->currentPos += shift;
        this->firstOffset += shift;
        this->dataStart = this->buffer.begin();
    } else if (this->dataStart == this->dataEnd) {
        // All the data in the buffer has been consumed, we can simply overwrite it
        this->dataStart = this->currentPos = this->dataEnd = this->buffer.begin();
        xoj_assert(this->readingOffset == 0);
        this->firstOffset = this->buffer.end();
    } else if (this->dataStart == this->buffer.begin() && this->dataEnd == this->buffer.end()) {
        // The buffer is full; we have to resize the buffer to accomodate more data
        const auto currentPosIndex = this->currentPos - this->buffer.begin();
        const auto dataEndIndex = this->dataEnd - this->buffer.begin();
        const auto firstOffsetIndex = this->firstOffset - this->buffer.begin();
        this->buffer.resize(this->buffer.size() * 2);
        this->dataStart = this->buffer.begin();
        this->currentPos = this->buffer.begin() + currentPosIndex;
        this->dataEnd = this->buffer.begin() + dataEndIndex;
        this->firstOffset = this->buffer.begin() + firstOffsetIndex;
    }

    int bytesRead = this->readCallback(&(*this->dataEnd), static_cast<int>(this->buffer.end() - this->dataEnd));
    if (bytesRead < 0) {
        throw std::runtime_error("Read error occurred");
    }
    this->dataEnd += bytesRead;

    if (bytesRead == 0) {
        this->hasMoreData = false;
    }

    return addressShift(oldDataStart);
}

char Reader::peek() {
    if (this->currentPos == this->dataEnd && this->hasMoreData) {
        refillBuffer();
    }
    return (this->currentPos != this->dataEnd) ? *this->currentPos : '\0';
}

void Reader::advance() {
    if (this->currentPos != this->dataEnd) {
        this->currentPos++;
    }
}

void Reader::skipWhitespace() {
    while (std::isspace(static_cast<unsigned char>(peek()))) {
        advance();
    }
}

void Reader::moveOffsetData() {
    if (this->readingOffset != 0) {
        xoj_assert(this->currentPos - this->firstOffset >= 0);
        xoj_assert(this->readingOffset != 0);
        const auto movingBlockSize = this->currentPos - this->firstOffset;  // currentPos does not need to be moved
        if (movingBlockSize > 0) {
            std::memmove(&(*this->firstOffset) - this->readingOffset, &(*this->firstOffset), movingBlockSize);
            this->firstOffset = this->currentPos;
        }
    }
}

static constexpr std::array<std::pair<std::string_view, char>, 5> predefinedEntities = {
        std::make_pair("amp", '&'), std::make_pair("lt", '<'), std::make_pair("gt", '>'), std::make_pair("apos", '\''),
        std::make_pair("quot", '\"')};

void Reader::replaceUnicodeReference(std::string_view codepointSv, int base, size_t entitySize) {
    if (!codepointSv.empty()) {
        gunichar codepoint{};
        std::from_chars(codepointSv.data(), codepointSv.data() + codepointSv.size(), codepoint, base);
        auto numBytes = g_unichar_to_utf8(codepoint, &(*(this->currentPos - this->readingOffset - entitySize -
                                                         1)));  // There will always be enough space
        this->readingOffset += entitySize + 2 - numBytes;
        this->firstOffset = this->currentPos + 1;
    }
}

ptrdiff_t Reader::replaceCharacterReference() {
    moveOffsetData();  // Move the data if we are already reading at an offset
    advance();         // Skip '&'
    auto oldDataStart = &(*this->dataStart);
    const auto entitySv = parseUntil(';', true);
    const auto entity = std::find_if(predefinedEntities.begin(), predefinedEntities.end(),
                                     [entitySv](std::pair<std::string_view, char> p) { return p.first == entitySv; });
    if (entity != predefinedEntities.end()) {
        this->readingOffset +=
                entity->first.size() + 1;  // +1 for '&', +1 for ';', -1 for the character we are about to write
        *(this->currentPos - this->readingOffset) = entity->second;
        this->firstOffset = this->currentPos + 1;
    } else if (entitySv.size() >= 2 && entitySv.front() == '#') {
        // Numeric character references such as &#123; or &#xABC;
        if (entitySv[1] == 'x') {
            std::string_view hexadecimal = entitySv.substr(2);
            replaceUnicodeReference(hexadecimal, 16, entitySv.size());
        } else {
            std::string_view decimal = entitySv.substr(1);
            replaceUnicodeReference(decimal, 10, entitySv.size());
        }
    }  // Unknown entities are silently ignored
    return addressShift(oldDataStart);
}

std::string_view Reader::parseWhile(bool (*func)(char), bool ignoreCR) {
    auto start = this->currentPos;
    return parseWhileFrom(start, func, ignoreCR);
}

std::string_view Reader::parseUntilFrom(std::vector<char>::iterator& start, char terminator, bool ignoreCR) {
    return parseWhileFrom(start, [terminator](char c) { return c != terminator; });
}

std::string_view Reader::parseUntil(char terminator, bool ignoreCR) {
    auto start = this->currentPos;
    return parseUntilFrom(start, terminator, ignoreCR);
}

std::string_view Reader::parseName() {
    return parseWhile(
            [](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == ':' || c == '_' || c == '-'; });
}

std::unique_ptr<Node> Reader::parseOpeningNode() {
    this->dataStart = this->currentPos;
    auto node = std::make_unique<OpeningNode>();
    this->currentNode = node.get();
    node->name = parseName();
    skipWhitespace();

    while (peek() != '/' && peek() != '>') {
        // When parsing the attribute value, the data may be shifted. That's why store the
        // attribute name in the class so we can access it when shifting data.
        tempAttrName = parseName();
        advance();  // Skip '='
        char quote = peek();
        advance();  // Skip opening quote
        std::string_view attrValue = parseUntil(quote);
        advance();  // Skip closing quote

        node->attributes.emplace_back(tempAttrName, attrValue);
        tempAttrName = {};
        skipWhitespace();
    }

    if (peek() == '/') {
        node->empty = true;
        advance();  // Skip '/'
    }

    advance();  // Skip '>'
    this->currentNode = nullptr;
    this->lastNodeWasOpening = true;
    return node;
}

std::unique_ptr<Node> Reader::parseClosingNode() {
    advance();  // Skip '/'
    this->dataStart = this->currentPos;
    auto node = std::make_unique<ClosingNode>();
    this->currentNode = node.get();
    node->name = parseName();
    advance();  // Skip '>'
    this->currentNode = nullptr;
    this->lastNodeWasOpening = false;
    return node;
}

std::unique_ptr<Node> Reader::parseTextNode() {
    this->dataStart = this->currentPos;
    auto node = std::make_unique<TextNode>();
    this->currentNode = node.get();
    node->text = parseUntil('<');
    this->currentNode = nullptr;
    this->lastNodeWasOpening = false;
    // Discard whitespace-only text nodes
    if (!std::all_of(node->text.begin(), node->text.end(), [](unsigned char c) { return std::isspace(c); })) {
        return node;
    } else {
        return nullptr;
    }
}

void Reader::ignoreSpecialXML() {
    advance();  // Skip '!'
    if (peek() == '-') {
        advance();  // Skip first '-'
        if (peek() == '-') {
            advance();
            do {
                parseUntil('-', true);  // Go to first candidate closing hyphen
                advance();
            } while (peek() != '-' && this->currentPos != this->dataEnd);  // Check for second closing hyphen
            this->lastNodeWasOpening = true;  // Comments may occur inside text sections
            ignoreNodeEnd();
        }
    } else if (peek() == '[') {
        // CDATA section
        parseUntil(']', true);
        ignoreNodeEnd();
    } else {
        // Other declarations
        ignoreNodeEnd();
    }
}

void Reader::ignoreNodeEnd() {
    parseUntil('>', true);
    advance();
}

std::unique_ptr<Node> Reader::readNode() {
    while (true) {
        if (!this->lastNodeWasOpening) {
            skipWhitespace();
        } else if (this->currentPos == this->dataEnd) {
            refillBuffer();
        }

        if (this->currentPos == this->dataEnd && !this->hasMoreData) {
            return std::make_unique<EndNode>();
        }

        char c = peek();

        if (c == '<' or c == '\0') {
            advance();
            if (peek() == '/') {
                return parseClosingNode();
            } else if (peek() == '?') {
                ignoreNodeEnd();  // Ignore prolog
            } else if (peek() == '!') {
                ignoreSpecialXML();
            } else {
                return parseOpeningNode();
            }
        } else if (this->lastNodeWasOpening) {
            auto res = parseTextNode();
            if (res != nullptr) {
                return res;
            }
        } else {
            throw std::runtime_error("Unexpected character \'" + std::string(1, c) + "\' found outside node.");
        }
    }
}

const char* Reader::nullTerminate(std::string_view sv) {
    auto begin = this->buffer.begin() + (sv.data() - this->buffer.data());
    auto end = begin + sv.size();
    if (begin < this->buffer.begin() || end >= this->buffer.end()) {
        throw std::runtime_error("Requested to null-terminate a string view outside the buffer.");
    }
    *end = '\0';
    return sv.data();
}
