#include "openai.h"
#include "../config.h"
#include <jansson.h>
#include <string.h>
#include <libsoup/soup.h>

#define OPENAI_API_BASE "https://api.openai.com/v1/chat/completions"

static void openai_send_message(LumilaProvider *provider, const gchar *message,
                                 LumilaResponseCallback callback, gpointer user_data);
static void openai_send_message_stream(LumilaProvider *provider, const gchar *message,
                                        LumilaChunkCallback chunk_cb,
                                        LumilaResponseCallback final_cb,
                                        gpointer user_data);
static void openai_cancel(LumilaProvider *provider);

typedef struct {
    LumilaProvider base;
    LumilaResponseCallback callback;
    LumilaChunkCallback chunk_cb;
    LumilaResponseCallback final_cb;
    gpointer user_data;
    GString *stream_buffer;
    GInputStream *stream;
} OpenAIProvider;

#if SOUP_CHECK_VERSION(3, 0, 0)
static void on_message_sent(GObject *source, GAsyncResult *result, gpointer user_data);
#else
static void on_message_sent(SoupSession *session, SoupMessage *msg, gpointer user_data);
#endif

LumilaProvider *openai_provider_new(void)
{
    OpenAIProvider *provider = g_new0(OpenAIProvider, 1);

    provider->base.type = LUMILA_PROVIDER_OPENAI;
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
    provider->base.send_message = openai_send_message;
#if SOUP_CHECK_VERSION(3, 0, 0)
    provider->base.send_message_stream = openai_send_message_stream;
#else
    provider->base.send_message_stream = NULL;
#endif
    provider->base.cancel = openai_cancel;
    provider->callback = NULL;
    provider->chunk_cb = NULL;
    provider->final_cb = NULL;
    provider->user_data = NULL;
    provider->stream_buffer = NULL;
    provider->stream = NULL;

    return (LumilaProvider *)provider;
}

static void openai_cancel(LumilaProvider *provider)
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

    OpenAIProvider *provider = (OpenAIProvider *)user_data;
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
                // Parse successful response
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

    OpenAIProvider *provider = (OpenAIProvider *)user_data;
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

static void openai_send_message(LumilaProvider *provider, const gchar *message,
                                 LumilaResponseCallback callback, gpointer user_data)
{
    OpenAIProvider *oa = (OpenAIProvider *)provider;

    oa->callback = callback;
    oa->user_data = user_data;

    const gchar *api_key = lumila_config_get_api_key(LUMILA_PROVIDER_OPENAI);
    if (!api_key || !*api_key) {
        if (callback) {
            callback("Error: API key not configured", user_data);
        }
        return;
    }

    // Build JSON request
    json_t *root = json_object();

    // Select model based on model_id
    const gchar *model_name;
    switch (provider->model_id) {
        case 0: model_name = "gpt-4.1"; break;       // GPT-4.1
        case 1: model_name = "gpt-4.1-mini"; break;  // GPT-4.1 mini
        default: model_name = "gpt-4.1"; break;
    }

    json_object_set_new(root, "model", json_string(model_name));
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
    SoupMessage *msg = soup_message_new("POST", OPENAI_API_BASE);

    soup_message_headers_append(soup_message_get_request_headers(msg), "Content-Type", "application/json");

    gchar *auth_header = g_strdup_printf("Bearer %s", api_key);
    soup_message_headers_append(soup_message_get_request_headers(msg), "Authorization", auth_header);
    g_free(auth_header);

    soup_message_set_request_body_from_bytes(msg, "application/json", g_bytes_new(json_body, strlen(json_body)));
    g_free(json_body);

    // Send async
    soup_session_send_and_read_async(provider->session, msg, G_PRIORITY_DEFAULT, 
                                      provider->cancellable, on_message_sent, provider);
    g_object_unref(msg);
#else
    SoupMessage *msg = soup_message_new("POST", OPENAI_API_BASE);

    soup_message_headers_append(msg->request_headers, "Content-Type", "application/json");

    gchar *auth_header = g_strdup_printf("Bearer %s", api_key);
    soup_message_headers_append(msg->request_headers, "Authorization", auth_header);
    g_free(auth_header);

    soup_message_body_append(msg->request_body, SOUP_MEMORY_COPY, json_body, strlen(json_body));
    g_free(json_body);

    // Send async
    soup_session_queue_message(provider->session, msg, on_message_sent, provider);
#endif
}

#if SOUP_CHECK_VERSION(3, 0, 0)

static void read_stream_line(GDataInputStream *data_stream, GAsyncResult *result, gpointer user_data);

static void on_stream_open(GObject *source, GAsyncResult *result, gpointer user_data)
{
    (void)source;
    OpenAIProvider *oa = (OpenAIProvider *)user_data;

    GError *error = NULL;
    GInputStream *stream = soup_session_send_finish(SOUP_SESSION(source), result, &error);

    if (error) {
        if (oa->final_cb) {
            gchar *err = g_strdup_printf("Error: %s", error->message);
            oa->final_cb(err, oa->user_data);
            g_free(err);
        }
        g_error_free(error);
        oa->stream = NULL;
        return;
    }

    oa->stream = stream;
    GDataInputStream *data_stream = g_data_input_stream_new(stream);
    g_data_input_stream_set_newline_type(data_stream, G_DATA_STREAM_NEWLINE_TYPE_LF);

    g_data_input_stream_read_line_async(data_stream, G_PRIORITY_DEFAULT,
                                        oa->base.cancellable,
                                        (GAsyncReadyCallback)read_stream_line, oa);
}

