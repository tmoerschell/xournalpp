/*
 * Xournal++
 *
 * Lightwheight, streaming, in-situ XML reader
 *
 * @author Xournal++ Team
 * https://github.com/xournalpp/xournalpp
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <functional>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

#include "util/EnumIndexedArray.h"

namespace oxml {

enum class NodeType : unsigned int { OPENING, CLOSING, TEXT, END, ENUMERATOR_COUNT };

constexpr EnumIndexedArray<const char*, NodeType> NODE_TYPE_NAMES = {"opening", "closing", "text", "end"};

class Node {
public:
    virtual NodeType getType() const = 0;

private:
    virtual void shiftData(ptrdiff_t distance) = 0;

    friend class Reader;
};

class OpeningNode: public Node {
public:
    virtual NodeType getType() const override { return NodeType::OPENING; };

    std::string_view getName() const { return name; }
    bool isEmpty() const { return empty; }
    const std::vector<std::pair<std::string_view, std::string_view>>& getAttributes() const { return attributes; }

private:
    virtual void shiftData(ptrdiff_t distance) override;

    std::string_view name;
    bool empty;

    std::vector<std::pair<std::string_view, std::string_view>> attributes;

    friend class Reader;
};

class ClosingNode: public Node {
public:
    virtual NodeType getType() const override { return NodeType::CLOSING; };

    std::string_view getName() const { return name; }

private:
    virtual void shiftData(ptrdiff_t distance) override;

    std::string_view name;

    friend class Reader;
};

class TextNode: public Node {
public:
    virtual NodeType getType() const override { return NodeType::TEXT; };

    std::string_view getText() const { return text; }

private:
    virtual void shiftData(ptrdiff_t distance) override;

    std::string_view text;

    friend class Reader;
};

class EndNode: public Node {
public:
    virtual NodeType getType() const override { return NodeType::END; };

private:
    virtual void shiftData(ptrdiff_t distance) override {};
};


class Reader {
public:
    Reader(std::function<int(char*, int)> readCallback, std::function<void(void)> closeCallback);
    ~Reader();

    /**
     * Reads the next node from the XML input.
     *
     * @returns A unique pointer to the parsed Node. Returns an EndNode when the
     *          end is reached.
     * @throws `std::runtime_error` if an unexpected character is encountered.
     */
    std::unique_ptr<Node> readNode();
    /**
     * Null-terminate a string view from the reader.
     * Internally, this function only checks that sv is still in the buffer and
     * appends '\0', so it does not require an expensive copy.
     *
     * @param sv A string view from the reader
     * @returns A null-terminated C-string
     * @throws `std::runtime_error` if any part of `sv` is outside the internal
     *         buffer. This will never be the case for any string view provided
     *         by the reader in the latest call to readNode().
     */
    const char* nullTerminate(std::string_view sv);

