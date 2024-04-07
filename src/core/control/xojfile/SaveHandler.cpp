#include "SaveHandler.h"

#include <cinttypes>   // for PRIx32
#include <cstdint>     // for uint32_t
#include <cstdio>      // for sprintf, size_t

#include <cairo.h>                  // for cairo_surface_t
#include <gdk-pixbuf/gdk-pixbuf.h>  // for gdk_pixbuf_save
#include <glib.h>                   // for g_free, g_strdup_printf

#include "control/pagetype/PageTypeHandler.h"  // for PageTypeHandler
#include "control/xml/XmlAudioNode.h"          // for XmlAudioNode
#include "control/xml/XmlImageNode.h"          // for XmlImageNode
#include "control/xml/XmlNode.h"               // for XmlNode
#include "control/xml/XmlPointNode.h"          // for XmlPointNode
#include "control/xml/XmlTexNode.h"            // for XmlTexNode
#include "control/xml/XmlTextNode.h"           // for XmlTextNode
#include "control/xojfile/XmlAttrs.h"          // for XmlAttrs
#include "control/xojfile/XmlTags.h"           // for XmlTags
#include "model/AudioElement.h"                // for AudioElement
#include "model/BackgroundImage.h"             // for BackgroundImage
#include "model/Document.h"                    // for Document
#include "model/Element.h"                     // for Element, ELEMENT_IMAGE
#include "model/Font.h"                        // for XojFont
#include "model/Image.h"                       // for Image
#include "model/Layer.h"                       // for Layer
#include "model/LineStyle.h"                   // for LineStyle
#include "model/PageType.h"                    // for PageType
#include "model/Point.h"                       // for Point
#include "model/Stroke.h"                      // for Stroke, StrokeCapStyle
#include "model/StrokeStyle.h"                 // for StrokeStyle
#include "model/TexImage.h"                    // for TexImage
#include "model/Text.h"                        // for Text
#include "model/XojPage.h"                     // for XojPage
#include "pdf/base/XojPdfDocument.h"           // for XojPdfDocument
#include "util/OutputStream.h"                 // for GzOutputStream, Output...
#include "util/PathUtil.h"                     // for clearExtensions, normalizeAssetPath
#include "util/PlaceholderString.h"            // for PlaceholderString
#include "util/i18n.h"                         // for FS, _F

#include "config.h"  // for FILE_FORMAT_VERSION
#include "filesystem.h"


static constexpr auto& TAG_NAMES = XmlTags::NAMES;
using TagType = XmlTags::Type;

SaveHandler::SaveHandler() {
    this->firstPdfPageVisited = false;
    this->attachBgId = 1;
}

void SaveHandler::prepareSave(const Document* doc, const fs::path& target) {
    if (this->root) {
        // cleanup old data
        backgroundImages.clear();
    }

    this->firstPdfPageVisited = false;
    this->attachBgId = 1;

    root.reset(new XmlNode(TAG_NAMES[TagType::XOURNAL]));

    writeHeader();

    cairo_surface_t* preview = doc->getPreview();
    if (preview) {
        auto* image = new XmlImageNode(TAG_NAMES[TagType::PREVIEW]);
        image->setImage(preview);
        this->root->addChild(image);
    }

    for (size_t i = 0; i < doc->getPageCount(); i++) {
        PageRef p = doc->getPage(i);
        p->getBackgroundImage().clearSaveState();
    }

    for (size_t i = 0; i < doc->getPageCount(); i++) {
        PageRef p = doc->getPage(i);
        visitPage(root.get(), p, doc, static_cast<int>(i), target);
    }
}

void SaveHandler::writeHeader() {
    this->root->setAttrib(XmlAttrs::CREATOR_STR, PROJECT_STRING);
    this->root->setAttrib(XmlAttrs::FILEVERSION_STR, FILE_FORMAT_VERSION);
    this->root->addChild(new XmlTextNode(TAG_NAMES[TagType::TITLE],
                                         std::string{"Xournal++ document - see "} + PROJECT_HOMEPAGE_URL));
}

auto SaveHandler::getColorStr(Color c, unsigned char alpha) -> std::string {
    char str[10];
    sprintf(str, "#%08" PRIx32, uint32_t(c) << 8U | alpha);
    std::string color(str);
    return color;
}

