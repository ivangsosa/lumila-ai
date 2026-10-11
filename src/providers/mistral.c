#include "mistral.h"
#include "provider_base.h"
#include "model_registry.h"
#include "model_resolve.h"
#include "../config.h"
#include <jansson.h>
#include <string.h>
#include <libsoup/soup.h>

#define MISTRAL_API_BASE "https://api.mistral.ai/v1/chat/completions"
#define MISTRAL_DEFAULT_MODEL "mistral-large-4"

static void mistral_send_message(LumilaProvider *provider, const gchar *system_prompt,
                                 GArray *messages,
                                 LumilaResponseCallback callback, gpointer user_data);
#if SOUP_CHECK_VERSION(3, 0, 0)
static void mistral_send_message_stream(LumilaProvider *provider, const gchar *system_prompt,
                                        GArray *messages,
                                        LumilaChunkCallback chunk_cb,
                                        LumilaResponseCallback final_cb,
                                        gpointer user_data);
#endif
static void mistral_cancel(LumilaProvider *provider);

typedef struct {
    LumilaProvider base;
    LumilaResponseCallback callback;
    gpointer user_data;
} MistralProvider;

#if SOUP_CHECK_VERSION(3, 0, 0)
static void on_message_sent(GObject *source, GAsyncResult *result, gpointer user_data);
#else
static void on_message_sent(SoupSession *session, SoupMessage *msg, gpointer user_data);
#endif

LumilaProvider *mistral_provider_new(void)
{
    MistralProvider *provider = g_new0(MistralProvider, 1);

    provider->base.type = LUMILA_PROVIDER_MISTRAL;
#if SOUP_CHECK_VERSION(3, 0, 0)
    provider->base.session = soup_session_new_with_options(
        "timeout", (lumila_config_get_timeout(LUMILA_PROVIDER_MISTRAL) > 0 ? lumila_config_get_timeout(LUMILA_PROVIDER_MISTRAL) : 180),
        NULL);
#else
    provider->base.session = soup_session_new_with_options(
        SOUP_SESSION_TIMEOUT, (lumila_config_get_timeout(LUMILA_PROVIDER_MISTRAL) > 0 ? lumila_config_get_timeout(LUMILA_PROVIDER_MISTRAL) : 180),
        NULL);
#endif
    provider->base.cancellable = g_cancellable_new();
    provider->base.send_message = mistral_send_message;
#if SOUP_CHECK_VERSION(3, 0, 0)
    provider->base.send_message_stream = mistral_send_message_stream;
#else
    provider->base.send_message_stream = NULL;
#endif
    provider->base.cancel = mistral_cancel;
    provider->callback = NULL;
    provider->user_data = NULL;

    return (LumilaProvider *)provider;
}

static void mistral_cancel(LumilaProvider *provider)
{
    if (provider && provider->cancellable) {
        g_cancellable_cancel(provider->cancellable);
    }
}

#if SOUP_CHECK_VERSION(3, 0, 0)
static void on_message_sent(GObject *source, GAsyncResult *result, gpointer user_data)
{
    (void)source;

    MistralProvider *provider = (MistralProvider *)user_data;
    LumilaResponseCallback callback = provider->callback;
    gpointer cb_data = provider->user_data;

    gboolean cancelled = FALSE;
    gchar *response_text = lumila_provider_base_finish_send_and_read(
        SOUP_SESSION(source), result, lumila_provider_base_parse_openai,
        provider->base.pending_msg, &provider->base.last_usage, &cancelled);

    if (callback && !cancelled) {
        callback(response_text, cb_data);
    }

    g_free(response_text);

    provider->callback = NULL;
    provider->user_data = NULL;
    if (provider->base.pending_msg) {
        g_object_unref(provider->base.pending_msg);
        provider->base.pending_msg = NULL;
    }
}
#else
static void on_message_sent(SoupSession *session, SoupMessage *msg, gpointer user_data)
{
    (void)session;

    MistralProvider *provider = (MistralProvider *)user_data;
    LumilaResponseCallback callback = provider->callback;
    gpointer cb_data = provider->user_data;

    const gchar *response_text = NULL;

    if (soup_message_get_status(msg) == 200) {
        SoupBuffer *buffer = soup_message_body_flatten(SOUP_MESSAGE(msg)->response_body);

        json_error_t error;
        json_t *root = json_loadb(buffer->data, buffer->length, 0, &error);

        if (root) {
            json_t *choices = json_object_get(root, "choices");
            if (choices && json_is_array(choices) && json_array_size(choices) > 0) {
                json_t *first = json_array_get(choices, 0);
                json_t *message_obj = json_object_get(first, "message");
                if (message_obj) {
                    json_t *content = json_object_get(message_obj, "content");
                    if (content && json_is_string(content)) {
                        response_text = json_string_value(content);
                    }
                }
            }
            json_decref(root);
        }
        soup_buffer_free(buffer);
    } else {
        response_text = "Error: HTTP request failed (non-200 status)";
    }

    if (callback) {
        callback(response_text, cb_data);
    }

    provider->callback = NULL;
    provider->user_data = NULL;
}
#endif

