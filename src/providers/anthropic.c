#include "anthropic.h"
#include "provider_base.h"
#include "model_registry.h"
#include "model_resolve.h"
#include "../config.h"
#include <jansson.h>
#include <string.h>
#include <libsoup/soup.h>

#define ANTHROPIC_API_BASE "https://api.anthropic.com/v1/messages"
#define ANTHROPIC_VERSION "2023-06-01"
#define ANTHROPIC_DEFAULT_MODEL "claude-sonnet-5-5"

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
        "timeout", (lumila_config_get_timeout(LUMILA_PROVIDER_ANTHROPIC) > 0 ? lumila_config_get_timeout(LUMILA_PROVIDER_ANTHROPIC) : 180),
        NULL);
#else
    provider->base.session = soup_session_new_with_options(
        SOUP_SESSION_TIMEOUT, (lumila_config_get_timeout(LUMILA_PROVIDER_ANTHROPIC) > 0 ? lumila_config_get_timeout(LUMILA_PROVIDER_ANTHROPIC) : 180),
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
    AnthropicProvider *provider = (AnthropicProvider *)user_data;
    LumilaResponseCallback callback = provider->callback;
    gpointer cb_data = provider->user_data;

    gboolean cancelled = FALSE;
    gchar *response_text = lumila_provider_base_finish_send_and_read(
        SOUP_SESSION(source), result, lumila_provider_base_parse_anthropic,
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

    const gchar *model_name = lumila_provider_resolve_model(provider, ANTHROPIC_DEFAULT_MODEL);
    json_object_set_new(root, "model", json_string(model_name));
    json_object_set_new(root, "max_tokens", json_integer(lumila_config_get_max_tokens()));

    /* Claude 5.x rejects non-default temperature/top_p; newer models
     * (incl. Haiku 4.5) reject temperature+top_p together, so top_p is
     * never sent. Sampling params are only sent for models that admit
     * them. Adaptive thinking is the default on 5.x; output_config.effort
     * selects the reasoning effort. */
    if (!(provider->model_flags & LUMILA_MODEL_FLAG_NO_SAMPLING)) {
        json_object_set_new(root, "temperature", json_real(lumila_config_get_temperature()));
    }
    if (provider->model_flags & LUMILA_MODEL_FLAG_ANTHROPIC_EFFORT) {
        const gchar *effort = lumila_config_get_reasoning_effort();
        if (effort && *effort && g_strcmp0(effort, "none") != 0) {
            json_t *output_config = json_object();
            json_object_set_new(output_config, "effort", json_string(effort));
            json_object_set_new(root, "output_config", output_config);
        }
    }

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
    const gchar *model_name = lumila_provider_resolve_model(provider, ANTHROPIC_DEFAULT_MODEL);
    json_object_set_new(root, "model", json_string(model_name));
    json_object_set_new(root, "max_tokens", json_integer(lumila_config_get_max_tokens()));

    if (!(provider->model_flags & LUMILA_MODEL_FLAG_NO_SAMPLING)) {
        json_object_set_new(root, "temperature", json_real(lumila_config_get_temperature()));
    }
    if (provider->model_flags & LUMILA_MODEL_FLAG_ANTHROPIC_EFFORT) {
        const gchar *effort = lumila_config_get_reasoning_effort();
        if (effort && *effort && g_strcmp0(effort, "none") != 0) {
            json_t *output_config = json_object();
            json_object_set_new(output_config, "effort", json_string(effort));
            json_object_set_new(root, "output_config", output_config);
        }
    }
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
    state->usage_out = &provider->last_usage;
    lumila_provider_base_stream_start(provider->session, msg, state);
}
#endif
