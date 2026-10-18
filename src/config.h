#ifndef LUMILA_CONFIG_H
#define LUMILA_CONFIG_H

#include <glib.h>
#include <jansson.h>
#include "providers/provider.h"

void lumila_config_init(void);
void lumila_config_cleanup(void);

/* Persist config.json immediately (atomic write). Called automatically
 * by every lumila_config_set_* and once more on cleanup. */
gboolean lumila_config_save(void);

const gchar *lumila_config_get_api_key(LumilaProviderType provider);
void lumila_config_set_api_key(LumilaProviderType provider, const gchar *key);

gdouble lumila_config_get_temperature(void);
gint lumila_config_get_max_tokens(void);
gdouble lumila_config_get_top_p(void);
gdouble lumila_config_get_repeat_penalty(void);

/* Legacy numeric default (index into the model registry). Still honored
 * when "default_model" is absent or doesn't match any entry. */
gint lumila_config_get_default_provider(void);

/* Persisted default model by API name (e.g. "gpt-6.1-sol"). NULL if unset. */
const gchar *lumila_config_get_default_model(void);
void lumila_config_set_default_model(const gchar *model_name);

/* Returns the per-provider custom model override, or NULL when unset/empty. */
const gchar *lumila_config_get_custom_model(LumilaProviderType provider);
/* Returns the per-provider endpoint override, or NULL when unset/empty. */
const gchar *lumila_config_get_endpoint(LumilaProviderType provider);
gint lumila_config_get_timeout(LumilaProviderType provider);

gboolean lumila_config_get_ask_mode(void);
void lumila_config_set_ask_mode(gboolean enabled);

gboolean lumila_config_get_plan_mode(void);
void lumila_config_set_plan_mode(gboolean enabled);

/* Character budget for the auto-attached open-files context. */
gint lumila_config_get_context_max_chars(void);

/* Ask before applying ```file: edits. */
gboolean lumila_config_get_confirm_edits(void);
void lumila_config_set_confirm_edits(gboolean enabled);

/* Automatic retry with backoff on transient HTTP errors. */
gboolean lumila_config_get_auto_retry(void);

/* Max messages kept in the request context (0 = unlimited). */
gint lumila_config_get_history_max_messages(void);

/* Ghost-text AI completions while typing (opt-in, fires requests on a
 * typing debounce). */
gboolean lumila_config_get_autocomplete(void);
void lumila_config_set_autocomplete(gboolean enabled);

/* API model name for the completion request ("" = current chat model). */
const gchar *lumila_config_get_autocomplete_model(void);

/* Decorate affected files inline before the Apply/Discard dialog. */
gboolean lumila_config_get_diff_preview(void);
void lumila_config_set_diff_preview(gboolean enabled);

/* "low"|"medium"|"high"|"xhigh"|"max"|"none" (model dependent). */
const gchar *lumila_config_get_reasoning_effort(void);
/* Gemini thinking level: "low"|"medium"|"high". */
const gchar *lumila_config_get_thinking_level(void);

/* Borrowed reference to the "models" array in config.json, or NULL.
 * Each element: {"name": str, "provider": str, "model": str, "flags": [str]}. */
json_t *lumila_config_get_models(void);

#endif
