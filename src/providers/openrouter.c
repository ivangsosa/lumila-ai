#include "openrouter.h"
#include "../config.h"
#include <jansson.h>
#include <string.h>
#include <libsoup/soup.h>

#define OPENROUTER_API_BASE "https://openrouter.ai/api/v1/chat/completions"

static void openrouter_send_message(LumilaProvider *provider, const gchar *message,
                                     LumilaResponseCallback callback, gpointer user_data);
static void openrouter_cancel(LumilaProvider *provider);

typedef struct {
    LumilaProvider base;
    LumilaResponseCallback callback;
    gpointer user_data;
} OpenRouterProvider;

#if SOUP_CHECK_VERSION(3, 0, 0)
static void on_message_sent(GObject *source, GAsyncResult *result, gpointer user_data);
#else
static void on_message_sent(SoupSession *session, SoupMessage *msg, gpointer user_data);
#endif

LumilaProvider *openrouter_provider_new(void)
{
    OpenRouterProvider *provider = g_new0(OpenRouterProvider, 1);

    provider->base.type = LUMILA_PROVIDER_OPENROUTER;
#if SOUP_CHECK_VERSION(3, 0, 0)
    provider->base.session = soup_session_new_with_options(
        "timeout", 60,
        NULL);
#else
    provider->base.session = soup_session_new_with_options(
        SOUP_SESSION_TIMEOUT, 60,
        NULL);
#endif
    provider->base.cancellable = g_cancellable_new();
    provider->base.send_message = openrouter_send_message;
    provider->base.cancel = openrouter_cancel;
    provider->callback = NULL;
    provider->user_data = NULL;

    return (LumilaProvider *)provider;
}

static void openrouter_cancel(LumilaProvider *provider)
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

    OpenRouterProvider *provider = (OpenRouterProvider *)user_data;
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

        json_error_t json_error;
        json_t *root = json_loadb(data, size, 0, &json_error);

        if (root) {
            // Check for API error
            json_t *error_obj = json_object_get(root, "error");
            if (error_obj) {
                json_t *msg = json_object_get(error_obj, "message");
                if (msg && json_is_string(msg)) {
                    response_text = g_strdup_printf("API Error: %s", json_string_value(msg));
                }
            } else {
                // Parse successful response (OpenAI-compatible format)
                json_t *choices = json_object_get(root, "choices");
                if (choices && json_is_array(choices) && json_array_size(choices) > 0) {
                    json_t *first = json_array_get(choices, 0);
                    json_t *message_obj = json_object_get(first, "message");
                    if (message_obj) {
                        json_t *content = json_object_get(message_obj, "content");
                        if (content && json_is_string(content)) {
                            response_text = g_strdup(json_string_value(content));
                        }
                    }
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

    OpenRouterProvider *provider = (OpenRouterProvider *)user_data;
    LumilaResponseCallback callback = provider->callback;
    gpointer cb_data = provider->user_data;

    const gchar *response_text = NULL;

    if (SOUP_MESSAGE_STATUS_CODE(msg) == 200) {
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
    }

    if (callback) {
        callback(response_text, cb_data);
    }

    provider->callback = NULL;
    provider->user_data = NULL;
}
#endif

static void openrouter_send_message(LumilaProvider *provider, const gchar *message,
                                     LumilaResponseCallback callback, gpointer user_data)
{
    OpenRouterProvider *openrouter = (OpenRouterProvider *)provider;

    openrouter->callback = callback;
    openrouter->user_data = user_data;

    const gchar *api_key = lumila_config_get_api_key(LUMILA_PROVIDER_OPENROUTER);
    if (!api_key || !*api_key) {
        if (callback) {
            callback("Error: API key not configured", user_data);
        }
        return;
    }

    // Build JSON request (OpenAI-compatible format)
    json_t *root = json_object();
    json_object_set_new(root, "model", json_string("anthropic/claude-3.5-sonnet"));
    json_object_set_new(root, "max_tokens", json_integer(lumila_config_get_max_tokens()));
    json_object_set_new(root, "temperature", json_real(lumila_config_get_temperature()));
    json_object_set_new(root, "top_p", json_real(lumila_config_get_top_p()));
    json_object_set_new(root, "frequency_penalty", json_real(lumila_config_get_repeat_penalty() - 1.0));

    json_t *messages = json_array();
    json_t *msg_obj = json_object();
    json_object_set_new(msg_obj, "role", json_string("user"));
    json_object_set_new(msg_obj, "content", json_string(message));
    json_array_append_new(messages, msg_obj);
    json_object_set_new(root, "messages", messages);

    gchar *json_body = json_dumps(root, 0);
    json_decref(root);

#if SOUP_CHECK_VERSION(3, 0, 0)
    SoupMessage *msg = soup_message_new("POST", OPENROUTER_API_BASE);

    soup_message_headers_append(soup_message_get_request_headers(msg), "Content-Type", "application/json");

    gchar *auth_header = g_strdup_printf("Bearer %s", api_key);
    soup_message_headers_append(soup_message_get_request_headers(msg), "Authorization", auth_header);
    g_free(auth_header);

    /* OpenRouter requires these headers for app identification */
    soup_message_headers_append(soup_message_get_request_headers(msg), "HTTP-Referer", "https://github.com/ivangsosa/lumila-ai");
    soup_message_headers_append(soup_message_get_request_headers(msg), "X-Title", "Lumila AI");

    soup_message_set_request_body_from_bytes(msg, "application/json", g_bytes_new(json_body, strlen(json_body)));
    g_free(json_body);

    // Send async
    soup_session_send_and_read_async(provider->session, msg, G_PRIORITY_DEFAULT,
                                      provider->cancellable, on_message_sent, provider);
    g_object_unref(msg);
#else
    SoupMessage *msg = soup_message_new("POST", OPENROUTER_API_BASE);

    soup_message_headers_append(msg->request_headers, "Content-Type", "application/json");

    gchar *auth_header = g_strdup_printf("Bearer %s", api_key);
    soup_message_headers_append(msg->request_headers, "Authorization", auth_header);
    g_free(auth_header);

    /* OpenRouter requires these headers for app identification */
    soup_message_headers_append(msg->request_headers, "HTTP-Referer", "https://github.com/ivangsosa/lumila-ai");
    soup_message_headers_append(msg->request_headers, "X-Title", "Lumila AI");

    soup_message_body_append(msg->request_body, SOUP_MEMORY_COPY, json_body, strlen(json_body));
    g_free(json_body);

    // Send async
    soup_session_queue_message(provider->session, msg, on_message_sent, provider);
#endif
}
