#ifndef LUMILA_PROVIDER_H
#define LUMILA_PROVIDER_H

#include <glib.h>
#include <libsoup/soup.h>
#include "../message.h"

typedef enum {
    LUMILA_PROVIDER_OPENAI = 0,
    LUMILA_PROVIDER_ANTHROPIC,
    LUMILA_PROVIDER_GOOGLE,
    LUMILA_PROVIDER_MOONSHOT,
    LUMILA_PROVIDER_OPENROUTER,
    LUMILA_PROVIDER_OLLAMA,
    LUMILA_PROVIDER_DEEPSEEK,
    LUMILA_PROVIDER_MISTRAL
} LumilaProviderType;

typedef void (*LumilaResponseCallback)(const gchar *response, gpointer user_data);
typedef void (*LumilaChunkCallback)(const gchar *chunk, gboolean is_final, gpointer user_data);

typedef struct _LumilaProvider LumilaProvider;

struct _LumilaProvider {
    LumilaProviderType type;
    gint model_id;
    SoupSession *session;
    GCancellable *cancellable;
    SoupMessage *pending_msg;  /* Used for HTTP status check in async callbacks */

    void (*send_message)(LumilaProvider *provider, const gchar *system_prompt,
                         GArray *messages,
                         LumilaResponseCallback callback, gpointer user_data);
    void (*send_message_stream)(LumilaProvider *provider, const gchar *system_prompt,
                                 GArray *messages,
                                 LumilaChunkCallback chunk_cb,
                                 LumilaResponseCallback final_cb,
                                 gpointer user_data);
    void (*cancel)(LumilaProvider *provider);
};

LumilaProvider *lumila_provider_create(LumilaProviderType type);
void lumila_provider_free(LumilaProvider *provider);
void lumila_provider_send_message(LumilaProvider *provider, const gchar *system_prompt,
                                   GArray *messages,
                                   LumilaResponseCallback callback, gpointer user_data);
void lumila_provider_send_message_stream(LumilaProvider *provider, const gchar *system_prompt,
                                          GArray *messages,
                                          LumilaChunkCallback chunk_cb,
                                          LumilaResponseCallback final_cb, gpointer user_data);
void lumila_provider_cancel(LumilaProvider *provider);
const gchar *lumila_provider_get_key_name(LumilaProviderType type);
const gchar *lumila_provider_get_name(LumilaProviderType type);

#endif
