#ifndef LUMILA_CHAT_H
#define LUMILA_CHAT_H

#include <gtk/gtk.h>

void lumila_chat_init(void);
void lumila_chat_cleanup(void);
void lumila_chat_set_view(GtkTextView *view);
void lumila_chat_send_message(const gchar *message);
void lumila_chat_send_selection(void);
void lumila_chat_export_markdown(void);
void lumila_chat_set_provider(gint provider_id);
void lumila_chat_new_conversation(void);
void lumila_chat_save_history(void);
void lumila_chat_cancel_request(void);
void lumila_chat_load_conversation(const gchar *filename);
void lumila_chat_set_title(const gchar *title);

#endif
