#ifndef LUMILA_HISTORY_H
#define LUMILA_HISTORY_H

#include <gtk/gtk.h>

typedef struct {
    gchar *filename;
    gchar *title;
    gchar *created_at;
    gchar *model_name;
} LumilaHistoryEntry;

GList *lumila_history_list(void);
void lumila_history_entry_free(LumilaHistoryEntry *entry);

GArray *lumila_history_load_messages(const gchar *filename);
gboolean lumila_history_delete(const gchar *filename);
gboolean lumila_history_rename(const gchar *old_filename, const gchar *new_title);

#endif
