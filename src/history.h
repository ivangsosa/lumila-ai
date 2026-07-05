#ifndef LUMILA_HISTORY_H
#define LUMILA_HISTORY_H

#include <gtk/gtk.h>
#include "message.h"

typedef struct {
    gchar *filename;
    gchar *title;
    gchar *created_at;
    gchar *model_name;
} LumilaHistoryEntry;

GList *lumila_history_list(void);
void lumila_history_entry_free(LumilaHistoryEntry *entry);
gint lumila_history_compare_by_date(LumilaHistoryEntry *a, LumilaHistoryEntry *b);

GArray *lumila_history_load_messages(const gchar *filename);
void lumila_history_free_messages(GArray *msgs);
gchar *lumila_history_load_title(const gchar *filename);
gboolean lumila_history_delete(const gchar *filename);
gboolean lumila_history_rename(const gchar *old_filename, const gchar *new_title);

#endif
