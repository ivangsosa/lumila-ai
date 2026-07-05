#include "google.h"
#include "provider_base.h"
#include "../config.h"
#include <jansson.h>
#include <string.h>
#include <libsoup/soup.h>

#define GOOGLE_API_BASE "https://generativelanguage.googleapis.com/v1beta/models/"

static void google_send_message(LumilaProvider *provider, const gchar *message,
                                 LumilaResponseCallback callback, gpointer user_data);
static void google_send_message_stream(LumilaProvider *provider, const gchar *message,
                                        LumilaChunkCallback chunk_cb,
                                        LumilaResponseCallback final_cb,
                                        gpointer user_data);
static void google_cancel(LumilaProvider *provider);

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
        "timeout", lumila_config_get_timeout(LUMILA_PROVIDER_GOOGLE) || 60,
        NULL);
#else
    provider->base.session = soup_session_new_with_options(
        SOUP_SESSION_TIMEOUT, lumila_config_get_timeout(LUMILA_PROVIDER_GOOGLE) || 60,
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
    (void)source;

    GoogleProvider *provider = (GoogleProvider *)user_data;
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
                response_text = g_strdup_printf("HTTP Error %u: %s", status,
                    soup_message_get_reason_phrase(provider->base.pending_msg));
            }
        }
        if (!response_text) {
            response_text = lumila_provider_base_parse_google(data, size);
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

    GoogleProvider *provider = (GoogleProvider *)user_data;
    LumilaResponseCallback callback = provider->callback;
    gpointer cb_data = provider->user_data;

    const gchar *response_text = NULL;

    if (SOUP_MESSAGE_STATUS_CODE(msg) == 200) {
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

static void google_send_message(LumilaProvider *provider, const gchar *message,
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

    // Select model based on model_id
    const gchar *model_name;
    switch (provider->model_id) {
        case 0: model_name = "gemini-2.5-flash"; break;  // Gemini 2.5 Flash
        case 1: model_name = "gemini-2.5-pro"; break;   // Gemini 2.5 Pro
        case 2: model_name = "gemma-4-12b-it"; break;      // Gemma 4 12B
        default: model_name = "gemini-2.5-flash"; break;
    }
    const gchar *custom = lumila_config_get_custom_model(LUMILA_PROVIDER_GOOGLE);
    if (custom) model_name = custom;

    // Build URL (API key sent via header for security)
    const gchar *base = lumila_config_get_endpoint(LUMILA_PROVIDER_GOOGLE);
    if (!base) base = GOOGLE_API_BASE;
    gchar *url = g_strdup_printf("%s%s:generateContent", base, model_name);

    // Build JSON request for Gemini API
    json_t *root = json_object();

    json_t *contents = json_array();
    json_t *content_obj = json_object();

    json_t *parts = json_array();
    json_t *part_obj = json_object();
    json_object_set_new(part_obj, "text", json_string(message));
    json_array_append_new(parts, part_obj);

    json_object_set_new(content_obj, "parts", parts);
    json_array_append_new(contents, content_obj);
    json_object_set_new(root, "contents", contents);

    // Add generation config
    json_t *gen_config = json_object();
    json_object_set_new(gen_config, "temperature", json_real(lumila_config_get_temperature()));
    json_object_set_new(gen_config, "maxOutputTokens", json_integer(lumila_config_get_max_tokens()));
    json_object_set_new(gen_config, "topP", json_real(lumila_config_get_top_p()));
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
            if (parts && json_is_array(parts) && json_array_size(parts) > 0) {
                json_t *first_part = json_array_get(parts, 0);
                json_t *text = json_object_get(first_part, "text");
                if (text && json_is_string(text)) {
                    result = g_strdup(json_string_value(text));
                }
            }
        }
    }
    json_decref(root);
    return result;
}

static void google_send_message_stream(LumilaProvider *provider, const gchar *message,
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

    const gchar *model_name;
    switch (provider->model_id) {
        case 0: model_name = "gemini-2.5-flash"; break;
        case 1: model_name = "gemini-2.5-pro"; break;
        case 2: model_name = "gemma-4-12b-it"; break;
        default: model_name = "gemini-2.5-flash"; break;
    }
    const gchar *custom = lumila_config_get_custom_model(LUMILA_PROVIDER_GOOGLE);
    if (custom) model_name = custom;

    const gchar *base = lumila_config_get_endpoint(LUMILA_PROVIDER_GOOGLE);
    if (!base) base = GOOGLE_API_BASE;
    /* streamGenerateContent endpoint with alt=sse for SSE format */
    gchar *url = g_strdup_printf("%s%s:streamGenerateContent?alt=sse", base, model_name);

    json_t *root = json_object();
    json_t *contents = json_array();
    json_t *content_obj = json_object();
    json_t *parts = json_array();
    json_t *part_obj = json_object();
    json_object_set_new(part_obj, "text", json_string(message));
    json_array_append_new(parts, part_obj);
    json_object_set_new(content_obj, "parts", parts);
    json_array_append_new(contents, content_obj);
    json_object_set_new(root, "contents", contents);

    json_t *gen_config = json_object();
    json_object_set_new(gen_config, "temperature", json_real(lumila_config_get_temperature()));
    json_object_set_new(gen_config, "maxOutputTokens", json_integer(lumila_config_get_max_tokens()));
    json_object_set_new(gen_config, "topP", json_real(lumila_config_get_top_p()));
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
