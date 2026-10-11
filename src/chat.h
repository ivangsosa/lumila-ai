#ifndef LUMILA_CHAT_H
#define LUMILA_CHAT_H

#include <gtk/gtk.h>

void lumila_chat_init(void);
void lumila_chat_cleanup(void);
void lumila_chat_set_view(GtkTextView *view);
void lumila_chat_send_message(const gchar *message);
/* allow_refs: process @file references. skip_active_ctx: don't attach the
 * active file to the system context (caller already embedded it).
 * force_code_prompt: use the code-mode prompt even in Plan mode. */
void lumila_chat_send_message_ex(const gchar *message, gboolean allow_refs,
                                 gboolean skip_active_ctx, gboolean force_code_prompt);
void lumila_chat_send_selection(void);
void lumila_chat_send_current_file(void);
void lumila_chat_set_ask_mode(gboolean enabled);
void lumila_chat_set_plan_mode(gboolean enabled);
gboolean lumila_chat_get_plan_mode(void);
gboolean lumila_chat_get_plan_pending(void);
void lumila_chat_execute_plan(void);
gboolean lumila_chat_is_streaming(void);
void lumila_chat_export_markdown(void);
void lumila_chat_set_provider(gint provider_id);
void lumila_chat_new_conversation(void);
void lumila_chat_save_history(void);
void lumila_chat_cancel_request(void);
void lumila_chat_load_conversation(const gchar *filename);
void lumila_chat_set_title(const gchar *title);

#endif
