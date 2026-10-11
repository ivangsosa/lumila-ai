#include "google.h"
#include "provider_base.h"
#include "model_registry.h"
#include "model_resolve.h"
#include "../config.h"
#include <jansson.h>
#include <string.h>
#include <libsoup/soup.h>

#define GOOGLE_API_BASE "https://generativelanguage.googleapis.com/v1beta/models/"
#define GOOGLE_DEFAULT_MODEL "gemini-3.8-flash"

static void google_send_message(LumilaProvider *provider, const gchar *system_prompt,
                                 GArray *messages,
                                 LumilaResponseCallback callback, gpointer user_data);
static void google_send_message_stream(LumilaProvider *provider, const gchar *system_prompt,
                                        GArray *messages,
                                        LumilaChunkCallback chunk_cb,
                                        LumilaResponseCallback final_cb,
                                        gpointer user_data);static void google_cancel(LumilaProvider *provider);

typedef struct {
    LumilaProvider base;
    LumilaResponseCallback callback;
    gpointer user_data;
} GoogleProvider;

#if SOUP_CHECK_VERSION(3, 0, 0)
static void on_message_sent(GObject *source, GAsyncResult *result, gpointer user_data);
#else
static void on_message_sent(SoupSession *session, SoupMessage *msg, gpointer user_data);
#endif

LumilaProvider *google_provider_new(void)
{
    GoogleProvider *provider = g_new0(GoogleProvider, 1);
    
    provider->base.type = LUMILA_PROVIDER_GOOGLE;
#if SOUP_CHECK_VERSION(3, 0, 0)
    provider->base.session = soup_session_new_with_options(
        "timeout", (lumila_config_get_timeout(LUMILA_PROVIDER_GOOGLE) > 0 ? lumila_config_get_timeout(LUMILA_PROVIDER_GOOGLE) : 180),
        NULL);
#else
    provider->base.session = soup_session_new_with_options(
        SOUP_SESSION_TIMEOUT, (lumila_config_get_timeout(LUMILA_PROVIDER_GOOGLE) > 0 ? lumila_config_get_timeout(LUMILA_PROVIDER_GOOGLE) : 180),
        NULL);
#endif
    provider->base.cancellable = g_cancellable_new();
    provider->base.send_message = google_send_message;
#if SOUP_CHECK_VERSION(3, 0, 0)
    provider->base.send_message_stream = google_send_message_stream;
#else
    provider->base.send_message_stream = NULL;
#endif
    provider->base.cancel = google_cancel;
    provider->callback = NULL;
    provider->user_data = NULL;
    
    return (LumilaProvider *)provider;
}

static void google_cancel(LumilaProvider *provider)
{
    if (provider && provider->cancellable) {
        g_cancellable_cancel(provider->cancellable);
    }
}

