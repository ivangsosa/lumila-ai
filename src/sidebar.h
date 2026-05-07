#ifndef LUMILA_SIDEBAR_H
#define LUMILA_SIDEBAR_H

#include <gtk/gtk.h>

void lumila_sidebar_init(void);
void lumila_sidebar_cleanup(void);
GtkWidget *lumila_sidebar_get_widget(void);

void lumila_sidebar_set_status(const gchar *status);
void lumila_sidebar_set_input_sensitive(gboolean sensitive);
void lumila_sidebar_cancel_request(void);

#endif
