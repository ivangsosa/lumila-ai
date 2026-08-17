#include "anthropic.h"
#include "provider_base.h"
#include "../config.h"
#include <jansson.h>
#include <string.h>
#include <libsoup/soup.h>

#define ANTHROPIC_API_BASE "https://api.anthropic.com/v1/messages"
#define ANTHROPIC_VERSION "2023-06-01"

static void anthropic_send_message(LumilaProvider *provider, const gchar *system_prompt,
                                    GArray *messages,
                                    LumilaResponseCallback callback, gpointer user_data);
static void anthropic_send_message_stream(LumilaProvider *provider, const gchar *system_prompt,
                                           GArray *messages,
                                           LumilaChunkCallback chunk_cb,
                                           LumilaResponseCallback final_cb,
                                           gpointer user_data);
static void anthropic_cancel(LumilaProvider *provider);

typedef struct {
    LumilaProvider base;
    LumilaResponseCallback callback;
    gpointer user_data;
} AnthropicProvider;

#if SOUP_CHECK_VERSION(3, 0, 0)
static void on_message_sent(GObject *source, GAsyncResult *result, gpointer user_data);
#else
static void on_message_sent(SoupSession *session, SoupMessage *msg, gpointer user_data);
#endif

LumilaProvider *anthropic_provider_new(void)
{
    AnthropicProvider *provider = g_new0(AnthropicProvider, 1);

    provider->base.type = LUMILA_PROVIDER_ANTHROPIC;
#if SOUP_CHECK_VERSION(3, 0, 0)
    provider->base.session = soup_session_new_with_options(
        "timeout", (lumila_config_get_timeout(LUMILA_PROVIDER_ANTHROPIC) > 0 ? lumila_config_get_timeout(LUMILA_PROVIDER_ANTHROPIC) : 60),
        NULL);
#else
    provider->base.session = soup_session_new_with_options(
        SOUP_SESSION_TIMEOUT, (lumila_config_get_timeout(LUMILA_PROVIDER_ANTHROPIC) > 0 ? lumila_config_get_timeout(LUMILA_PROVIDER_ANTHROPIC) : 60),
        NULL);
#endif
    provider->base.cancellable = g_cancellable_new();
    provider->base.send_message = anthropic_send_message;
#if SOUP_CHECK_VERSION(3, 0, 0)
    provider->base.send_message_stream = anthropic_send_message_stream;
#else
    provider->base.send_message_stream = NULL;
#endif
    provider->base.cancel = anthropic_cancel;
    provider->callback = NULL;
    provider->user_data = NULL;

    return (LumilaProvider *)provider;
}

static void anthropic_cancel(LumilaProvider *provider)
{
    if (provider && provider->cancellable) {
        g_cancellable_cancel(provider->cancellable);
    }
}

