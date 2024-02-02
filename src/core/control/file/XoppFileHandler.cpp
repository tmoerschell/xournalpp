#include "control/file/XoppFileHandler.h"

#include <zlib.h>  // for gzFile

#include "model/Document.h"     // for Document
#include "util/GzUtil.h"        // for openPath
#include "util/OutputStream.h"  // for GzOutputStream

#include "filesystem.h"

XoppFileHandler::XoppFileHandler() = default;

XoppFileHandler::XoppFileHandler(const fs::path& path): SequentialAccessFileHandler(path) {}

XoppFileHandler::~XoppFileHandler() = default;

auto XoppFileHandler::loadWholeDocument(Document& document) -> bool {
    // open the file in read mode
    gzFile file = GzUtil::openPath(filepath, "r");
    if (!file) {
        return false;
    }

    // parse the file contents
    // ...

    // close file and return
    if (gzclose(file) != Z_OK) {
        return false;
    } else {
        return true;
    }
}

auto XoppFileHandler::saveWholeDocument(const Document& document) -> bool {
    // open file for writing
    GzOutputStream file(filepath);
    if (!file.getLastError().empty()) {
        return false;
    }

    // write file contents
    // ...

    // close file and return
    file.close();
    if (!file.getLastError().empty()) {
        return false;
    } else {
        return true;
    }
}
