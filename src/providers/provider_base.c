#include "provider_base.h"
#include <jansson.h>
#include <string.h>

static gchar *parse_api_error(json_t *root)
{
    json_t *error_obj = json_object_get(root, "error");
    if (error_obj) {
        json_t *msg = json_object_get(error_obj, "message");
        if (msg && json_is_string(msg)) {
            return g_strdup_printf("API Error: %s", json_string_value(msg));
        }
    }
    return NULL;
}

gchar *lumila_provider_base_parse_openai(const gchar *data, gsize size)
{
    json_error_t json_error;
    json_t *root = json_loadb(data, size, 0, &json_error);
    gchar *response_text = NULL;

    if (!root) {
        return g_strdup_printf("JSON Parse Error: %s", json_error.text);
    }

    response_text = parse_api_error(root);
    if (!response_text) {
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
    return response_text;
}

gchar *lumila_provider_base_parse_anthropic(const gchar *data, gsize size)
{
    json_error_t json_error;
    json_t *root = json_loadb(data, size, 0, &json_error);
    gchar *response_text = NULL;

    if (!root) {
        return g_strdup_printf("JSON Parse Error: %s", json_error.text);
    }

    response_text = parse_api_error(root);
    if (!response_text) {
        json_t *content = json_object_get(root, "content");
        if (content && json_is_array(content) && json_array_size(content) > 0) {
            json_t *first = json_array_get(content, 0);
            json_t *text = json_object_get(first, "text");
            if (text && json_is_string(text)) {
                response_text = g_strdup(json_string_value(text));
            }
        }
    }

    json_decref(root);
    return response_text;
}

gchar *lumila_provider_base_parse_google(const gchar *data, gsize size)
{
    json_error_t json_error;
    json_t *root = json_loadb(data, size, 0, &json_error);
    gchar *response_text = NULL;

    if (!root) {
        return g_strdup_printf("JSON Parse Error: %s", json_error.text);
    }

    response_text = parse_api_error(root);
    if (!response_text) {
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
                        response_text = g_strdup(json_string_value(text));
                    }
                }
            }
        }
    }

    json_decref(root);
    return response_text;
}

gchar *lumila_provider_base_parse_ollama(const gchar *data, gsize size)
{
    /* Ollama returns NDJSON; parse the last line */
    const gchar *last_line = data;
    for (gsize i = 0; i < size; i++) {
        if (data[i] == '\n' && i + 1 < size) {
            last_line = &data[i + 1];
        }
    }

    json_error_t json_error;
    json_t *root = json_loads(last_line, 0, &json_error);
    gchar *response_text = NULL;

    if (!root) {
        return g_strdup_printf("JSON Parse Error: %s", json_error.text);
    }

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
    return response_text;
}

#if SOUP_CHECK_VERSION(3, 0, 0)

static void lumila_provider_base_read_stream_line(GDataInputStream *data_stream,
                                                   GAsyncResult *result,
                                                   gpointer user_data);

static void lumila_provider_base_on_stream_open(GObject *source,
                                                 GAsyncResult *result,
                                                 gpointer user_data)
{
    (void)source;
    LumilaStreamState *state = (LumilaStreamState *)user_data;

    GError *error = NULL;
    GInputStream *stream = soup_session_send_finish(SOUP_SESSION(source), result, &error);

    if (error) {
        if (state->final_cb) {
            gchar *err = g_strdup_printf("Error: %s", error->message);
            state->final_cb(err, state->user_data);
            g_free(err);
        }
        g_error_free(error);
        lumila_stream_state_free(state);
        return;
    }

    state->stream = stream;
    GDataInputStream *data_stream = g_data_input_stream_new(stream);
    g_data_input_stream_set_newline_type(data_stream, G_DATA_STREAM_NEWLINE_TYPE_LF);

    g_data_input_stream_read_line_async(data_stream, G_PRIORITY_DEFAULT,
                                        state->cancellable,
                                        (GAsyncReadyCallback)lumila_provider_base_read_stream_line,
                                        state);
}