private:
    /**
     * If the `dataStart` moved, shifts all string views contained in the
     * `currentNode, as well as `tempAttrName`, so that they stay valid.
     *
     * @param oldDataStart Previous location of `dataStart`
     * @returns The shift that was experienced by `dataStart`. This should be
     *          used if any local string views need to be updated.
     */
    ptrdiff_t addressShift(const char* oldDataStart);
    /**
     * Refill the buffer by moving the data from `dataStart` to the beginning of
     * the buffer and filling the newly available space with data from the
     * `readCallback`. If the buffer is full, its size is doubled before reading
     * new content. Iterators are updated in the process.
     *
     * @attention As this function will most likely move data around, it is
     *            important to properly update any string views that were
     *            pointing to it, using `addressShift()` and the return value.
     * @returns The shift that was experienced by existing data.
     * @throws `std::runtime_error` if the read callback fails.
     * @throws `std::bad_alloc` if growing the buffer fails.
     */
    ptrdiff_t refillBuffer();
    char peek();
    void advance();
    void skipWhitespace();
    /**
     * Moves the data from `firstOffset` to `currentPos` (not included) within
     * the buffer by the `readingOffset` to the left.
     * Use this after replacing character references.
     */
    void moveOffsetData();
    /**
     * Writes Unicode codepoint as UTF-8 into the buffer.
     *
     * @param codepointSv A string view of the Unicode codepoint
     * @param base The base of the codepoint (hex or decimal)
     * @param entitySize The original size of the entity for offset calculation
     */
    void replaceUnicodeReference(std::string_view codepointSv, int base, size_t entitySize);
    /**
     * Reads a character reference ("&...;") and replace it with the appropriate
     * character or UTF-8 string. If unrecognized, the reference is left
     * unchanged.
     * After calling this function, remaining data, starting at firstOffset,
     * will be offset by readingOffset, and must be moved into the correct place
     * before returning or when replacing another unicode reference.
     *
     * @returns The shift that was experienced by `dataStart` as a result of
     *          eventual buffer managment operations.
     * @see moveOffsetData()
     */
    ptrdiff_t replaceCharacterReference();
    /**
     * Parse the data while `func()` returns true. Refills the buffer and
     * updates `start` as needed.
     *
     * @param start An iterator to the start of the string
     * @param func A function that decides whether to continue parsing or not
     *             based on the current character.
     * @param ignoreCR A toggle for ignoring character references, (e.x., inside
     *                 comments)
     * @returns A string view of the whole parsed data.
     */
    template <typename TerminationCondition>
    std::string_view parseWhileFrom(std::vector<char>::iterator& start, TerminationCondition func,
                                    bool ignoreCR = false);
    /**
     * Parse data from the current position while `func()` returns true.
     *
     * @param func A function that decides whether to continue parsing or not
     *             based on the current character.
     * @param ignoreCR A toggle for ignoring character references, (e.x., inside
     *                 comments)
     * @returns A string view of the whole parsed data.
     */
    std::string_view parseWhile(bool (*func)(char), bool ignoreCR = false);
    /**
     * Parses data from `start` until `terminator` is reached.
     *
     * @param start An iterator to the start of the string.
     * @param terminator Character that stops parsing.
     * @param ignoreCR A toggle for ignoring character references, (e.x., inside
     *                 comments)
     * @returns A string view of the whole parsed data.
     */
    std::string_view parseUntilFrom(std::vector<char>::iterator& start, char terminator, bool ignoreCR = false);
    /**
     * Parses data from the current positition until `terminator` is reached.
     *
     * @param terminator Character that stops parsing.
     * @param ignoreCR A toggle for ignoring character references, (e.x., inside
     *                 comments)
     * @returns A string view of the whole parsed data.
     */
    std::string_view parseUntil(char terminator, bool ignoreCR = false);
    /**
     * Parses and returns a name (e.g., tag or attribute name) from the current
     * position.
     *
     * @returns A string view of the parsed name.
     */
    std::string_view parseName();

    std::unique_ptr<Node> parseOpeningNode();
    std::unique_ptr<Node> parseClosingNode();
    std::unique_ptr<Node> parseTextNode();
    void ignoreSpecialXML();
    void ignoreNodeEnd();

    std::function<int(char*, int)> readCallback;
    std::function<void(void)> closeCallback;

    std::vector<char> buffer;
    std::vector<char>::iterator dataStart;
    std::vector<char>::iterator currentPos;
    std::vector<char>::iterator dataEnd;
    bool hasMoreData = true;
    bool lastNodeWasOpening = false;
    Node* currentNode;
    std::string_view tempAttrName;
    ptrdiff_t readingOffset;
    std::vector<char>::iterator firstOffset;
};


// template implementation

template <typename TerminationCondition>
std::string_view Reader::parseWhileFrom(std::vector<char>::iterator& start, TerminationCondition func, bool ignoreCR) {
    while (this->currentPos != this->dataEnd && func(*this->currentPos)) {
        if (*this->currentPos == '&' && !ignoreCR) {  // only repace character references if the data actually matters
            start += replaceCharacterReference();
        }
        advance();
    }
    if (this->currentPos == this->dataEnd) {
        if (this->hasMoreData) {
            start += refillBuffer();
            return parseWhileFrom(start, func, ignoreCR);
        } else {
            throw std::runtime_error("Unexpected end of data while parsing");
        }
    }
    if (ignoreCR) {
        return std::string_view(&(*start), this->currentPos - start);
    } else {
        moveOffsetData();
        const auto result = std::string_view(&(*start), this->currentPos - this->readingOffset - start);
        this->readingOffset = 0;
#ifndef NDEBUG
        this->firstOffset = this->buffer.end();
#endif  // NDEBUG
        return result;
    }
}

}  // namespace oxml
