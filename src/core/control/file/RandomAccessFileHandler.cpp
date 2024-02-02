#include "control/file/RandomAccessFileHandler.h"

#include "model/Document.h"
#include "model/DocumentHandler.h"

#include "filesystem.h"

RandomAccessFileHandler::RandomAccessFileHandler() = default;

RandomAccessFileHandler::~RandomAccessFileHandler() = default;


auto RandomAccessFileHandler::loadDocument(Document& document) -> bool {
    return loadObject(ObjectType::DocumentObject, 0, document);
}

auto RandomAccessFileHandler::saveChanges(const Document& document) -> bool {
    g_warning("RandomAccessFileHandler::saveChanges() is not implemented. Defaulting to saveWholeDocument() instead.");
    return saveWholeDocument(document);
}

auto RandomAccessFileHandler::saveAs(const fs::path& newFilepath, const Document& document) -> bool {
    if (!this->filepath.empty()) {
        g_warning("Efficient RandomAccessFileHandler::saveAs() is not implemented. "
                  "Falling back to inefficient implementation.");
        DocumentHandler handler;
        Document tempDocument(&handler);
        loadWholeDocument(tempDocument);
        closeFile();
        createEmptyFile(filepath);
        saveWholeDocument(tempDocument);
        return saveWholeDocument(document);
    } else {
        createEmptyFile(filepath);
        return saveWholeDocument(document);
    }
}

auto RandomAccessFileHandler::loadWholeDocument(Document& document) -> bool {
    return loadObjectWithChildren(ObjectType::DocumentObject, 0, document);
}


auto RandomAccessFileHandler::isRandomAccess() const -> bool { return true; }