#if SOUP_CHECK_VERSION(3, 0, 0)
static void on_message_sent(GObject *source, GAsyncResult *result, gpointer user_data)
{
    (void)source;

    AnthropicProvider *provider = (AnthropicProvider *)user_data;
    LumilaResponseCallback callback = provider->callback;
    gpointer cb_data = provider->user_data;

    GError *error = NULL;
    GBytes *bytes = soup_session_send_and_read_finish(SOUP_SESSION(source), result, &error);

    gchar *response_text = NULL;

    if (error) {
        response_text = g_strdup_printf("Error: %s", error->message);
        g_error_free(error);
    } else if (bytes) {
        gsize size;
        const gchar *data = g_bytes_get_data(bytes, &size);
        if (provider->base.pending_msg) {
            guint status = soup_message_get_status(provider->base.pending_msg);
            if (status != 200) {
                if (lumila_provider_base_is_retriable_status(status)) {
                    response_text = g_strdup_printf("Transient HTTP Error %u: %s (please retry)", status,
                        soup_message_get_reason_phrase(provider->base.pending_msg));
                } else {
                    response_text = g_strdup_printf("HTTP Error %u: %s", status,
                        soup_message_get_reason_phrase(provider->base.pending_msg));
                }
            }
        }
        if (!response_text) {
            response_text = lumila_provider_base_parse_anthropic(data, size);
        }
        g_bytes_unref(bytes);
    }

    if (callback) {
        callback(response_text, cb_data);
    }

    if (response_text) {
        g_free(response_text);
    }

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

    AnthropicProvider *provider = (AnthropicProvider *)user_data;
    LumilaResponseCallback callback = provider->callback;
    gpointer cb_data = provider->user_data;

    const gchar *response_text = NULL;

    if (soup_message_get_status(msg) == 200) {
        SoupBuffer *buffer = soup_message_body_flatten(SOUP_MESSAGE(msg)->response_body);

        json_error_t error;
        json_t *root = json_loadb(buffer->data, buffer->length, 0, &error);

        if (root) {
            json_t *content = json_object_get(root, "content");
            if (content && json_is_array(content) && json_array_size(content) > 0) {
                json_t *first = json_array_get(content, 0);
                json_t *text = json_object_get(first, "text");
                if (text && json_is_string(text)) {
                    response_text = json_string_value(text);
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

static void anthropic_send_message(LumilaProvider *provider, const gchar *system_prompt,
                                    GArray *messages,
                                    LumilaResponseCallback callback, gpointer user_data)
{
    AnthropicProvider *ant = (AnthropicProvider *)provider;

    ant->callback = callback;
    ant->user_data = user_data;

    const gchar *api_key = lumila_config_get_api_key(LUMILA_PROVIDER_ANTHROPIC);
    if (!api_key || !*api_key) {
        if (callback) {
            callback("Error: API key not configured", user_data);
        }
        return;
    }

    // Build JSON request for Anthropic API
    json_t *root = json_object();

    // Select model based on model_id
    const gchar *model_name;
    switch (provider->model_id) {
        case 0: model_name = "claude-sonnet-5"; break;   // Claude Sonnet 5
        case 1: model_name = "claude-opus-5"; break;     // Claude Opus 5
        case 2: model_name = "claude-haiku-4-5"; break;  // Claude Haiku 4.5
        default: model_name = "claude-sonnet-5"; break;
    }
    const gchar *custom = lumila_config_get_custom_model(LUMILA_PROVIDER_ANTHROPIC);
    if (custom) model_name = custom;

    json_object_set_new(root, "model", json_string(model_name));
    json_object_set_new(root, "max_tokens", json_integer(lumila_config_get_max_tokens()));
    json_object_set_new(root, "temperature", json_real(lumila_config_get_temperature()));
    json_object_set_new(root, "top_p", json_real(lumila_config_get_top_p()));

    /* Anthropic uses a top-level "system" field, not a system role in messages */
    if (system_prompt && *system_prompt) {
        json_object_set_new(root, "system", json_string(system_prompt));
    }
    json_t *msgs = lumila_provider_base_build_messages_anthropic(messages);
    json_object_set_new(root, "messages", msgs);

    gchar *json_body = json_dumps(root, 0);
    json_decref(root);

#if SOUP_CHECK_VERSION(3, 0, 0)
    SoupMessage *msg = soup_message_new("POST", lumila_config_get_endpoint(LUMILA_PROVIDER_ANTHROPIC) ? lumila_config_get_endpoint(LUMILA_PROVIDER_ANTHROPIC) : ANTHROPIC_API_BASE);

    soup_message_headers_append(soup_message_get_request_headers(msg), "Content-Type", "application/json");
    soup_message_headers_append(soup_message_get_request_headers(msg), "x-api-key", api_key);
    soup_message_headers_append(soup_message_get_request_headers(msg), "anthropic-version", ANTHROPIC_VERSION);

    GBytes *body_bytes = g_bytes_new(json_body, strlen(json_body));
    soup_message_set_request_body_from_bytes(msg, "application/json", body_bytes);
    g_bytes_unref(body_bytes);
    g_free(json_body);

    // Send async
    soup_session_send_and_read_async(provider->session, msg, G_PRIORITY_DEFAULT, 
                                      provider->cancellable, on_message_sent, provider);
    provider->pending_msg = msg;
#else
    SoupMessage *msg = soup_message_new("POST", lumila_config_get_endpoint(LUMILA_PROVIDER_ANTHROPIC) ? lumila_config_get_endpoint(LUMILA_PROVIDER_ANTHROPIC) : ANTHROPIC_API_BASE);

    soup_message_headers_append(msg->request_headers, "Content-Type", "application/json");
    soup_message_headers_append(msg->request_headers, "x-api-key", api_key);
    soup_message_headers_append(msg->request_headers, "anthropic-version", ANTHROPIC_VERSION);

    soup_message_body_append(msg->request_body, SOUP_MEMORY_COPY, json_body, strlen(json_body));
    g_free(json_body);

    // Send async
    soup_session_queue_message(provider->session, msg, on_message_sent, provider);
#endif
}

#if SOUP_CHECK_VERSION(3, 0, 0)
/* Parse Anthropic SSE chunk: data: {"type":"content_block_delta","delta":{"type":"text_delta","text":"..."}} */
static gchar *anthropic_parse_stream_chunk(const gchar *data, gsize len)
{
    (void)len;
    /* data here is the full line (may or may not have "data: " prefix - handled by base) */
    json_error_t jerr;
    json_t *root = json_loads(data, 0, &jerr);
    if (!root) return NULL;

    gchar *result = NULL;
    json_t *type = json_object_get(root, "type");
    if (type && json_is_string(type) &&
        g_strcmp0(json_string_value(type), "content_block_delta") == 0) {
        json_t *delta = json_object_get(root, "delta");
        if (delta) {
            json_t *text = json_object_get(delta, "text");
            if (text && json_is_string(text)) {
                result = g_strdup(json_string_value(text));
            }
        }
    }
    json_decref(root);
    return result;
}

static void anthropic_send_message_stream(LumilaProvider *provider, const gchar *system_prompt,
                                           GArray *messages,
                                           LumilaChunkCallback chunk_cb,
                                           LumilaResponseCallback final_cb,
                                           gpointer user_data)
{
    const gchar *api_key = lumila_config_get_api_key(LUMILA_PROVIDER_ANTHROPIC);
    if (!api_key || !*api_key) {
        if (final_cb) {
            final_cb("Error: API key not configured", user_data);
        }
        return;
    }

    json_t *root = json_object();
    const gchar *model_name;
    switch (provider->model_id) {
        case 0: model_name = "claude-sonnet-5"; break;
        case 1: model_name = "claude-opus-5"; break;
        case 2: model_name = "claude-haiku-4-5"; break;
        default: model_name = "claude-sonnet-5"; break;
    }
    const gchar *custom = lumila_config_get_custom_model(LUMILA_PROVIDER_ANTHROPIC);
    if (custom) model_name = custom;

    json_object_set_new(root, "model", json_string(model_name));
    json_object_set_new(root, "max_tokens", json_integer(lumila_config_get_max_tokens()));
    json_object_set_new(root, "temperature", json_real(lumila_config_get_temperature()));
    json_object_set_new(root, "top_p", json_real(lumila_config_get_top_p()));
    json_object_set_new(root, "stream", json_true());

    /* Anthropic uses a top-level "system" field, not a system role in messages */
    if (system_prompt && *system_prompt) {
        json_object_set_new(root, "system", json_string(system_prompt));
    }
    json_t *msgs = lumila_provider_base_build_messages_anthropic(messages);
    json_object_set_new(root, "messages", msgs);

    gchar *json_body = json_dumps(root, 0);
    json_decref(root);

    SoupMessage *msg = soup_message_new("POST", lumila_config_get_endpoint(LUMILA_PROVIDER_ANTHROPIC) ? lumila_config_get_endpoint(LUMILA_PROVIDER_ANTHROPIC) : ANTHROPIC_API_BASE);
    soup_message_headers_append(soup_message_get_request_headers(msg), "Content-Type", "application/json");
    soup_message_headers_append(soup_message_get_request_headers(msg), "x-api-key", api_key);
    soup_message_headers_append(soup_message_get_request_headers(msg), "anthropic-version", ANTHROPIC_VERSION);

    GBytes *body_bytes = g_bytes_new(json_body, strlen(json_body));
    soup_message_set_request_body_from_bytes(msg, "application/json", body_bytes);
    g_bytes_unref(body_bytes);
    g_free(json_body);

    LumilaStreamState *state = lumila_stream_state_new(provider->cancellable,
                                                        chunk_cb, final_cb, user_data);
    state->parse_chunk = anthropic_parse_stream_chunk;
    lumila_provider_base_stream_start(provider->session, msg, state);
}
#endif