static void read_stream_line(GDataInputStream *data_stream, GAsyncResult *result, gpointer user_data)
{
    OpenAIProvider *oa = (OpenAIProvider *)user_data;

    GError *error = NULL;
    gsize len = 0;
    gchar *line = g_data_input_stream_read_line_finish(data_stream, result, &len, &error);

    if (error) {
        g_object_unref(data_stream);
        g_input_stream_close(oa->stream, NULL, NULL);
        g_object_unref(oa->stream);
        oa->stream = NULL;
        if (oa->final_cb) {
            gchar *err = g_strdup_printf("Error: %s", error->message);
            oa->final_cb(err, oa->user_data);
            g_free(err);
        }
        g_error_free(error);
        return;
    }

    if (!line) {
        // End of stream
        g_object_unref(data_stream);
        g_input_stream_close(oa->stream, NULL, NULL);
        g_object_unref(oa->stream);
        oa->stream = NULL;

        gchar *full = oa->stream_buffer ? g_strdup(oa->stream_buffer->str) : NULL;
        if (oa->final_cb) {
            oa->final_cb(full, oa->user_data);
        }
        if (oa->stream_buffer) {
            g_string_free(oa->stream_buffer, TRUE);
            oa->stream_buffer = NULL;
        }
        g_free(full);
        return;
    }

    if (g_str_has_prefix(line, "data: ")) {
        const gchar *data = line + 6;
        if (g_strcmp0(data, "[DONE]") == 0) {
            g_free(line);
            g_object_unref(data_stream);
            g_input_stream_close(oa->stream, NULL, NULL);
            g_object_unref(oa->stream);
            oa->stream = NULL;

            gchar *full = oa->stream_buffer ? g_strdup(oa->stream_buffer->str) : NULL;
            if (oa->final_cb) {
                oa->final_cb(full, oa->user_data);
            }
            if (oa->stream_buffer) {
                g_string_free(oa->stream_buffer, TRUE);
                oa->stream_buffer = NULL;
            }
            g_free(full);
            return;
        }

        json_error_t jerr;
        json_t *root = json_loads(data, 0, &jerr);
        if (root) {
            json_t *choices = json_object_get(root, "choices");
            if (choices && json_is_array(choices) && json_array_size(choices) > 0) {
                json_t *first = json_array_get(choices, 0);
                json_t *delta = json_object_get(first, "delta");
                if (delta) {
                    json_t *content = json_object_get(delta, "content");
                    if (content && json_is_string(content)) {
                        const gchar *text = json_string_value(content);
                        if (text && *text) {
                            if (!oa->stream_buffer)
                                oa->stream_buffer = g_string_new("");
                            g_string_append(oa->stream_buffer, text);
                            if (oa->chunk_cb) {
                                oa->chunk_cb(text, FALSE, oa->user_data);
                            }
                        }
                    }
                }
            }
            json_decref(root);
        }
    }

    g_free(line);
    g_data_input_stream_read_line_async(data_stream, G_PRIORITY_DEFAULT,
                                        oa->base.cancellable,
                                        (GAsyncReadyCallback)read_stream_line, oa);
}

static void openai_send_message_stream(LumilaProvider *provider, const gchar *message,
                                        LumilaChunkCallback chunk_cb,
                                        LumilaResponseCallback final_cb,
                                        gpointer user_data)
{
    OpenAIProvider *oa = (OpenAIProvider *)provider;

    oa->chunk_cb = chunk_cb;
    oa->final_cb = final_cb;
    oa->user_data = user_data;
    if (oa->stream_buffer) {
        g_string_free(oa->stream_buffer, TRUE);
    }
    oa->stream_buffer = g_string_new("");

    const gchar *api_key = lumila_config_get_api_key(LUMILA_PROVIDER_OPENAI);
    if (!api_key || !*api_key) {
        if (final_cb) {
            final_cb("Error: API key not configured", user_data);
        }
        return;
    }

    json_t *root = json_object();

    const gchar *model_name;
    switch (provider->model_id) {
        case 0: model_name = "gpt-4.1"; break;
        case 1: model_name = "gpt-4.1-mini"; break;
        default: model_name = "gpt-4.1"; break;
    }

    json_object_set_new(root, "model", json_string(model_name));
    json_object_set_new(root, "max_tokens", json_integer(lumila_config_get_max_tokens()));
    json_object_set_new(root, "temperature", json_real(lumila_config_get_temperature()));
    json_object_set_new(root, "top_p", json_real(lumila_config_get_top_p()));
    json_object_set_new(root, "frequency_penalty", json_real(lumila_config_get_repeat_penalty() - 1.0));
    json_object_set_new(root, "stream", json_true());

    json_t *messages = json_array();
    json_t *msg_obj = json_object();
    json_object_set_new(msg_obj, "role", json_string("user"));
    json_object_set_new(msg_obj, "content", json_string(message));
    json_array_append_new(messages, msg_obj);
    json_object_set_new(root, "messages", messages);

    gchar *json_body = json_dumps(root, 0);
    json_decref(root);

    SoupMessage *msg = soup_message_new("POST", OPENAI_API_BASE);

    soup_message_headers_append(soup_message_get_request_headers(msg), "Content-Type", "application/json");

    gchar *auth_header = g_strdup_printf("Bearer %s", api_key);
    soup_message_headers_append(soup_message_get_request_headers(msg), "Authorization", auth_header);
    g_free(auth_header);

    soup_message_set_request_body_from_bytes(msg, "application/json", g_bytes_new(json_body, strlen(json_body)));
    g_free(json_body);

    soup_session_send_async(provider->session, msg, G_PRIORITY_DEFAULT,
                             provider->cancellable, on_stream_open, provider);
    g_object_unref(msg);
}
#endif
