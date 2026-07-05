#include "ollama.h"
#include "provider_base.h"
#include "../config.h"
#include <jansson.h>
#include <string.h>
#include <libsoup/soup.h>

#define OLLAMA_API_BASE "http://localhost:11434/api/generate"

static void ollama_send_message(LumilaProvider *provider, const gchar *message,
                                 LumilaResponseCallback callback, gpointer user_data);
static void ollama_send_message_stream(LumilaProvider *provider, const gchar *message,
                                        LumilaChunkCallback chunk_cb,
                                        LumilaResponseCallback final_cb,
                                        gpointer user_data);
static void ollama_cancel(LumilaProvider *provider);

typedef struct {
    LumilaProvider base;
    LumilaResponseCallback callback;
    gpointer user_data;
} OllamaProvider;

#if SOUP_CHECK_VERSION(3, 0, 0)
static void on_message_sent(GObject *source, GAsyncResult *result, gpointer user_data);
#else
static void on_message_sent(SoupSession *session, SoupMessage *msg, gpointer user_data);
#endif

LumilaProvider *ollama_provider_new(void)
{
    OllamaProvider *provider = g_new0(OllamaProvider, 1);

    provider->base.type = LUMILA_PROVIDER_OLLAMA;
#if SOUP_CHECK_VERSION(3, 0, 0)
    provider->base.session = soup_session_new_with_options(
        "timeout", lumila_config_get_timeout(LUMILA_PROVIDER_OLLAMA) || 120,
        NULL);
#else
    provider->base.session = soup_session_new_with_options(
        SOUP_SESSION_TIMEOUT, lumila_config_get_timeout(LUMILA_PROVIDER_OLLAMA) || 120,
        NULL);
#endif
    provider->base.cancellable = g_cancellable_new();
    provider->base.send_message = ollama_send_message;
#if SOUP_CHECK_VERSION(3, 0, 0)
    provider->base.send_message_stream = ollama_send_message_stream;
#else
    provider->base.send_message_stream = NULL;
#endif
    provider->base.cancel = ollama_cancel;
    provider->callback = NULL;
    provider->user_data = NULL;

    return (LumilaProvider *)provider;
}

static void ollama_cancel(LumilaProvider *provider)
{
    if (provider && provider->cancellable) {
        g_cancellable_cancel(provider->cancellable);
    }
}

