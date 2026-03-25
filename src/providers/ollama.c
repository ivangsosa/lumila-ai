#include "ollama.h"
#include "../config.h"
#include <jansson.h>
#include <string.h>
#include <libsoup/soup.h>

#define OLLAMA_API_BASE "http://localhost:11434/api/generate"

static void ollama_send_message(LumilaProvider *provider, const gchar *message,
                                 LumilaResponseCallback callback, gpointer user_data);
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
        "timeout", 120,
        NULL);
#else
    provider->base.session = soup_session_new_with_options(
        SOUP_SESSION_TIMEOUT, 120,
        NULL);
#endif
    provider->base.cancellable = g_cancellable_new();
    provider->base.send_message = ollama_send_message;
    provider->base.cancel = ollama_cancel;
    provider->callback = NULL;
    provider->user_data = NULL;

    return (LumilaProvider *)provider;
}

static void ollama_cancel(LumilaProvider *provider)
{
    if (provider && provider->cancellable) {
        g_cancellable_cancel(provider->cancellable);
        g_object_unref(provider->cancellable);
        provider->cancellable = g_cancellable_new();
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

        // Ollama returns NDJSON (newline-delimited JSON), parse last line
        const gchar *last_line = data;
        for (gsize i = 0; i < size; i++) {
            if (data[i] == '\n' && i + 1 < size) {
                last_line = &data[i + 1];
            }
        }

        json_error_t json_error;
        json_t *root = json_loads(last_line, 0, &json_error);

        if (root) {
            json_t *error_obj = json_object_get(root, "error");
            if (error_obj && json_is_string(error_obj)) {
                response_text = g_strdup_printf("API Error: %s", json_string_value(error_obj));
            } else {
                json_t *response_obj = json_object_get(root, "response");
                if (response_obj && json_is_string(response_obj)) {
                    response_text = g_strdup(json_string_value(response_obj));
                }
            }
            json_decref(root);
        } else {
            response_text = g_strdup_printf("JSON Parse Error: %s", json_error.text);
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
        case 0: model_name = "llama3.2:3b"; break;   // Llama 3.2 3B
        case 1: model_name = "qwen2.5:7b"; break;    // Qwen 2.5 7B
        default: model_name = "llama3.2:3b"; break;
    }

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
    SoupMessage *msg = soup_message_new("POST", OLLAMA_API_BASE);

    soup_message_headers_append(soup_message_get_request_headers(msg), "Content-Type", "application/json");

    soup_message_set_request_body_from_bytes(msg, "application/json", g_bytes_new(json_body, strlen(json_body)));
    g_free(json_body);

    // Send async
    soup_session_send_and_read_async(provider->session, msg, G_PRIORITY_DEFAULT, 
                                      provider->cancellable, on_message_sent, provider);
    g_object_unref(msg);
#else
    SoupMessage *msg = soup_message_new("POST", OLLAMA_API_BASE);

    soup_message_headers_append(msg->request_headers, "Content-Type", "application/json");

    soup_message_body_append(msg->request_body, SOUP_MEMORY_COPY, json_body, strlen(json_body));
    g_free(json_body);

    // Send async
    soup_session_queue_message(provider->session, msg, on_message_sent, provider);
#endif
}
