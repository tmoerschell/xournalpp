/*
 * Xournal++
 *
 * The window for presentation on an external screen
 *
 * @author Xournal++ Team
 * https://github.com/xournalpp/xournalpp
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include "gui/widgets/PresentationScreenWidget.h"
#include "model/DocumentListener.h"
#include "util/raii/GtkWindowUPtr.h"

class XournalView;

struct PresentationScreenDeleter {
    void operator()(GtkPresentationScreen* w) {
        if (w) {
            gtk_widget_destroy(GTK_WIDGET(w));
        }
    }
};
using GtkPresentationScreenUPtr = std::unique_ptr<GtkPresentationScreen, PresentationScreenDeleter>;

class PresentationWindow: public DocumentListener {
public:
    PresentationWindow(int monitor, XournalView* view);

    double getOptimalZoom(XojPageView* pageView) const;
    void repaintWidget();

public:
    // DocumentListener interface
    void pageSelected(size_t page) override;

private:
    xoj::util::GtkWindowUPtr window;
    GtkPresentationScreenUPtr presentationScreen;
};