#if SOUP_CHECK_VERSION(3, 0, 0)
static void on_message_sent(GObject *source, GAsyncResult *result, gpointer user_data)
{
    (void)source;

    OllamaProvider *provider = (OllamaProvider *)user_data;
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
            response_text = lumila_provider_base_parse_ollama(data, size);
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

    OllamaProvider *provider = (OllamaProvider *)user_data;
    LumilaResponseCallback callback = provider->callback;
    gpointer cb_data = provider->user_data;

    const gchar *response_text = NULL;

    if (SOUP_MESSAGE_STATUS_CODE(msg) == 200) {
        SoupBuffer *buffer = soup_message_body_flatten(SOUP_MESSAGE(msg)->response_body);

        // Parse last line of NDJSON
        const gchar *last_line = buffer->data;
        for (gsize i = 0; i < buffer->length; i++) {
            if (buffer->data[i] == '\n' && i + 1 < buffer->length) {
                last_line = &buffer->data[i + 1];
            }
        }

        json_error_t error;
        json_t *root = json_loads(last_line, 0, &error);

        if (root) {
            json_t *response_obj = json_object_get(root, "response");
            if (response_obj && json_is_string(response_obj)) {
                response_text = json_string_value(response_obj);
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

static void ollama_send_message(LumilaProvider *provider, const gchar *message,
                                 LumilaResponseCallback callback, gpointer user_data)
{
    OllamaProvider *ollama = (OllamaProvider *)provider;

    ollama->callback = callback;
    ollama->user_data = user_data;

    // Select model based on model_id
    const gchar *model_name;
    switch (provider->model_id) {
        case 0: model_name = "llama3.3"; break;     // Llama 3.3
        case 1: model_name = "qwen3:8b"; break;      // Qwen3 8B
        case 2: model_name = "mistral-small:24b"; break; // Mistral Small
        default: model_name = "llama3.3"; break;
    }
    const gchar *custom = lumila_config_get_custom_model(LUMILA_PROVIDER_OLLAMA);
    if (custom) model_name = custom;

    // Build JSON request for Ollama API
    json_t *root = json_object();
    json_object_set_new(root, "model", json_string(model_name));
    json_object_set_new(root, "prompt", json_string(message));
    json_object_set_new(root, "stream", json_false());

    // Add options
    json_t *options = json_object();
    json_object_set_new(options, "temperature", json_real(lumila_config_get_temperature()));
    json_object_set_new(options, "num_predict", json_integer(lumila_config_get_max_tokens()));
    json_object_set_new(options, "top_p", json_real(lumila_config_get_top_p()));
    json_object_set_new(options, "repeat_penalty", json_real(lumila_config_get_repeat_penalty()));
    json_object_set_new(root, "options", options);

    gchar *json_body = json_dumps(root, 0);
    json_decref(root);

#if SOUP_CHECK_VERSION(3, 0, 0)
    SoupMessage *msg = soup_message_new("POST", lumila_config_get_endpoint(LUMILA_PROVIDER_OLLAMA) ? lumila_config_get_endpoint(LUMILA_PROVIDER_OLLAMA) : OLLAMA_API_BASE);

    soup_message_headers_append(soup_message_get_request_headers(msg), "Content-Type", "application/json");

    GBytes *body_bytes = g_bytes_new(json_body, strlen(json_body));
    soup_message_set_request_body_from_bytes(msg, "application/json", body_bytes);
    g_bytes_unref(body_bytes);
    g_free(json_body);

    // Send async
    soup_session_send_and_read_async(provider->session, msg, G_PRIORITY_DEFAULT, 
                                      provider->cancellable, on_message_sent, provider);
    provider->pending_msg = msg;
#else
    SoupMessage *msg = soup_message_new("POST", lumila_config_get_endpoint(LUMILA_PROVIDER_OLLAMA) ? lumila_config_get_endpoint(LUMILA_PROVIDER_OLLAMA) : OLLAMA_API_BASE);

    soup_message_headers_append(msg->request_headers, "Content-Type", "application/json");

    soup_message_body_append(msg->request_body, SOUP_MEMORY_COPY, json_body, strlen(json_body));
    g_free(json_body);

    // Send async
    soup_session_queue_message(provider->session, msg, on_message_sent, provider);
#endif
}

#if SOUP_CHECK_VERSION(3, 0, 0)
/* Parse Ollama NDJSON streaming chunk: {"response":"...","done":false} */
static gchar *ollama_parse_stream_chunk(const gchar *data, gsize len)
{
    (void)len;
    json_error_t jerr;
    json_t *root = json_loads(data, 0, &jerr);
    if (!root) return NULL;

    gchar *result = NULL;
    json_t *response = json_object_get(root, "response");
    if (response && json_is_string(response)) {
        const gchar *text = json_string_value(response);
        if (text && *text) {
            result = g_strdup(text);
        }
    }
    json_decref(root);
    return result;
}

static void ollama_send_message_stream(LumilaProvider *provider, const gchar *message,
                                        LumilaChunkCallback chunk_cb,
                                        LumilaResponseCallback final_cb,
                                        gpointer user_data)
{
    const gchar *model_name;
    switch (provider->model_id) {
        case 0: model_name = "llama3.3"; break;
        case 1: model_name = "qwen3:8b"; break;
        case 2: model_name = "mistral-small:24b"; break;
        default: model_name = "llama3.3"; break;
    }
    const gchar *custom = lumila_config_get_custom_model(LUMILA_PROVIDER_OLLAMA);
    if (custom) model_name = custom;

    json_t *root = json_object();
    json_object_set_new(root, "model", json_string(model_name));
    json_object_set_new(root, "prompt", json_string(message));
    json_object_set_new(root, "stream", json_true());

    json_t *options = json_object();
    json_object_set_new(options, "temperature", json_real(lumila_config_get_temperature()));
    json_object_set_new(options, "num_predict", json_integer(lumila_config_get_max_tokens()));
    json_object_set_new(options, "top_p", json_real(lumila_config_get_top_p()));
    json_object_set_new(options, "repeat_penalty", json_real(lumila_config_get_repeat_penalty()));
    json_object_set_new(root, "options", options);

    gchar *json_body = json_dumps(root, 0);
    json_decref(root);

    SoupMessage *msg = soup_message_new("POST", lumila_config_get_endpoint(LUMILA_PROVIDER_OLLAMA) ? lumila_config_get_endpoint(LUMILA_PROVIDER_OLLAMA) : OLLAMA_API_BASE);
    soup_message_headers_append(soup_message_get_request_headers(msg), "Content-Type", "application/json");

    GBytes *body_bytes = g_bytes_new(json_body, strlen(json_body));
    soup_message_set_request_body_from_bytes(msg, "application/json", body_bytes);
    g_bytes_unref(body_bytes);
    g_free(json_body);

    LumilaStreamState *state = lumila_stream_state_new(provider->cancellable,
                                                        chunk_cb, final_cb, user_data);
    state->parse_chunk = ollama_parse_stream_chunk;
    lumila_provider_base_stream_start(provider->session, msg, state);
}
#endif
