#ifndef LUMILA_PROVIDER_BASE_H
#define LUMILA_PROVIDER_BASE_H

#include "provider.h"
#include <libsoup/soup.h>

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

/* --- Streaming helpers (libsoup-3.0 only) --- */

#if SOUP_CHECK_VERSION(3, 0, 0)

typedef struct {
    GCancellable *cancellable;
    LumilaChunkCallback chunk_cb;
    LumilaResponseCallback final_cb;
    gpointer user_data;
    GString *stream_buffer;
    GInputStream *stream;
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

#endif

#endif
