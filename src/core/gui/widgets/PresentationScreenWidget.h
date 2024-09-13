/*
 * Xournal++
 *
 * Presentation widget, which display a copy of the xournal contents
 *
 * @author Xournal++ Team
 * https://github.com/xournalpp/xournalpp
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <glib-object.h>  // for G_TYPE_CHECK_INSTANCE_CAST, G_TYPE_C...
#include <glib.h>         // for G_BEGIN_DECLS, G_END_DECLS
#include <gtk/gtk.h>      // for GtkWidget, GtkWidgetClass

class XojPageView;
class XournalView;

struct _GtkPresentationScreen;
struct _GtkPresentationScreenClass;

G_BEGIN_DECLS

#define GTK_PRESENTATION_SCREEN(obj) \
    G_TYPE_CHECK_INSTANCE_CAST(obj, gtk_presentation_screen_get_type(), GtkPresentationScreen)
#define GTK_PRESENTATION_SCREEN_CLASS(klass) \
    GTK_CHECK_CLASS_CAST(klass, gtk_presentation_screen_get_type(), GtkPresentationScreenClass)
#define GTK_IS_PRESENTATION_SCREEN(obj) G_TYPE_CHECK_INSTANCE_TYPE(obj, gtk_presentation_screen_get_type())

typedef struct _GtkPresentationScreen GtkPresentationScreen;
typedef struct _GtkPresentationScreenClass GtkPresentationScreenClass;

struct _GtkPresentationScreen {
    GtkWidget widget;
    XournalView* view;
};

struct _GtkPresentationScreenClass {
    GtkWidgetClass parent_class;
};

GType gtk_presentation_screen_get_type();

GtkWidget* gtk_presentation_screen_new(XournalView* view);

/**
 * Get the zoom that will be applied to the page view on the presentation screen
 */
double gtk_presentation_screen_get_zoom(GtkPresentationScreen* screen, XojPageView* pageView);

G_END_DECLS
