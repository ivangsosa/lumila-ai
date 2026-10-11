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
            if (response_text) {
                json_t *finish = json_object_get(first, "finish_reason");
                const gchar *fr = finish && json_is_string(finish) ? json_string_value(finish) : NULL;
                if (g_strcmp0(fr, "length") == 0) {
                    gchar *tmp = g_strconcat(response_text,
                        "\n\n*[Response truncated: max tokens reached]*", NULL);
                    g_free(response_text);
                    response_text = tmp;
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
        if (content && json_is_array(content)) {
            /* Newer Claude models may return thinking blocks before text;
             * concatenate every block of type "text" instead of taking [0]. */
            GString *buf = g_string_new("");
            gsize i;
            json_t *block;
            json_array_foreach(content, i, block) {
                json_t *type = json_object_get(block, "type");
                if (type && json_is_string(type) &&
                    g_strcmp0(json_string_value(type), "text") == 0) {
                    json_t *text = json_object_get(block, "text");
                    if (text && json_is_string(text)) {
                        g_string_append(buf, json_string_value(text));
                    }
                }
            }
            if (buf->len > 0) {
                response_text = g_string_free(buf, FALSE);
            } else {
                g_string_free(buf, TRUE);
                json_t *stop = json_object_get(root, "stop_reason");
                const gchar *sr = stop && json_is_string(stop) ? json_string_value(stop) : NULL;
                if (g_strcmp0(sr, "refusal") == 0) {
                    response_text = g_strdup("[Response refused by the model]");
                }
            }
        }
        if (response_text) {
            json_t *stop = json_object_get(root, "stop_reason");
            const gchar *sr = stop && json_is_string(stop) ? json_string_value(stop) : NULL;
            if (g_strcmp0(sr, "max_tokens") == 0) {
                gchar *tmp = g_strconcat(response_text,
                    "\n\n*[Response truncated: max_tokens reached]*", NULL);
                g_free(response_text);
                response_text = tmp;
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
                if (parts && json_is_array(parts)) {
                    /* Concatenate all text parts; skip thought parts
                     * ("thought": true) emitted by thinking models. */
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
                        response_text = g_string_free(buf, FALSE);
                    } else {
                        g_string_free(buf, TRUE);
                    }
                }
            }
            if (response_text) {
                json_t *finish = json_object_get(first, "finishReason");
                const gchar *fr = finish && json_is_string(finish) ? json_string_value(finish) : NULL;
                if (g_strcmp0(fr, "MAX_TOKENS") == 0) {
                    gchar *tmp = g_strconcat(response_text,
                        "\n\n*[Response truncated: max output tokens reached]*", NULL);
                    g_free(response_text);
                    response_text = tmp;
                }
            }
        } else {
            /* No candidates: the prompt may have been blocked. */
            json_t *feedback = json_object_get(root, "promptFeedback");
            if (feedback) {
                json_t *reason = json_object_get(feedback, "blockReason");
                if (reason && json_is_string(reason)) {
                    response_text = g_strdup_printf("API Error: prompt blocked (%s)",
                                                    json_string_value(reason));
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

static void lumila_stream_state_finish(LumilaStreamState *state,
                                       GDataInputStream *data_stream)
{
    if (data_stream) {
        g_object_unref(data_stream);
    }
    if (state->stream) {
        g_input_stream_close(state->stream, NULL, NULL);
        g_object_unref(state->stream);
        state->stream = NULL;
    }
    lumila_stream_state_free(state);
}

static void lumila_stream_state_emit_final(LumilaStreamState *state)
{
    gchar *full = NULL;
    if (state->error_text) {
        full = g_strdup(state->error_text);
    } else if (state->http_status != 0 &&
               !SOUP_STATUS_IS_SUCCESSFUL(state->http_status)) {
        /* HTTP error body was accumulated raw in stream_buffer. */
        const gchar *body = state->stream_buffer ? state->stream_buffer->str : NULL;
        const gchar *reason = soup_status_get_phrase(state->http_status);
        if (body && *body) {
            gchar *err_msg = lumila_provider_base_extract_error_message(body, strlen(body));
            if (err_msg) {
                full = g_strdup_printf("HTTP Error %u: %s", state->http_status, err_msg);
                g_free(err_msg);
            } else {
                gchar *trunc = g_strndup(body, 300);
                full = g_strdup_printf("HTTP Error %u: %s", state->http_status, trunc);
                g_free(trunc);
            }
        } else {
            full = g_strdup_printf("HTTP Error %u: %s", state->http_status, reason);
        }
    } else if (state->stream_buffer) {
        full = g_strdup(state->stream_buffer->str);
    }
    if (state->final_cb) {
        state->final_cb(full, state->user_data);
    }
    g_free(full);
}

static void lumila_provider_base_on_stream_open(GObject *source,
                                                 GAsyncResult *result,
                                                 gpointer user_data)
{
    LumilaStreamState *state = (LumilaStreamState *)user_data;

    GError *error = NULL;
    GInputStream *stream = soup_session_send_finish(SOUP_SESSION(source), result, &error);

    if (error) {
        if (state->final_cb) {
            if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
                /* Cancelled before any data: emit NULL so the caller can
                 * distinguish "no response" from an API error. */
                state->final_cb(NULL, state->user_data);
            } else {
                gchar *err = g_strdup_printf("Error: %s", error->message);
                state->final_cb(err, state->user_data);
                g_free(err);
            }
        }
        g_error_free(error);
        lumila_stream_state_free(state);
        return;
    }

    if (state->msg) {
        state->http_status = soup_message_get_status(state->msg);
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
        if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
            /* Cancelled mid-stream: emit whatever partial text we have. */
            lumila_stream_state_emit_final(state);
        } else if (state->final_cb) {
            gchar *err = g_strdup_printf("Error: %s", error->message);
            state->final_cb(err, state->user_data);
            g_free(err);
        }
        g_error_free(error);
        lumila_stream_state_finish(state, data_stream);
        return;
    }

    if (!line) {
        lumila_stream_state_emit_final(state);
        lumila_stream_state_finish(state, data_stream);
        return;
    }

    if (state->http_status != 0 && !SOUP_STATUS_IS_SUCCESSFUL(state->http_status)) {
        /* Error responses are not SSE: accumulate the raw body so
         * emit_final can build a meaningful message. */
        if (!state->stream_buffer) {
            state->stream_buffer = g_string_new("");
        }
        g_string_append(state->stream_buffer, line);
        g_string_append_c(state->stream_buffer, '\n');
        g_free(line);
        g_data_input_stream_read_line_async(data_stream, G_PRIORITY_DEFAULT,
                                            state->cancellable,
                                            (GAsyncReadyCallback)lumila_provider_base_read_stream_line,
                                            state);
        return;
    }

    if (g_str_has_prefix(line, "data: ")) {
        const gchar *data = line + 6;
        if (g_strcmp0(data, "[DONE]") == 0) {
            g_free(line);
            lumila_stream_state_emit_final(state);
            lumila_stream_state_finish(state, data_stream);
            return;
        }

        gboolean extracted = FALSE;
        json_error_t jerr;
        json_t *root = json_loads(data, 0, &jerr);
        if (root) {
            /* Mid-stream API errors arrive as a normal "data:" event with
             * an error payload: Anthropic {"type":"error",...},
             * OpenAI-compatible {"error":{...}}. Record it and keep
             * draining until EOF/[DONE]. */
            if (!state->error_text) {
                gchar *err = lumila_provider_base_extract_error_message(data, strlen(data));
                if (err) {
                    state->error_text = err;
                    json_decref(root);
                    g_free(line);
                    g_data_input_stream_read_line_async(data_stream, G_PRIORITY_DEFAULT,
                                                        state->cancellable,
                                                        (GAsyncReadyCallback)lumila_provider_base_read_stream_line,
                                                        state);
                    return;
                }
            }
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
    if (state->msg) {
        g_object_unref(state->msg);
    }
    if (state->stream) {
        g_object_unref(state->stream);
        state->stream = NULL;
    }
    if (state->stream_buffer) {
        g_string_free(state->stream_buffer, TRUE);
    }
    g_free(state->error_text);
    g_free(state);
}

void lumila_provider_base_stream_start(SoupSession *session, SoupMessage *msg,
                                        LumilaStreamState *state)
{
    state->msg = g_object_ref(msg);
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
           status == 503 || status == 504 || status == 529;
}

gchar *lumila_provider_base_extract_error_message(const gchar *data, gsize size)
{
    json_error_t jerr;
    json_t *root = json_loadb(data, size, JSON_DISABLE_EOF_CHECK, &jerr);
    if (!root) {
        return NULL;
    }

    gchar *result = NULL;
    json_t *type = json_object_get(root, "type");
    json_t *error_obj = json_object_get(root, "error");

    if (type && json_is_string(type) &&
        g_strcmp0(json_string_value(type), "error") == 0) {
        /* Anthropic: {"type":"error","error":{"type":"...","message":"..."}} */
        json_t *msg = error_obj ? json_object_get(error_obj, "message") : NULL;
        if (msg && json_is_string(msg)) {
            result = g_strdup_printf("API Error: %s", json_string_value(msg));
        }
    } else if (error_obj) {
        json_t *msg = json_object_get(error_obj, "message");
        if (msg && json_is_string(msg)) {
            result = g_strdup_printf("API Error: %s", json_string_value(msg));
        } else if (json_is_string(error_obj)) {
            result = g_strdup_printf("API Error: %s", json_string_value(error_obj));
        }
    }

    json_decref(root);
    return result;
}

gint64 lumila_provider_base_parse_usage_total_tokens(const gchar *data, gsize size)
{
    json_error_t jerr;
    json_t *root = json_loadb(data, size, JSON_DISABLE_EOF_CHECK, &jerr);
    if (!root) {
        return -1;
    }

    gint64 total = -1;
    json_t *usage = json_object_get(root, "usage");
    if (usage) {
        json_t *t = json_object_get(usage, "total_tokens");
        if (t && json_is_integer(t)) {
            total = json_integer_value(t);
        } else {
            /* Anthropic reports input/output separately. */
            json_t *in = json_object_get(usage, "input_tokens");
            json_t *out = json_object_get(usage, "output_tokens");
            if (in && json_is_integer(in) && out && json_is_integer(out)) {
                total = json_integer_value(in) + json_integer_value(out);
            }
        }
    }
    if (total < 0) {
        json_t *meta = json_object_get(root, "usageMetadata");
        if (meta) {
            json_t *t = json_object_get(meta, "totalTokenCount");
            if (t && json_is_integer(t)) {
                total = json_integer_value(t);
            }
        }
    }

    json_decref(root);
    return total;
}

void lumila_provider_base_sleep_ms(guint ms)
{
    g_usleep((guint64)ms * 1000);
}

gchar *lumila_provider_base_finish_send_and_read(
    SoupSession *session, GAsyncResult *result,
    gchar *(*parse_func)(const gchar *data, gsize size),
    SoupMessage *pending_msg, gint64 *usage_out, gboolean *cancelled_out)
{
    if (cancelled_out) {
        *cancelled_out = FALSE;
    }
    if (usage_out) {
        *usage_out = -1;
    }

    GError *error = NULL;
    GBytes *bytes = soup_session_send_and_read_finish(session, result, &error);

    if (error) {
        if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
            if (cancelled_out) {
                *cancelled_out = TRUE;
            }
            g_error_free(error);
            return NULL;
        }
        gchar *txt = g_strdup_printf("Error: %s", error->message);
        g_error_free(error);
        return txt;
    }

    gchar *response_text = NULL;
    if (bytes) {
        gsize size;
        const gchar *data = g_bytes_get_data(bytes, &size);
        if (usage_out) {
            *usage_out = lumila_provider_base_parse_usage_total_tokens(data, size);
        }
        guint status = pending_msg ? soup_message_get_status(pending_msg) : 0;
        if (status != 0 && status != 200) {
            gchar *emsg = lumila_provider_base_extract_error_message(data, size);
            const gchar *detail = emsg ? emsg :
                (pending_msg ? soup_message_get_reason_phrase(pending_msg) : "");
            response_text = g_strdup_printf("%sHTTP Error %u: %s",
                lumila_provider_base_is_retriable_status(status) ? "Transient " : "",
                status, detail);
            g_free(emsg);
        } else if (parse_func) {
            response_text = parse_func(data, size);
        }
        g_bytes_unref(bytes);
    }
    return response_text;
}

#endif
