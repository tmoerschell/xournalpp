/*
 * Xournal++
 *
 * File handler that does not support reading
 * or writing only part of a file
 *
 * @author Xournal++ Team
 * https://github.com/xournalpp/xournalpp
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include "control/file/FileHandler.h"
#include "model/Document.h"

#include "filesystem.h"

class SequentialAccessFileHandler: public FileHandler {
protected:
    SequentialAccessFileHandler();
    SequentialAccessFileHandler(const fs::path& path);
    virtual ~SequentialAccessFileHandler() override;

public:
    bool openFile(const fs::path& path) override final;

    bool loadDocument(Document& document) override final;
    bool saveChanges(const Document& document) override final;
    bool saveAs(const fs::path& newFilepath, const Document& document) override final;

    virtual bool loadWholeDocument(Document& document) override = 0;
    virtual bool saveWholeDocument(const Document& document) override = 0;

    bool loadObject(ObjectType type, size_t object_nr, Document& document) override final = 0;
    bool loadObjectWithChildren(ObjectType type, size_t object_nr, Document& document) override final = 0;
    bool saveObject(ObjectType type, size_t object_nr, const Document& document) override final = 0;

    bool isRandomAccess() const override final;
};