void SaveHandler::writeTimestamp(XmlAudioNode* xmlAudioNode, const AudioElement* audioElement) {
    if (!audioElement->getAudioFilename().empty()) {
        /** set stroke timestamp value to the XmlPointNode */
        xmlAudioNode->setAttrib(XmlAttrs::TIMESTAMP_STR, audioElement->getTimestamp());
        xmlAudioNode->setAttrib(XmlAttrs::AUDIO_FILENAME_STR, audioElement->getAudioFilename().u8string());
    }
}

void SaveHandler::visitStroke(XmlPointNode* stroke, const Stroke* s) {
    StrokeTool t = s->getToolType();

    unsigned char alpha = 0xff;

    if (t == StrokeTool::PEN) {
        stroke->setAttrib(XmlAttrs::TOOL_STR, "pen");
        writeTimestamp(stroke, s);
    } else if (t == StrokeTool::ERASER) {
        stroke->setAttrib(XmlAttrs::TOOL_STR, "eraser");
    } else if (t == StrokeTool::HIGHLIGHTER) {
        stroke->setAttrib(XmlAttrs::TOOL_STR, "highlighter");
        alpha = 0x7f;
    } else {
        g_warning("Unknown StrokeTool::Value");
        stroke->setAttrib(XmlAttrs::TOOL_STR, "pen");
    }

    stroke->setAttrib(XmlAttrs::COLOR_STR, getColorStr(s->getColor(), alpha).c_str());

    const auto& pts = s->getPointVector();

    stroke->setPoints(pts);

    if (s->hasPressure()) {
        std::vector<double> values;
        values.reserve(pts.size() + 1);
        values.emplace_back(s->getWidth());
        std::transform(pts.begin(), pts.end() - 1, std::back_inserter(values), [](const Point& p) { return p.z; });
        stroke->setAttrib(XmlAttrs::WIDTH_STR, std::move(values));
    } else {
        stroke->setAttrib(XmlAttrs::WIDTH_STR, s->getWidth());
    }

    visitStrokeExtended(stroke, s);
}

/**
 * Export the fill attributes
 */
void SaveHandler::visitStrokeExtended(XmlPointNode* stroke, const Stroke* s) {
    if (s->getFill() != -1) {
        stroke->setAttrib(XmlAttrs::FILL_STR, s->getFill());
    }

    const StrokeCapStyle capStyle = s->getStrokeCapStyle();
    if (capStyle == StrokeCapStyle::BUTT) {
        stroke->setAttrib(XmlAttrs::CAPSTYLE_STR, "butt");
    } else if (capStyle == StrokeCapStyle::ROUND) {
        stroke->setAttrib(XmlAttrs::CAPSTYLE_STR, "round");
    } else if (capStyle == StrokeCapStyle::SQUARE) {
        stroke->setAttrib(XmlAttrs::CAPSTYLE_STR, "square");
    } else {
        g_warning("Unknown stroke cap type: %i", capStyle);
        stroke->setAttrib(XmlAttrs::CAPSTYLE_STR, "round");
    }

    if (s->getLineStyle().hasDashes()) {
        stroke->setAttrib(XmlAttrs::STYLE_STR, StrokeStyle::formatStyle(s->getLineStyle()));
    }
}