#if SOUP_CHECK_VERSION(3, 0, 0)
static void on_message_sent(GObject *source, GAsyncResult *result, gpointer user_data)
{
    GoogleProvider *provider = (GoogleProvider *)user_data;
    LumilaResponseCallback callback = provider->callback;
    gpointer cb_data = provider->user_data;

    gboolean cancelled = FALSE;
    gchar *response_text = lumila_provider_base_finish_send_and_read(
        SOUP_SESSION(source), result, lumila_provider_base_parse_google,
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

    GoogleProvider *provider = (GoogleProvider *)user_data;
    LumilaResponseCallback callback = provider->callback;
    gpointer cb_data = provider->user_data;

    const gchar *response_text = NULL;

    if (soup_message_get_status(msg) == 200) {
        SoupBuffer *buffer = soup_message_body_flatten(SOUP_MESSAGE(msg)->response_body);

        json_error_t error;
        json_t *root = json_loadb(buffer->data, buffer->length, 0, &error);

        if (root) {
            json_t *candidates = json_object_get(root, "candidates");
            if (candidates && json_is_array(candidates) && json_array_size(candidates) > 0) {
                json_t *first = json_array_get(candidates, 0);
                json_t *content = json_object_get(first, "content");
                if (content) {
                    json_t *parts = json_object_get(content, "parts");
                    if (parts && json_is_array(parts) && json_array_size(parts) > 0) {
                        json_t *first_part = json_array_get(parts, 0);
                        json_t *text = json_object_get(first_part, "text");
                        if (text && json_is_string(text)) {
                            response_text = json_string_value(text);
                        }
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

static void google_send_message(LumilaProvider *provider, const gchar *system_prompt,
                                 GArray *messages,
                                 LumilaResponseCallback callback, gpointer user_data)
{
    GoogleProvider *google = (GoogleProvider *)provider;

    google->callback = callback;
    google->user_data = user_data;

    const gchar *api_key = lumila_config_get_api_key(LUMILA_PROVIDER_GOOGLE);
    if (!api_key || !*api_key) {
        if (callback) {
            callback("Error: API key not configured", user_data);
        }
        return;
    }

    const gchar *model_name = lumila_provider_resolve_model(provider, GOOGLE_DEFAULT_MODEL);

    // Build URL (API key sent via header for security)
    const gchar *base = lumila_config_get_endpoint(LUMILA_PROVIDER_GOOGLE);
    if (!base) base = GOOGLE_API_BASE;
    gchar *url = g_strdup_printf("%s%s:generateContent", base, model_name);

    // Build JSON request for Gemini API
    json_t *root = json_object();

    /* System prompt goes in systemInstruction, conversation in contents */
    if (system_prompt && *system_prompt) {
        json_t *sys_instruction = json_object();
        json_t *sys_parts = json_array();
        json_t *sys_part = json_object();
        json_object_set_new(sys_part, "text", json_string(system_prompt));
        json_array_append_new(sys_parts, sys_part);
        json_object_set_new(sys_instruction, "parts", sys_parts);
        json_object_set_new(root, "systemInstruction", sys_instruction);
    }

    json_t *contents = lumila_provider_base_build_messages_google(messages);
    json_object_set_new(root, "contents", contents);

    // Add generation config
    json_t *gen_config = json_object();
    json_object_set_new(gen_config, "temperature", json_real(lumila_config_get_temperature()));
    json_object_set_new(gen_config, "maxOutputTokens", json_integer(lumila_config_get_max_tokens()));
    json_object_set_new(gen_config, "topP", json_real(lumila_config_get_top_p()));
    if (provider->model_flags & LUMILA_MODEL_FLAG_THINKING_LEVEL) {
        const gchar *level = lumila_config_get_thinking_level();
        if (level && *level) {
            json_t *thinking = json_object();
            json_object_set_new(thinking, "thinkingLevel", json_string(level));
            json_object_set_new(gen_config, "thinkingConfig", thinking);
        }
    }
    json_object_set_new(root, "generationConfig", gen_config);

    gchar *json_body = json_dumps(root, 0);
    json_decref(root);

#if SOUP_CHECK_VERSION(3, 0, 0)
    SoupMessage *msg = soup_message_new("POST", url);

    soup_message_headers_append(soup_message_get_request_headers(msg), "Content-Type", "application/json");
    soup_message_headers_append(soup_message_get_request_headers(msg), "x-goog-api-key", api_key);

    GBytes *body_bytes = g_bytes_new(json_body, strlen(json_body));
    soup_message_set_request_body_from_bytes(msg, "application/json", body_bytes);
    g_bytes_unref(body_bytes);
    g_free(json_body);
    g_free(url);

    // Send async
    soup_session_send_and_read_async(provider->session, msg, G_PRIORITY_DEFAULT, 
                                      provider->cancellable, on_message_sent, provider);
    provider->pending_msg = msg;
#else
    SoupMessage *msg = soup_message_new("POST", url);

    soup_message_headers_append(msg->request_headers, "Content-Type", "application/json");
    soup_message_headers_append(msg->request_headers, "x-goog-api-key", api_key);

    soup_message_body_append(msg->request_body, SOUP_MEMORY_COPY, json_body, strlen(json_body));
    g_free(json_body);
    g_free(url);

    // Send async
    soup_session_queue_message(provider->session, msg, on_message_sent, provider);
#endif
}

#if SOUP_CHECK_VERSION(3, 0, 0)
/* Parse Google streaming chunk: candidates[0].content.parts[0].text */
static gchar *google_parse_stream_chunk(const gchar *data, gsize len)
{
    (void)len;
    json_error_t jerr;
    json_t *root = json_loads(data, 0, &jerr);
    if (!root) return NULL;

    gchar *result = NULL;
    json_t *candidates = json_object_get(root, "candidates");
    if (candidates && json_is_array(candidates) && json_array_size(candidates) > 0) {
        json_t *first = json_array_get(candidates, 0);
        json_t *content = json_object_get(first, "content");
        if (content) {
            json_t *parts = json_object_get(content, "parts");
            if (parts && json_is_array(parts)) {
                /* Concatenate all text parts; skip "thought" parts. */
                GString *buf = g_string_new("");
                gsize i;
                json_t *part;
                json_array_foreach(parts, i, part) {
                    json_t *thought = json_object_get(part, "thought");
                    if (thought && json_is_true(thought)) {
                        continue;
                    }
                    json_t *text = json_object_get(part, "text");
                    if (text && json_is_string(text)) {
                        g_string_append(buf, json_string_value(text));
                    }
                }
                if (buf->len > 0) {
                    result = g_string_free(buf, FALSE);
                } else {
                    g_string_free(buf, TRUE);
                }
            }
        }
    }
    json_decref(root);
    return result;
}

static void google_send_message_stream(LumilaProvider *provider, const gchar *system_prompt,
                                        GArray *messages,
                                        LumilaChunkCallback chunk_cb,
                                        LumilaResponseCallback final_cb,
                                        gpointer user_data)
{
    const gchar *api_key = lumila_config_get_api_key(LUMILA_PROVIDER_GOOGLE);
    if (!api_key || !*api_key) {
        if (final_cb) {
            final_cb("Error: API key not configured", user_data);
        }
        return;
    }

    const gchar *model_name = lumila_provider_resolve_model(provider, GOOGLE_DEFAULT_MODEL);

    const gchar *base = lumila_config_get_endpoint(LUMILA_PROVIDER_GOOGLE);
    if (!base) base = GOOGLE_API_BASE;
    /* streamGenerateContent endpoint with alt=sse for SSE format */
    gchar *url = g_strdup_printf("%s%s:streamGenerateContent?alt=sse", base, model_name);

    json_t *root = json_object();

    /* System prompt goes in systemInstruction, conversation in contents */
    if (system_prompt && *system_prompt) {
        json_t *sys_instruction = json_object();
        json_t *sys_parts = json_array();
        json_t *sys_part = json_object();
        json_object_set_new(sys_part, "text", json_string(system_prompt));
        json_array_append_new(sys_parts, sys_part);
        json_object_set_new(sys_instruction, "parts", sys_parts);
        json_object_set_new(root, "systemInstruction", sys_instruction);
    }

    json_t *contents = lumila_provider_base_build_messages_google(messages);
    json_object_set_new(root, "contents", contents);

    json_t *gen_config = json_object();
    json_object_set_new(gen_config, "temperature", json_real(lumila_config_get_temperature()));
    json_object_set_new(gen_config, "maxOutputTokens", json_integer(lumila_config_get_max_tokens()));
    json_object_set_new(gen_config, "topP", json_real(lumila_config_get_top_p()));
    if (provider->model_flags & LUMILA_MODEL_FLAG_THINKING_LEVEL) {
        const gchar *level = lumila_config_get_thinking_level();
        if (level && *level) {
            json_t *thinking = json_object();
            json_object_set_new(thinking, "thinkingLevel", json_string(level));
            json_object_set_new(gen_config, "thinkingConfig", thinking);
        }
    }
    json_object_set_new(root, "generationConfig", gen_config);

    gchar *json_body = json_dumps(root, 0);
    json_decref(root);

    SoupMessage *msg = soup_message_new("POST", url);
    soup_message_headers_append(soup_message_get_request_headers(msg), "Content-Type", "application/json");
    soup_message_headers_append(soup_message_get_request_headers(msg), "x-goog-api-key", api_key);

    GBytes *body_bytes = g_bytes_new(json_body, strlen(json_body));
    soup_message_set_request_body_from_bytes(msg, "application/json", body_bytes);
    g_bytes_unref(body_bytes);
    g_free(json_body);
    g_free(url);

    LumilaStreamState *state = lumila_stream_state_new(provider->cancellable,
                                                        chunk_cb, final_cb, user_data);
    state->parse_chunk = google_parse_stream_chunk;
    lumila_provider_base_stream_start(provider->session, msg, state);
}
#endif
