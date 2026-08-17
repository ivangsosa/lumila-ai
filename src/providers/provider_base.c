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
    /* Ollama returns NDJSON: multiple JSON objects separated by newlines.
     * Each object has a "response" field with a chunk of text.
     * We concatenate all chunks and check for errors in any line.
     * The last line typically has "done": true. */
    GString *full_response = g_string_new("");
    gboolean had_error = FALSE;
    gchar *error_msg = NULL;

    const gchar *start = data;
    const gchar *end = data + size;
    const gchar *line_start = start;

    for (const gchar *p = start; p <= end; p++) {
        if (p == end || *p == '\n') {
            gsize line_len = p - line_start;
            if (line_len > 0) {
                gchar *line = g_strndup(line_start, line_len);
                json_error_t json_error;
                json_t *root = json_loads(line, 0, &json_error);

                if (root) {
                    json_t *error_obj = json_object_get(root, "error");
                    if (error_obj && json_is_string(error_obj)) {
                        had_error = TRUE;
                        g_free(error_msg);
                        error_msg = g_strdup_printf("API Error: %s", json_string_value(error_obj));
                    } else {
                        json_t *response_obj = json_object_get(root, "response");
                        if (response_obj && json_is_string(response_obj)) {
                            g_string_append(full_response, json_string_value(response_obj));
                        }
                    }
                    json_decref(root);
                }
                g_free(line);
            }
            line_start = p + 1;
        }
    }

    gchar *result;
    if (had_error && full_response->len == 0) {
        result = error_msg ? error_msg : g_strdup("Unknown API Error");
        g_string_free(full_response, TRUE);
    } else if (had_error) {
        /* Si hay respuesta parcial pero también error, devolver la respuesta */
        result = g_string_free(full_response, FALSE);
        g_free(error_msg);
    } else {
        result = g_string_free(full_response, FALSE);
        g_free(error_msg);
    }

    return result;
}

/* --- Multi-turn message helpers --- */

json_t *lumila_provider_base_build_messages_openai(const gchar *system_prompt,
                                                    GArray *messages)
{
    json_t *arr = json_array();
    if (system_prompt && *system_prompt) {
        json_t *sys = json_object();
        json_object_set_new(sys, "role", json_string("system"));
        json_object_set_new(sys, "content", json_string(system_prompt));
        json_array_append_new(arr, sys);
    }
    if (messages) {
        for (guint i = 0; i < messages->len; i++) {
            LumilaMessage *m = &g_array_index(messages, LumilaMessage, i);
            json_t *obj = json_object();
            json_object_set_new(obj, "role", json_string(m->role));
            json_object_set_new(obj, "content", json_string(m->content));
            json_array_append_new(arr, obj);
        }
    }
    return arr;
}

json_t *lumila_provider_base_build_messages_anthropic(GArray *messages)
{
    json_t *arr = json_array();
    if (messages) {
        for (guint i = 0; i < messages->len; i++) {
            LumilaMessage *m = &g_array_index(messages, LumilaMessage, i);
            json_t *obj = json_object();
            json_object_set_new(obj, "role", json_string(m->role));
            json_object_set_new(obj, "content", json_string(m->content));
            json_array_append_new(arr, obj);
        }
    }
    return arr;
}

json_t *lumila_provider_base_build_messages_google(GArray *messages)
{
    json_t *arr = json_array();
    if (messages) {
        for (guint i = 0; i < messages->len; i++) {
            LumilaMessage *m = &g_array_index(messages, LumilaMessage, i);
            json_t *obj = json_object();
            /* Google uses "model" instead of "assistant" */
            const char *role = g_str_equal(m->role, "assistant") ? "model" : m->role;
            json_object_set_new(obj, "role", json_string(role));
            json_t *parts = json_array();
            json_t *part = json_object();
            json_object_set_new(part, "text", json_string(m->content));
            json_array_append_new(parts, part);
            json_object_set_new(obj, "parts", parts);
            json_array_append_new(arr, obj);
        }
    }
    return arr;
}

json_t *lumila_provider_base_build_messages_ollama(const gchar *system_prompt,
                                                    GArray *messages)
{
    json_t *arr = json_array();
    if (system_prompt && *system_prompt) {
        json_t *sys = json_object();
        json_object_set_new(sys, "role", json_string("system"));
        json_object_set_new(sys, "content", json_string(system_prompt));
        json_array_append_new(arr, sys);
    }
    if (messages) {
        for (guint i = 0; i < messages->len; i++) {
            LumilaMessage *m = &g_array_index(messages, LumilaMessage, i);
            json_t *obj = json_object();
            json_object_set_new(obj, "role", json_string(m->role));
            json_object_set_new(obj, "content", json_string(m->content));
            json_array_append_new(arr, obj);
        }
    }
    return arr;
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

        gboolean extracted = FALSE;
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
                            extracted = TRUE;
                        }
                    }
                }
            }
            json_decref(root);
        } else if (jerr.text[0]) {
            /* Partial JSON chunk - log but continue (next line may complete it) */
            g_debug("Lumila: SSE JSON parse: %s", jerr.text);
        }

        /* Fallback: if OpenAI-style parsing extracted nothing, try the
         * provider-specific chunk parser (e.g. Anthropic, Google) which
         * also use the "data: " SSE prefix but with a different schema. */
        if (!extracted && state->parse_chunk) {
            gchar *text = state->parse_chunk(data, strlen(data));
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
    } else if (state->parse_chunk) {
        /* Provider-specific chunk parser (e.g. Ollama NDJSON without "data: " prefix) */
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
