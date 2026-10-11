#ifndef LUMILA_MODEL_REGISTRY_H
#define LUMILA_MODEL_REGISTRY_H

#include "provider.h"
#include <jansson.h>

#define LUMILA_NUM_PROVIDERS 22

/* Per-model capability flags. Providers use them to decide which
 * request parameters are valid for the selected model. */
enum {
    LUMILA_MODEL_FLAG_NONE                  = 0,
    /* Model rejects temperature/top_p/top_k/penalties (reasoning). */
    LUMILA_MODEL_FLAG_NO_SAMPLING           = 1 << 0,
    /* Use "max_completion_tokens" instead of "max_tokens". */
    LUMILA_MODEL_FLAG_MAX_COMPLETION_TOKENS = 1 << 1,
    /* OpenAI-style "reasoning_effort" parameter. */
    LUMILA_MODEL_FLAG_REASONING_EFFORT      = 1 << 2,
    /* Anthropic "output_config": {"effort": ...}. */
    LUMILA_MODEL_FLAG_ANTHROPIC_EFFORT      = 1 << 3,
    /* Gemini "thinkingConfig": {"thinkingLevel": ...}. */
    LUMILA_MODEL_FLAG_THINKING_LEVEL        = 1 << 4
};

typedef struct {
    gint          provider_id;   /* stable slot index (combo/history id) */
    const gchar  *display_name;
    LumilaProviderType type;
    const gchar  *model_name;    /* API model identifier */
    guint         flags;
} LumilaModelEntry;

/* Total number of selectable models (builtin + config-defined). */
gint lumila_model_count(void);

/* Returns the entry for a slot id; falls back to the default model when
 * the id is out of range. Never NULL. */
const LumilaModelEntry *lumila_model_registry_lookup(gint provider_id);
const gchar *lumila_model_registry_get_display_name(gint provider_id);
const gchar *lumila_model_registry_get_model_name(gint provider_id);
LumilaProviderType lumila_model_registry_get_type(gint provider_id);

/* Find a model slot by its API model name (e.g. "gpt-6.1-sol").
 * Returns the provider_id or -1 when not found. */
gint lumila_model_registry_find_by_model_name(const gchar *model_name);

/* Register a config-defined model. Returns its provider_id slot.
 * Strings are copied; the caller keeps ownership of the arguments. */
gint lumila_model_registry_add(const gchar *display_name,
                               LumilaProviderType type,
                               const gchar *model_name,
                               guint flags);

/* Parse a JSON array of flag names (e.g. ["no_sampling",
 * "reasoning_effort"]) into a LUMILA_MODEL_FLAG_* bitmask. */
guint lumila_model_flags_from_json(json_t *flags_array);

#endif
