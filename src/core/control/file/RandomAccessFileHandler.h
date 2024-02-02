/*
 * Xournal++
 *
 * File handler that supports reading and
 * writing only part of a file
 * This file handler also tracks changes made to a document in order be able to
 * only save the changes that have been made since the last save.
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

class RandomAccessFileHandler: public FileHandler {
protected:
    RandomAccessFileHandler();
    virtual ~RandomAccessFileHandler() override;

public:
    virtual bool openFile(const fs::path& path) override = 0;

    bool loadDocument(Document& document) override final;
    bool saveChanges(const Document& document) override final;
    bool saveAs(const fs::path& newFilepath, const Document& document) override final;

    bool loadWholeDocument(Document& document) override final;
    virtual bool saveWholeDocument(const Document& document) override = 0;

    virtual bool loadObject(ObjectType type, size_t object_nr, Document& document) override = 0;
    virtual bool loadObjectWithChildren(ObjectType type, size_t object_nr, Document& document) override = 0;
    virtual bool saveObject(ObjectType type, size_t object_nr, const Document& document) override = 0;

    bool isRandomAccess() const override final;

protected:
    /**
     * Create and open a new empty file. This will overwrite preexisting files.
     *
     * @param newFilepath the path of the file that should be created
     * @return The new file was successfully created.
     */
    virtual bool createEmptyFile(const fs::path& newFilepath) = 0;
    /**
     * Close an open file. This function does not save the open file before
     * closing.
     */
    virtual void closeFile() = 0;
};
