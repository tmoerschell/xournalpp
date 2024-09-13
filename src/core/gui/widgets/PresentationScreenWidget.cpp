#include "PresentationScreenWidget.h"

#include <algorithm>
#include <cmath>  // for NAN
#include <iostream>

#include <cairo.h>
#include <gdk/gdk.h>

#include "control/Control.h"
#include "control/settings/Settings.h"
#include "gui/PageView.h"
#include "gui/XournalView.h"
#include "util/Assert.h"
#include "util/Color.h"
#include "util/Util.h"


static constexpr auto BACKGROUND_COLOR = Color(20, 20, 20);


static void gtk_presentation_screen_class_init(GtkPresentationScreenClass* cptr);
static void gtk_presentation_screen_init(GtkPresentationScreen* presentationScreen);
static void gtk_presentation_screen_realize(GtkWidget* widget);
static auto gtk_presentation_screen_draw(GtkWidget* widget, cairo_t* cr) -> gboolean;

auto gtk_presentation_screen_get_type() -> GType {
    static GType type = 0;

    if (!type) {
        static const GTypeInfo info = {sizeof(GtkPresentationScreenClass),
                                       // base initialize
                                       nullptr,
                                       // base finalize
                                       nullptr,
                                       // class initialize
                                       reinterpret_cast<GClassInitFunc>(gtk_presentation_screen_class_init),
                                       // class finalize
                                       nullptr,
                                       // class data,
                                       nullptr,
                                       // instance size
                                       sizeof(GtkPresentationScreen),
                                       // n_preallocs
                                       0,
                                       // instance init
                                       reinterpret_cast<GInstanceInitFunc>(gtk_presentation_screen_init),
                                       // value table
                                       nullptr};

        type = g_type_register_static(GTK_TYPE_WIDGET, "GtkPresentationScreen", &info, static_cast<GTypeFlags>(0));
    }

    return type;
}

static void gtk_presentation_screen_class_init(GtkPresentationScreenClass* cptr) {
    auto* widget_class = reinterpret_cast<GtkWidgetClass*>(cptr);

    widget_class->realize = gtk_presentation_screen_realize;
    widget_class->draw = gtk_presentation_screen_draw;
}

static void gtk_presentation_screen_init(GtkPresentationScreen* presentationScreen) {
    gtk_widget_set_hexpand(GTK_WIDGET(presentationScreen), true);
    gtk_widget_set_vexpand(GTK_WIDGET(presentationScreen), true);
}

static void gtk_presentation_screen_realize(GtkWidget* widget) {
    g_return_if_fail(widget != nullptr);
    g_return_if_fail(GTK_IS_PRESENTATION_SCREEN(widget));

    gtk_widget_set_realized(widget, true);

    GtkAllocation allocation;
    gtk_widget_get_allocation(widget, &allocation);
    GdkWindowAttr attributes;
    attributes.window_type = GDK_WINDOW_CHILD;
    attributes.x = allocation.x;
    attributes.y = allocation.y;
    attributes.width = allocation.width;
    attributes.height = allocation.height;

    attributes.wclass = GDK_INPUT_OUTPUT;
    attributes.event_mask = 0;  // Do not receive any events

    gint attributes_mask = GDK_WA_X | GDK_WA_Y;

    gtk_widget_set_window(widget, gdk_window_new(gtk_widget_get_parent_window(widget), &attributes, attributes_mask));
    gdk_window_set_user_data(gtk_widget_get_window(widget), widget);
}

static auto gtk_presentation_screen_draw(GtkWidget* widget, cairo_t* cr) -> gboolean {
    g_return_val_if_fail(widget != nullptr, false);
    g_return_val_if_fail(GTK_IS_PRESENTATION_SCREEN(widget), false);

    GtkPresentationScreen* screen = GTK_PRESENTATION_SCREEN(widget);
    Control* control = screen->view->getControl();
    Settings* settings = control->getSettings();
    (void)settings;

    double x1 = NAN, x2 = NAN, y1 = NAN, y2 = NAN;
    cairo_clip_extents(cr, &x1, &y1, &x2, &y2);
    const double widgetWidth = x2 - x1;
    const double widgetHeight = y2 - y1;

    // Draw background
    const auto backgroundColor = BACKGROUND_COLOR;  // todo: store and fetch from settings
    Util::cairo_set_source_rgbi(cr, backgroundColor);
    cairo_paint(cr);

    // Draw current page
    size_t currentPage = screen->view->getCurrentPage();
    xoj_assert(currentPage < screen->view->getViewPages().size());
    auto& pageView = screen->view->getViewPages()[currentPage];

    const auto pageWidth = pageView->getDisplayWidthDouble();
    const auto pageHeight = pageView->getDisplayHeightDouble();

    const double widgetAspectRatio = widgetWidth / widgetHeight;
    const double pageAspectRatio = pageWidth / pageHeight;

    xoj::util::CairoSaveGuard saveGuard(cr);

    if (pageAspectRatio < widgetAspectRatio) {
        const double displayWidth = pageAspectRatio / widgetAspectRatio * widgetWidth;
        cairo_translate(cr, (widgetWidth - displayWidth) / 2, 0);
    } else if (pageAspectRatio > widgetAspectRatio) {
        const double displayHeight = widgetAspectRatio / pageAspectRatio * widgetHeight;
        cairo_translate(cr, 0, (widgetHeight - displayHeight) / 2);
    }
    pageView->paintPresentationPage(cr);

    // Draw selection, if it exists and stems from this page
    auto selection = pageView->getXournal()->getSelection();
    if (selection && selection->getView() == pageView.get()) {
        selection->paint(cr, gtk_presentation_screen_get_zoom(GTK_PRESENTATION_SCREEN(widget), pageView.get()));
    }

    return true;
}

auto gtk_presentation_screen_new(XournalView* view) -> GtkWidget* {
    GtkPresentationScreen* screen = GTK_PRESENTATION_SCREEN(g_object_new(gtk_presentation_screen_get_type(), nullptr));
    screen->view = view;
    return GTK_WIDGET(screen);
}

auto gtk_presentation_screen_get_zoom(GtkPresentationScreen* screen, XojPageView* pageView) -> double {
    const auto pageWidth = pageView->getWidth();
    const auto pageHeight = pageView->getHeight();

#if GTK_MAJOR_VERSION == 3
    const int widgetWidth = gtk_widget_get_allocated_width(GTK_WIDGET(screen));
    const int widgetHeight = gtk_widget_get_allocated_height(GTK_WIDGET(screen));
#else
    const int widgetWidth = gtk_widget_get_width(GTK_WIDGET(window.get()));
    const int widgetHeight = gtk_widget_get_height(GTK_WIDGET(window.get()));
#endif

    return std::min(widgetWidth / pageWidth, widgetHeight / pageHeight);
}
