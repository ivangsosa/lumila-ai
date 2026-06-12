#ifndef LUMILA_CHAT_UI_H
#define LUMILA_CHAT_UI_H

#include <gtk/gtk.h>

void lumila_chat_ui_init(GtkTextView *view);
void lumila_chat_ui_append_user_message(GtkTextView *view, const gchar *message);
void lumila_chat_ui_append_ai_message(GtkTextView *view, const gchar *message);
void lumila_chat_ui_clear(GtkTextView *view);

/* Streaming UI */
void lumila_chat_ui_stream_start(GtkTextView *view);
void lumila_chat_ui_stream_append(GtkTextView *view, const gchar *chunk);
void lumila_chat_ui_stream_end(GtkTextView *view);
void lumila_chat_ui_stream_end_and_render(GtkTextView *view, const gchar *full_message);

#endif