void SaveHandler::visitLayer(XmlNode* page, const Layer* l) {
    auto* layer = new XmlNode(TAG_NAMES[TagType::LAYER]);
    page->addChild(layer);
    if (l->hasName()) {
        layer->setAttrib(XmlAttrs::NAME_STR, l->getName().c_str());
    }

    for (const auto& e: l->getElementsView()) {
        if (e->getType() == ELEMENT_STROKE) {
            auto* s = dynamic_cast<const Stroke*>(e);
            auto* stroke = new XmlPointNode(TAG_NAMES[TagType::STROKE]);
            layer->addChild(stroke);
            visitStroke(stroke, s);
        } else if (e->getType() == ELEMENT_TEXT) {
            const Text* t = dynamic_cast<const Text*>(e);
            auto* text = new XmlTextNode(TAG_NAMES[TagType::TEXT], t->getText());
            layer->addChild(text);

            const XojFont& f = t->getFont();

            text->setAttrib(XmlAttrs::FONT_STR, f.getName().c_str());
            text->setAttrib(XmlAttrs::SIZE_STR, f.getSize());
            text->setAttrib(XmlAttrs::X_COORD_STR, t->getX());
            text->setAttrib(XmlAttrs::Y_COORD_STR, t->getY());
            text->setAttrib(XmlAttrs::COLOR_STR, getColorStr(t->getColor()).c_str());

            writeTimestamp(text, t);
        } else if (e->getType() == ELEMENT_IMAGE) {
            auto* i = dynamic_cast<const Image*>(e);
            auto* image = new XmlImageNode(TAG_NAMES[TagType::IMAGE]);
            layer->addChild(image);

            image->setImage(i->getImage());

            image->setAttrib(XmlAttrs::LEFT_POS_STR, i->getX());
            image->setAttrib(XmlAttrs::TOP_POS_STR, i->getY());
            image->setAttrib(XmlAttrs::RIGHT_POS_STR, i->getX() + i->getElementWidth());
            image->setAttrib(XmlAttrs::BOTTOM_POS_STR, i->getY() + i->getElementHeight());
        } else if (e->getType() == ELEMENT_TEXIMAGE) {
            auto* i = dynamic_cast<const TexImage*>(e);
            auto* image = new XmlTexNode(TAG_NAMES[TagType::TEXIMAGE], std::string(i->getBinaryData()));
            layer->addChild(image);

            image->setAttrib(XmlAttrs::TEXT_STR, i->getText().c_str());
            image->setAttrib(XmlAttrs::LEFT_POS_STR, i->getX());
            image->setAttrib(XmlAttrs::TOP_POS_STR, i->getY());
            image->setAttrib(XmlAttrs::RIGHT_POS_STR, i->getX() + i->getElementWidth());
            image->setAttrib(XmlAttrs::BOTTOM_POS_STR, i->getY() + i->getElementHeight());
        }
    }
}

void SaveHandler::visitPage(XmlNode* root, ConstPageRef p, const Document* doc, int id, const fs::path& target) {
    auto* page = new XmlNode(TAG_NAMES[TagType::PAGE]);
    root->addChild(page);
    page->setAttrib(XmlAttrs::WIDTH_STR, p->getWidth());
    page->setAttrib(XmlAttrs::HEIGHT_STR, p->getHeight());

    auto* background = new XmlNode(TAG_NAMES[TagType::BACKGROUND]);
    page->addChild(background);

    writeBackgroundName(background, p);

    if (p->getBackgroundType().isPdfPage()) {
        /**
         * ATTENTION! The original xournal can only read the XML if the attributes are in the right order!
         * DO NOT CHANGE THE ORDER OF THE ATTRIBUTES!
         */

        background->setAttrib(XmlAttrs::TYPE_STR, "pdf");
        if (!firstPdfPageVisited) {
            firstPdfPageVisited = true;

            if (doc->isAttachPdf()) {
                background->setAttrib(XmlAttrs::DOMAIN_STR, "attach");
                auto filepath = doc->getFilepath();
                Util::clearExtensions(filepath);
                filepath += ".xopp.bg.pdf";
                background->setAttrib(XmlAttrs::FILENAME_STR, "bg.pdf");

                GError* error = nullptr;
                if (!exists(filepath)) {
                    doc->getPdfDocument().save(filepath, &error);
                }

                if (error) {
                    if (!this->errorMessage.empty()) {
                        this->errorMessage += "\n";
                    }
                    this->errorMessage +=
                            FS(_F("Could not write background \"{1}\", {2}") % filepath.u8string() % error->message);

                    g_error_free(error);
                }
            } else {
                // "absolute" just means path. For backward compatibility, it is hard to change the word
                background->setAttrib(XmlAttrs::DOMAIN_STR, "absolute");
                auto normalizedPath = Util::normalizeAssetPath(doc->getPdfFilepath(), target.parent_path(),
                                                               doc->getPathStorageMode());
                background->setAttrib(XmlAttrs::FILENAME_STR, std::move(normalizedPath));
            }
        }
        background->setAttrib(XmlAttrs::PAGE_NUMBER_STR, p->getPdfPageNr() + 1);
    } else if (p->getBackgroundType().isImagePage()) {
        background->setAttrib(XmlAttrs::TYPE_STR, "pixmap");

        int cloneId = p->getBackgroundImage().getCloneId();
        if (cloneId != -1) {
            background->setAttrib(XmlAttrs::DOMAIN_STR, "clone");
            char* filename = g_strdup_printf("%i", cloneId);
            background->setAttrib(XmlAttrs::FILENAME_STR, filename);
            g_free(filename);
        } else if (p->getBackgroundImage().isAttached() && p->getBackgroundImage().getPixbuf()) {
            char* filename = g_strdup_printf("bg_%d.png", this->attachBgId++);
            background->setAttrib(XmlAttrs::DOMAIN_STR, "attach");
            background->setAttrib(XmlAttrs::FILENAME_STR, filename);

            backgroundImages.emplace_back(p->getBackgroundImage());

            /*
             * Because BackgroundImage is basically a wrapped pointer, the following lines actually modify
             * *(p->getBackgroundImage().content) and thus the Document.
             * TODO Find a better way
             */
            backgroundImages.back().setFilepath(filename);
            backgroundImages.back().setCloneId(id);

            g_free(filename);
        } else {
            // "absolute" just means path. For backward compatibility, it is hard to change the word
            background->setAttrib(XmlAttrs::DOMAIN_STR, "absolute");
            auto normalizedPath = Util::normalizeAssetPath(p->getBackgroundImage().getFilepath(), target.parent_path(),
                                                           doc->getPathStorageMode());
            background->setAttrib(XmlAttrs::FILENAME_STR, std::move(normalizedPath));

            BackgroundImage img = p->getBackgroundImage();

            /*
             * Because BackgroundImage is basically a wrapped pointer, the following line actually modifies
             * *(p->getBackgroundImage().content) and thus the Document.
             * TODO Find a better way
             */
            img.setCloneId(id);
        }
    } else {
        writeSolidBackground(background, p);
    }

    // no layer, but we need to write one layer, else the old Xournal cannot read the file
    if (p->getLayerCount() == 0) {
        auto* layer = new XmlNode(TAG_NAMES[TagType::LAYER]);
        page->addChild(layer);
    }

    for (const Layer* l: p->getLayersView()) {
        visitLayer(page, l);
    }
}

