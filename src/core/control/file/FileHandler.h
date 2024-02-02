/*
 * Xournal++
 *
 * Base class for any file handler
 * The file handler allows reading and writing to a file. It may also track
 * changes that are made to the document in order to save only the necessary
 * parts if the implementation allows it.
 *
 * @author Xournal++ Team
 * https://github.com/xournalpp/xournalpp
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include "model/Document.h"
#include "model/XojPage.h"

#include "filesystem.h"

enum class ObjectType { DocumentObject, PageObject, LayerObject, TileObject };

class FileHandler {
protected:
    FileHandler();
    virtual ~FileHandler();

public:
    /**
     * Open a file and prepare contents for loading.
     *
     * @param path the path at which the file resides
     * @return true if the file is ready for use
     */
    virtual bool openFile(const fs::path& path) = 0;


    // implementation-adapted functions
    /**
     * Depending on the implementation, either load the whole file into
     * `document` (for sequential access files), or only load the top-level
     * document object (random access files).
     */
    virtual bool loadDocument(Document& document) = 0;
    /**
     * Save any changes that have been made to the document since it was last
     * saved. Random-access-capable implementations will minimize changes to the
     * file, while others will need to rewrite the whole file.
     */
    virtual bool saveChanges(const Document& document) = 0;
    /**
     * Save the `document` at a new location. This function may be called
     * whether the document is new to the file handler or not. If another file
     * is still open, it is closed and the new file will be kept open.
     *
     * @param newFilepath the new path at which the document should be saved
     * @param document the document that should be saved to the file
     * @return The document was sucessfully saved to the file.
     */
    virtual bool saveAs(const fs::path& newFilepath, const Document& document) = 0;


    // whole-file load and save functions

    /**
     * Load the whole file into `document`. After calling this function, all
     * contents of `document` are initialized and will not require any further
     * loading.
     *
     * @note For random access files, this call is equivalent to
     * `loadObjectWithChildren(ObjectType::DocumentObject, 0, document)`.
     *
     * @param document the document that the file should be loaded into
     * @return The document was successfully loaded.
     */
    virtual bool loadWholeDocument(Document& document) = 0;
    /**
     * Save the whole `document` to the file. Random-access-capable
     * implementations may choose to rewrite the complete file when this
     * function is called.
     *
     * @param document the document that should be saved to the file
     * @return The document was sucessfully saved to the file.
     */
    virtual bool saveWholeDocument(const Document& document) = 0;


    // random access load and save functions

    /**
     * Load the specified object into `document`. Children of the loaded object
     * are not loaded and must be fetched in a subsequent call if required. The
     * parent of the requested object must already have been loaded.
     *
     * @note When the `DocumentObject` is requested, `object_id` is ignored
     *
     * @param type the type of the object to be loaded
     * @param object_nr the sequence number of the object to be loaded, such as
     * the page number or the layer number
     * @param document the document that the object should be loaded into
     * @return The object was successfully loaded.
     */
    virtual bool loadObject(ObjectType type, size_t object_nr, Document& document) = 0;
    /**
     * Load the specified object and all its children into `document`. The
     * parent of the requested object must already have been loaded.
     *
     * @note When the `DocumentObject` is requested, `object_id` is ignored
     *
     * @param type the type of the top-level object to be loaded
     * @param object_nr the sequence number of the object to be loaded, such as
     * the page number or the layer number
     * @param document the document that the object and its children should be
     * loaded into
     * @return The object and its children were successfully loaded.
     */
    virtual bool loadObjectWithChildren(ObjectType type, size_t object_nr, Document& document) = 0;
    /**
     * Save the specified object to the file. All children will also be written
     * to the file, while trying to minimize changes to the file. Children that
     * exist in the file but are not loaded in the `document` will remain
     * unmodified.
     *
     * @param type the type of the object to be saved
     * @param object_nr the sequence number of the object to be saved, such as
     * the page number or the layer number
     * @param document the document that contains the object which should be
     * saved
     * @return The object was successfully saved to the file.
     */
    virtual bool saveObject(ObjectType type, size_t object_nr, const Document& document) = 0;


    /**
     * Return whether the handler supports random access reading and writing.
     */
    virtual bool isRandomAccess() const = 0;

protected:
    fs::path filepath;
};
