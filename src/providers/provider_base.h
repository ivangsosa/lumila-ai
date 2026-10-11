#ifndef LUMILA_PROVIDER_BASE_H
#define LUMILA_PROVIDER_BASE_H

#include "provider.h"
#include <libsoup/soup.h>
#include <jansson.h>

/* Parse JSON response helpers.
 * All return a newly allocated gchar* or NULL.
 * Caller must g_free() the result.
 */

/* OpenAI-compatible format (choices[0].message.content) */
gchar *lumila_provider_base_parse_openai(const gchar *data, gsize size);

/* Anthropic format (content[0].text) */
gchar *lumila_provider_base_parse_anthropic(const gchar *data, gsize size);

/* Google/Gemini format (candidates[0].content.parts[0].text) */
gchar *lumila_provider_base_parse_google(const gchar *data, gsize size);

/* Ollama format (last line NDJSON, .response) */
gchar *lumila_provider_base_parse_ollama(const gchar *data, gsize size);

/* --- Multi-turn message helpers --- */

/* Build a JSON array of messages in OpenAI-compatible format:
 * [{role:"system",content:system_prompt}, {role:"user",...}, {role:"assistant",...}, ...]
 * Returns a newly-allocated json_t* (caller must json_decref()).
 * If system_prompt is NULL/empty, no system message is prepended. */
json_t *lumila_provider_base_build_messages_openai(const gchar *system_prompt,
                                                    GArray *messages);

/* Build a JSON array of messages in Anthropic format:
 * [{role:"user",...}, {role:"assistant",...}, ...] (no system role).
 * System prompt is handled separately by the Anthropic provider.
 * Returns a newly-allocated json_t* (caller must json_decref()). */
json_t *lumila_provider_base_build_messages_anthropic(GArray *messages);

/* Build a JSON array of contents in Google/Gemini format:
 * [{role:"user",parts:[{text:...}]}, {role:"model",parts:[{text:...}]}, ...]
 * "assistant" role is mapped to "model".
 * Returns a newly-allocated json_t* (caller must json_decref()). */
json_t *lumila_provider_base_build_messages_google(GArray *messages);

/* Build a JSON array of messages in Ollama /api/chat format:
 * [{role:"user",content:...}, {role:"assistant",content:...}, ...]
 * System prompt is prepended as a {role:"system"} message if non-empty.
 * Returns a newly-allocated json_t* (caller must json_decref()). */
json_t *lumila_provider_base_build_messages_ollama(const gchar *system_prompt,
                                                    GArray *messages);

/* --- Streaming helpers (libsoup-3.0 only) --- */

#if SOUP_CHECK_VERSION(3, 0, 0)

typedef struct {
    GCancellable *cancellable;
    LumilaChunkCallback chunk_cb;
    LumilaResponseCallback final_cb;
    gpointer user_data;
    GString *stream_buffer;
    GInputStream *stream;
    /* Kept to inspect the HTTP status of the streaming response. */
    SoupMessage *msg;
    guint http_status;
    /* API-level error detected inside an SSE "data:" payload. */
    gchar *error_text;
    /* Optional destination for total-token usage seen in stream events
     * (providers point it at provider->last_usage). May be NULL. */
    gint64 *usage_out;
    gint64 usage_in;      /* last reported prompt/input token count, -1 unset */
    gint64 usage_out_tok; /* last reported completion/output count, -1 unset */
    gint64 usage_total;   /* explicit total when the API reports one */
    /* Provider-specific SSE chunk parser.
     * Returns newly-allocated text extracted from the JSON data line,
     * or NULL if no content. Caller must g_free(). */
    gchar *(*parse_chunk)(const gchar *data, gsize len);
} LumilaStreamState;

LumilaStreamState *lumila_stream_state_new(GCancellable *cancellable,
                                            LumilaChunkCallback chunk_cb,
                                            LumilaResponseCallback final_cb,
                                            gpointer user_data);
void lumila_stream_state_free(LumilaStreamState *state);

/* Start an SSE stream. The SoupMessage must already have stream=true in its body.
 * state is owned by the helper and freed automatically when the stream ends.
 */
void lumila_provider_base_stream_start(SoupSession *session, SoupMessage *msg,
                                        LumilaStreamState *state);

/* Default OpenAI-compatible SSE chunk parser (choices[0].delta.content). */
gchar *lumila_provider_base_parse_stream_openai(const gchar *data, gsize len);

/* Common finish() for soup_session_send_and_read_async():
 *  - extracts token usage (usage_out, -1 when absent)
 *  - reports HTTP errors with the API-provided message, "Transient HTTP
 *    Error" prefix for retriable statuses
 *  - parses a 2xx body with parse_func
 * Returns a newly allocated response string, or NULL on cancel
 * (*cancelled_out set to TRUE) or when the body yielded no text. */
gchar *lumila_provider_base_finish_send_and_read(
    SoupSession *session, GAsyncResult *result,
    gchar *(*parse_func)(const gchar *data, gsize size),
    SoupMessage *pending_msg, gint64 *usage_out, gboolean *cancelled_out);

/* Retry logic: returns TRUE if the HTTP status code indicates a transient
 * error that warrants a retry (429 rate limit, 500/502/503/504 server errors). */
gboolean lumila_provider_base_is_retriable_status(guint status);

/* Extract a human-readable message from an API error JSON body, or NULL.
 * Handles OpenAI ("error":{message}), Anthropic ("type":"error") and
 * Google error shapes. Returned string must be g_free()d. */
gchar *lumila_provider_base_extract_error_message(const gchar *data, gsize size);

/* Extract total token usage from a provider response body, or -1.
 * Understands OpenAI (usage.total_tokens), Anthropic
 * (usage.input_tokens + output_tokens) and Google
 * (usageMetadata.totalTokenCount) shapes. */
gint64 lumila_provider_base_parse_usage_total_tokens(const gchar *data, gsize size);

/* Sleep for the given number of milliseconds. Used for retry backoff. */
void lumila_provider_base_sleep_ms(guint ms);

#endif

#endif
