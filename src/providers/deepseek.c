#include "deepseek.h"
#include "provider_base.h"
#include "model_registry.h"
#include "model_resolve.h"
#include "../config.h"
#include <jansson.h>
#include <string.h>
#include <libsoup/soup.h>

#define DEEPSEEK_API_BASE "https://api.deepseek.com/chat/completions"
#define DEEPSEEK_DEFAULT_MODEL "deepseek-flash"

static void deepseek_send_message(LumilaProvider *provider, const gchar *system_prompt,
                                  GArray *messages,
                                  LumilaResponseCallback callback, gpointer user_data);
#if SOUP_CHECK_VERSION(3, 0, 0)
static void deepseek_send_message_stream(LumilaProvider *provider, const gchar *system_prompt,
                                         GArray *messages,
                                         LumilaChunkCallback chunk_cb,
                                         LumilaResponseCallback final_cb,
                                         gpointer user_data);
#endif
static void deepseek_cancel(LumilaProvider *provider);

typedef struct {
    LumilaProvider base;
    LumilaResponseCallback callback;
    gpointer user_data;
} DeepSeekProvider;

#if SOUP_CHECK_VERSION(3, 0, 0)
static void on_message_sent(GObject *source, GAsyncResult *result, gpointer user_data);
#else
static void on_message_sent(SoupSession *session, SoupMessage *msg, gpointer user_data);
#endif

LumilaProvider *deepseek_provider_new(void)
{
    DeepSeekProvider *provider = g_new0(DeepSeekProvider, 1);

    provider->base.type = LUMILA_PROVIDER_DEEPSEEK;
#if SOUP_CHECK_VERSION(3, 0, 0)
    provider->base.session = soup_session_new_with_options(
        "timeout", (lumila_config_get_timeout(LUMILA_PROVIDER_DEEPSEEK) > 0 ? lumila_config_get_timeout(LUMILA_PROVIDER_DEEPSEEK) : 180),
        NULL);
#else
    provider->base.session = soup_session_new_with_options(
        SOUP_SESSION_TIMEOUT, (lumila_config_get_timeout(LUMILA_PROVIDER_DEEPSEEK) > 0 ? lumila_config_get_timeout(LUMILA_PROVIDER_DEEPSEEK) : 180),
        NULL);
#endif
    provider->base.cancellable = g_cancellable_new();
    provider->base.send_message = deepseek_send_message;
#if SOUP_CHECK_VERSION(3, 0, 0)
    provider->base.send_message_stream = deepseek_send_message_stream;
#else
    provider->base.send_message_stream = NULL;
#endif
    provider->base.cancel = deepseek_cancel;
    provider->callback = NULL;
    provider->user_data = NULL;

    return (LumilaProvider *)provider;
}

static void deepseek_cancel(LumilaProvider *provider)
{
    if (provider && provider->cancellable) {
        g_cancellable_cancel(provider->cancellable);
    }
}

#if SOUP_CHECK_VERSION(3, 0, 0)
static void on_message_sent(GObject *source, GAsyncResult *result, gpointer user_data)
{
    (void)source;

    DeepSeekProvider *provider = (DeepSeekProvider *)user_data;
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

    DeepSeekProvider *provider = (DeepSeekProvider *)user_data;
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

static void deepseek_send_message(LumilaProvider *provider, const gchar *system_prompt,
                                  GArray *messages,
                                  LumilaResponseCallback callback, gpointer user_data)
{
    DeepSeekProvider *ds = (DeepSeekProvider *)provider;

    ds->callback = callback;
    ds->user_data = user_data;

    const gchar *api_key = lumila_config_get_api_key(LUMILA_PROVIDER_DEEPSEEK);
    if (!api_key || !*api_key) {
        if (callback) {
            callback("Error: API key not configured", user_data);
        }
        return;
    }

    // Build JSON request
    json_t *root = json_object();

    const gchar *model_name = lumila_provider_resolve_model(provider, DEEPSEEK_DEFAULT_MODEL);
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
    SoupMessage *msg = soup_message_new("POST", lumila_config_get_endpoint(LUMILA_PROVIDER_DEEPSEEK) ? lumila_config_get_endpoint(LUMILA_PROVIDER_DEEPSEEK) : DEEPSEEK_API_BASE);

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
    SoupMessage *msg = soup_message_new("POST", lumila_config_get_endpoint(LUMILA_PROVIDER_DEEPSEEK) ? lumila_config_get_endpoint(LUMILA_PROVIDER_DEEPSEEK) : DEEPSEEK_API_BASE);

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
static void deepseek_send_message_stream(LumilaProvider *provider, const gchar *system_prompt,
                                         GArray *messages,
                                         LumilaChunkCallback chunk_cb,
                                         LumilaResponseCallback final_cb,
                                         gpointer user_data)
{
    const gchar *api_key = lumila_config_get_api_key(LUMILA_PROVIDER_DEEPSEEK);
    if (!api_key || !*api_key) {
        if (final_cb) {
            final_cb("Error: API key not configured", user_data);
        }
        return;
    }

    json_t *root = json_object();
    const gchar *model_name = lumila_provider_resolve_model(provider, DEEPSEEK_DEFAULT_MODEL);
    json_object_set_new(root, "model", json_string(model_name));
    json_object_set_new(root, "max_tokens", json_integer(lumila_config_get_max_tokens()));
    json_object_set_new(root, "temperature", json_real(lumila_config_get_temperature()));
    json_object_set_new(root, "top_p", json_real(lumila_config_get_top_p()));
    json_object_set_new(root, "frequency_penalty", json_real(lumila_config_get_repeat_penalty() - 1.0));
    json_object_set_new(root, "stream", json_true());
    /* Ask for a final chunk carrying token usage. */
    json_t *stream_opts = json_object();
    json_object_set_new(stream_opts, "include_usage", json_true());
    json_object_set_new(root, "stream_options", stream_opts);

    json_t *msgs = lumila_provider_base_build_messages_openai(system_prompt, messages);
    json_object_set_new(root, "messages", msgs);

    gchar *json_body = json_dumps(root, 0);
    json_decref(root);

    SoupMessage *msg = soup_message_new("POST", lumila_config_get_endpoint(LUMILA_PROVIDER_DEEPSEEK) ? lumila_config_get_endpoint(LUMILA_PROVIDER_DEEPSEEK) : DEEPSEEK_API_BASE);
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
    state->usage_out = &provider->last_usage;
    lumila_provider_base_stream_start(provider->session, msg, state);
}
#endif
