#include "control/file/SequentialAccessFileHandler.h"

#include "model/Document.h"

#include "filesystem.h"

SequentialAccessFileHandler::SequentialAccessFileHandler() = default;

SequentialAccessFileHandler::SequentialAccessFileHandler(const fs::path& path): FileHandler() { openFile(path); }

SequentialAccessFileHandler::~SequentialAccessFileHandler() = default;


auto SequentialAccessFileHandler::openFile(const fs::path& path) -> bool {
    if (fs::exists(path)) {
        this->filepath = path;
        return true;
    } else {
        return false;
    }
}

auto SequentialAccessFileHandler::loadDocument(Document& document) -> bool { return loadWholeDocument(document); }

auto SequentialAccessFileHandler::saveChanges(const Document& document) -> bool { return saveWholeDocument(document); }

auto SequentialAccessFileHandler::saveAs(const fs::path& newFilepath, const Document& document) -> bool {
    // write document to new file
    this->filepath = newFilepath;
    return saveWholeDocument(document);
}

auto SequentialAccessFileHandler::isRandomAccess() const -> bool { return false; }