static void mistral_send_message(LumilaProvider *provider, const gchar *system_prompt,
                                 GArray *messages,
                                 LumilaResponseCallback callback, gpointer user_data)
{
    MistralProvider *m = (MistralProvider *)provider;

    m->callback = callback;
    m->user_data = user_data;

    const gchar *api_key = lumila_config_get_api_key(LUMILA_PROVIDER_MISTRAL);
    if (!api_key || !*api_key) {
        if (callback) {
            callback("Error: API key not configured", user_data);
        }
        return;
    }

    // Build JSON request
    json_t *root = json_object();

    const gchar *model_name = lumila_provider_resolve_model(provider, MISTRAL_DEFAULT_MODEL);
    json_object_set_new(root, "model", json_string(model_name));
    json_object_set_new(root, "max_tokens", json_integer(lumila_config_get_max_tokens()));
    json_object_set_new(root, "temperature", json_real(lumila_config_get_temperature()));
    json_object_set_new(root, "top_p", json_real(lumila_config_get_top_p()));
    json_object_set_new(root, "frequency_penalty", json_real(lumila_config_get_repeat_penalty() - 1.0));

    json_t *msgs = lumila_provider_base_build_messages_openai(system_prompt, messages);
    json_object_set_new(root, "messages", msgs);

    gchar *json_body = json_dumps(root, 0);
    json_decref(root);

#if SOUP_CHECK_VERSION(3, 0, 0)
    SoupMessage *msg = soup_message_new("POST", lumila_config_get_endpoint(LUMILA_PROVIDER_MISTRAL) ? lumila_config_get_endpoint(LUMILA_PROVIDER_MISTRAL) : MISTRAL_API_BASE);

    soup_message_headers_append(soup_message_get_request_headers(msg), "Content-Type", "application/json");

    gchar *auth_header = g_strdup_printf("Bearer %s", api_key);
    soup_message_headers_append(soup_message_get_request_headers(msg), "Authorization", auth_header);
    g_free(auth_header);

    GBytes *body_bytes = g_bytes_new(json_body, strlen(json_body));
    soup_message_set_request_body_from_bytes(msg, "application/json", body_bytes);
    g_bytes_unref(body_bytes);
    g_free(json_body);

    soup_session_send_and_read_async(provider->session, msg, G_PRIORITY_DEFAULT,
                                      provider->cancellable, on_message_sent, provider);
    provider->pending_msg = msg;
#else
    SoupMessage *msg = soup_message_new("POST", lumila_config_get_endpoint(LUMILA_PROVIDER_MISTRAL) ? lumila_config_get_endpoint(LUMILA_PROVIDER_MISTRAL) : MISTRAL_API_BASE);

    soup_message_headers_append(msg->request_headers, "Content-Type", "application/json");

    gchar *auth_header = g_strdup_printf("Bearer %s", api_key);
    soup_message_headers_append(msg->request_headers, "Authorization", auth_header);
    g_free(auth_header);

    soup_message_body_append(msg->request_body, SOUP_MEMORY_COPY, json_body, strlen(json_body));
    g_free(json_body);

    soup_session_queue_message(provider->session, msg, on_message_sent, provider);
#endif
}

#if SOUP_CHECK_VERSION(3, 0, 0)
static void mistral_send_message_stream(LumilaProvider *provider, const gchar *system_prompt,
                                        GArray *messages,
                                        LumilaChunkCallback chunk_cb,
                                        LumilaResponseCallback final_cb,
                                        gpointer user_data)
{
    const gchar *api_key = lumila_config_get_api_key(LUMILA_PROVIDER_MISTRAL);
    if (!api_key || !*api_key) {
        if (final_cb) {
            final_cb("Error: API key not configured", user_data);
        }
        return;
    }

    json_t *root = json_object();
    const gchar *model_name = lumila_provider_resolve_model(provider, MISTRAL_DEFAULT_MODEL);
    json_object_set_new(root, "model", json_string(model_name));
    json_object_set_new(root, "max_tokens", json_integer(lumila_config_get_max_tokens()));
    json_object_set_new(root, "temperature", json_real(lumila_config_get_temperature()));
    json_object_set_new(root, "top_p", json_real(lumila_config_get_top_p()));
    json_object_set_new(root, "frequency_penalty", json_real(lumila_config_get_repeat_penalty() - 1.0));
    json_object_set_new(root, "stream", json_true());

    json_t *msgs = lumila_provider_base_build_messages_openai(system_prompt, messages);
    json_object_set_new(root, "messages", msgs);

    gchar *json_body = json_dumps(root, 0);
    json_decref(root);

    SoupMessage *msg = soup_message_new("POST", lumila_config_get_endpoint(LUMILA_PROVIDER_MISTRAL) ? lumila_config_get_endpoint(LUMILA_PROVIDER_MISTRAL) : MISTRAL_API_BASE);
    soup_message_headers_append(soup_message_get_request_headers(msg), "Content-Type", "application/json");
    gchar *auth_header = g_strdup_printf("Bearer %s", api_key);
    soup_message_headers_append(soup_message_get_request_headers(msg), "Authorization", auth_header);
    g_free(auth_header);
    GBytes *body_bytes = g_bytes_new(json_body, strlen(json_body));
    soup_message_set_request_body_from_bytes(msg, "application/json", body_bytes);
    g_bytes_unref(body_bytes);
    g_free(json_body);

    LumilaStreamState *state = lumila_stream_state_new(provider->cancellable,
                                                        chunk_cb, final_cb, user_data);
    lumila_provider_base_stream_start(provider->session, msg, state);
}
#endif