static void lumila_provider_base_read_stream_line(GDataInputStream *data_stream,
                                                   GAsyncResult *result,
                                                   gpointer user_data)
{
    LumilaStreamState *state = (LumilaStreamState *)user_data;

    GError *error = NULL;
    gsize len = 0;
    gchar *line = g_data_input_stream_read_line_finish(data_stream, result, &len, &error);

    if (error) {
        g_object_unref(data_stream);
        g_input_stream_close(state->stream, NULL, NULL);
        g_object_unref(state->stream);
        state->stream = NULL;
        if (state->final_cb) {
            gchar *err = g_strdup_printf("Error: %s", error->message);
            state->final_cb(err, state->user_data);
            g_free(err);
        }
        g_error_free(error);
        lumila_stream_state_free(state);
        return;
    }

    if (!line) {
        g_object_unref(data_stream);
        g_input_stream_close(state->stream, NULL, NULL);
        g_object_unref(state->stream);
        state->stream = NULL;

        gchar *full = state->stream_buffer ? g_strdup(state->stream_buffer->str) : NULL;
        if (state->final_cb) {
            state->final_cb(full, state->user_data);
        }
        g_free(full);
        lumila_stream_state_free(state);
        return;
    }

    if (g_str_has_prefix(line, "data: ")) {
        const gchar *data = line + 6;
        if (g_strcmp0(data, "[DONE]") == 0) {
            g_free(line);
            g_object_unref(data_stream);
            g_input_stream_close(state->stream, NULL, NULL);
            g_object_unref(state->stream);
            state->stream = NULL;

            gchar *full = state->stream_buffer ? g_strdup(state->stream_buffer->str) : NULL;
            if (state->final_cb) {
                state->final_cb(full, state->user_data);
            }
            g_free(full);
            lumila_stream_state_free(state);
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
                            if (!state->stream_buffer)
                                state->stream_buffer = g_string_new("");
                            g_string_append(state->stream_buffer, text);
                            if (state->chunk_cb) {
                                state->chunk_cb(text, FALSE, state->user_data);
                            }
                        }
                    }
                }
            }
            json_decref(root);
        } else if (jerr.text[0]) {
            /* Partial JSON chunk - log but continue (next line may complete it) */
            g_debug("Lumila: SSE JSON parse: %s", jerr.text);
        }
    } else if (state->parse_chunk) {
        /* Provider-specific chunk parser (e.g. Anthropic, Google, Ollama) */
        gchar *text = state->parse_chunk(line, len);
        if (text && *text) {
            if (!state->stream_buffer)
                state->stream_buffer = g_string_new("");
            g_string_append(state->stream_buffer, text);
            if (state->chunk_cb) {
                state->chunk_cb(text, FALSE, state->user_data);
            }
        }
        g_free(text);
    }

    g_free(line);
    g_data_input_stream_read_line_async(data_stream, G_PRIORITY_DEFAULT,
                                        state->cancellable,
                                        (GAsyncReadyCallback)lumila_provider_base_read_stream_line,
                                        state);
}

LumilaStreamState *lumila_stream_state_new(GCancellable *cancellable,
                                            LumilaChunkCallback chunk_cb,
                                            LumilaResponseCallback final_cb,
                                            gpointer user_data)
{
    LumilaStreamState *state = g_new0(LumilaStreamState, 1);
    state->cancellable = cancellable ? g_object_ref(cancellable) : NULL;
    state->chunk_cb = chunk_cb;
    state->final_cb = final_cb;
    state->user_data = user_data;
    state->stream_buffer = g_string_new("");
    state->parse_chunk = NULL;
    return state;
}

void lumila_stream_state_free(LumilaStreamState *state)
{
    if (!state) return;
    if (state->cancellable) {
        g_object_unref(state->cancellable);
    }
    if (state->stream_buffer) {
        g_string_free(state->stream_buffer, TRUE);
    }
    g_free(state);
}

void lumila_provider_base_stream_start(SoupSession *session, SoupMessage *msg,
                                        LumilaStreamState *state)
{
    soup_session_send_async(session, msg, G_PRIORITY_DEFAULT,
                            state->cancellable,
                            lumila_provider_base_on_stream_open, state);
    g_object_unref(msg);
}

/* Default OpenAI-compatible SSE chunk parser (choices[0].delta.content) */
gchar *lumila_provider_base_parse_stream_openai(const gchar *data, gsize len)
{
    (void)len;
    json_error_t jerr;
    json_t *root = json_loads(data, 0, &jerr);
    if (!root) return NULL;

    gchar *result = NULL;
    json_t *choices = json_object_get(root, "choices");
    if (choices && json_is_array(choices) && json_array_size(choices) > 0) {
        json_t *first = json_array_get(choices, 0);
        json_t *delta = json_object_get(first, "delta");
        if (delta) {
            json_t *content = json_object_get(delta, "content");
            if (content && json_is_string(content)) {
                result = g_strdup(json_string_value(content));
            }
        }
    }
    json_decref(root);
    return result;
}

gboolean lumila_provider_base_is_retriable_status(guint status)
{
    return status == 429 || status == 500 || status == 502 ||
           status == 503 || status == 504;
}

void lumila_provider_base_sleep_ms(guint ms)
{
    g_usleep((guint64)ms * 1000);
}

#endif
