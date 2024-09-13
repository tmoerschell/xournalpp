#include "PresentationWindow.h"

#include <stdexcept>

#include <gtk/gtk.h>

#include "control/Control.h"
#include "gui/XournalView.h"
#include "model/DocumentHandler.h"
#include "model/DocumentListener.h"
#include "util/raii/GObjectSPtr.h"

class XournalView;

PresentationWindow::PresentationWindow(int monitor, XournalView* view):
        window(GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL))) {

    // Check that window was sucessfully created
    if (!this->window) {
        throw(std::runtime_error("Failed to create a new window"));
    }

    // Do not allow to close the presentation window manually
    g_signal_connect(this->window.get(), "delete-event", G_CALLBACK(+[](GtkWidget*, void*) { return true; }), nullptr);

    // Set some window attributes
    gtk_window_set_focus_on_map(this->window.get(), false);
    gtk_window_set_decorated(this->window.get(), false);

    GdkScreen* screen = gdk_screen_get_default();

    // Create and add widget to window
    this->presentationScreen.reset(GTK_PRESENTATION_SCREEN(gtk_presentation_screen_new(view)));
    gtk_container_add(GTK_CONTAINER(this->window.get()), GTK_WIDGET(this->presentationScreen.get()));

    // Realize window, following calls require the window to be realized
    gtk_widget_show_all(GTK_WIDGET(this->window.get()));
    GdkWindow* gdkWin = gtk_widget_get_window(GTK_WIDGET(this->window.get()));
    gdk_window_set_skip_taskbar_hint(gdkWin, true);
    gdk_window_set_skip_pager_hint(gdkWin, true);

    // request fullscreen
    gtk_window_fullscreen_on_monitor(this->window.get(), screen, monitor);

    // register document listener
    DocumentListener::registerListener(view->getControl());
}

auto PresentationWindow::getOptimalZoom(XojPageView* pageView) const -> double {
    return gtk_presentation_screen_get_zoom(this->presentationScreen.get(), pageView);
}

void PresentationWindow::repaintWidget() { gtk_widget_queue_draw(GTK_WIDGET(this->presentationScreen.get())); }

void PresentationWindow::pageSelected(size_t page) { repaintWidget(); }