void SaveHandler::writeSolidBackground(XmlNode* background, ConstPageRef p) {
    background->setAttrib(XmlAttrs::TYPE_STR, "solid");
    background->setAttrib(XmlAttrs::COLOR_STR, getColorStr(p->getBackgroundColor()));
    background->setAttrib(XmlAttrs::STYLE_STR,
                          PageTypeHandler::getStringForPageTypeFormat(p->getBackgroundType().format));

    // Not compatible with Xournal, so the background needs
    // to be changed to a basic one!
    if (!p->getBackgroundType().config.empty()) {
        background->setAttrib(XmlAttrs::CONFIG_STR, p->getBackgroundType().config);
    }
}

void SaveHandler::writeBackgroundName(XmlNode* background, ConstPageRef p) {
    if (p->backgroundHasName()) {
        background->setAttrib(XmlAttrs::NAME_STR, p->getBackgroundName());
    }
}

void SaveHandler::saveTo(const fs::path& filepath, ProgressListener* listener) {
    GzOutputStream out(filepath);

    if (!out.getLastError().empty()) {
        this->errorMessage = out.getLastError();
        return;
    }

    saveTo(&out, filepath, listener);

    out.close();

    if (this->errorMessage.empty()) {
        this->errorMessage = out.getLastError();
    }
}

void SaveHandler::saveTo(OutputStream* out, const fs::path& filepath, ProgressListener* listener) {
    // XMLNode should be locale-safe ( store doubles using Locale 'C' format

    out->write("<?xml version=\"1.0\" standalone=\"no\"?>\n");
    root->writeOut(out, listener);

    for (const BackgroundImage& img: backgroundImages) {
        auto tmpfn = (fs::path(filepath) += ".") += img.getFilepath();
        // Are we certain that does not modify the GdkPixbuf?
        if (!gdk_pixbuf_save(const_cast<GdkPixbuf*>(img.getPixbuf()), tmpfn.u8string().c_str(), "png", nullptr,
                             nullptr)) {
            if (!this->errorMessage.empty()) {
                this->errorMessage += "\n";
            }

            this->errorMessage += FS(_F("Could not write background \"{1}\". Continuing anyway.") % tmpfn.u8string());
        }
    }
}

auto SaveHandler::getErrorMessage() -> const std::string& { return this->errorMessage; }
