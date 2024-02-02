/*
 * Xournal++
 *
 * File handler for loading and saving
 * .xopp files
 *
 * @author Xournal++ Team
 * https://github.com/xournalpp/xournalpp
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include "control/file/SequentialAccessFileHandler.h"

#include "filesystem.h"

class XoppFileHandler: public SequentialAccessFileHandler {
public:
    XoppFileHandler();
    XoppFileHandler(const fs::path& path);
    virtual ~XoppFileHandler() override;

    virtual bool loadWholeDocument(Document& document) override;
    virtual bool saveWholeDocument(const Document& document) override;
};
