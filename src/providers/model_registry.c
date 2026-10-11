#include "model_registry.h"

#define LUMILA_DEFAULT_MODEL_ID 9 /* OpenRouter Auto */

static const LumilaModelEntry lumila_builtin_models[LUMILA_NUM_PROVIDERS] = {
    { 0,  "Claude Sonnet 5.5",          LUMILA_PROVIDER_ANTHROPIC,  "claude-sonnet-5-5",
      LUMILA_MODEL_FLAG_NO_SAMPLING | LUMILA_MODEL_FLAG_ANTHROPIC_EFFORT },
    { 1,  "Claude Opus 5.5",            LUMILA_PROVIDER_ANTHROPIC,  "claude-opus-5-5",
      LUMILA_MODEL_FLAG_NO_SAMPLING | LUMILA_MODEL_FLAG_ANTHROPIC_EFFORT },
    { 2,  "Claude Haiku 5.5",           LUMILA_PROVIDER_ANTHROPIC,  "claude-haiku-5-5",
      LUMILA_MODEL_FLAG_NO_SAMPLING | LUMILA_MODEL_FLAG_ANTHROPIC_EFFORT },
    { 3,  "GPT-6.1 Sol",                LUMILA_PROVIDER_OPENAI,     "gpt-6.1-sol",
      LUMILA_MODEL_FLAG_NO_SAMPLING | LUMILA_MODEL_FLAG_MAX_COMPLETION_TOKENS
      | LUMILA_MODEL_FLAG_REASONING_EFFORT },
    { 4,  "GPT-6 Astra",                LUMILA_PROVIDER_OPENAI,     "gpt-6-astra",
      LUMILA_MODEL_FLAG_NO_SAMPLING | LUMILA_MODEL_FLAG_MAX_COMPLETION_TOKENS
      | LUMILA_MODEL_FLAG_REASONING_EFFORT },
    { 5,  "GPT-6 Luna",                 LUMILA_PROVIDER_OPENAI,     "gpt-6-luna",
      LUMILA_MODEL_FLAG_NO_SAMPLING | LUMILA_MODEL_FLAG_MAX_COMPLETION_TOKENS
      | LUMILA_MODEL_FLAG_REASONING_EFFORT },
    { 6,  "Gemini 3.8 Flash",           LUMILA_PROVIDER_GOOGLE,     "gemini-3.8-flash",
      LUMILA_MODEL_FLAG_THINKING_LEVEL },
    { 7,  "Gemini 3.1 Pro Preview",     LUMILA_PROVIDER_GOOGLE,     "gemini-3.1-pro-preview",
      LUMILA_MODEL_FLAG_THINKING_LEVEL },
    { 8,  "Gemini 3.6 Flash",           LUMILA_PROVIDER_GOOGLE,     "gemini-3.6-flash",
      LUMILA_MODEL_FLAG_THINKING_LEVEL },
    { 9,  "OpenRouter Auto",            LUMILA_PROVIDER_OPENROUTER, "openrouter/auto", 0 },
    { 10, "DeepSeek V4.1 Flash",        LUMILA_PROVIDER_DEEPSEEK,   "deepseek-flash", 0 },
    { 11, "DeepSeek V4 Pro",            LUMILA_PROVIDER_DEEPSEEK,   "deepseek-v4-pro", 0 },
    { 12, "Mistral Large 4",            LUMILA_PROVIDER_MISTRAL,    "mistral-large-4", 0 },
    { 13, "Mistral Small",              LUMILA_PROVIDER_MISTRAL,    "mistral-small-latest", 0 },
    { 14, "Ollama Llama 4 Scout",       LUMILA_PROVIDER_OLLAMA,     "llama4", 0 },
    { 15, "Ollama Qwen3.6 27B",         LUMILA_PROVIDER_OLLAMA,     "qwen3.6:27b", 0 },
    { 16, "Ollama Mistral Small 3.1",   LUMILA_PROVIDER_OLLAMA,     "mistral-small3.1:24b", 0 },
    { 17, "Moonshot Kimi K3",           LUMILA_PROVIDER_MOONSHOT,   "kimi-k3", 0 },
    { 18, "OpenRouter GLM-4.6",         LUMILA_PROVIDER_OPENROUTER, "z-ai/glm-4.6", 0 },
    { 19, "OpenRouter Grok 4",          LUMILA_PROVIDER_OPENROUTER, "x-ai/grok-4", 0 },
    { 20, "OpenRouter Qwen3.5 Plus",    LUMILA_PROVIDER_OPENROUTER, "qwen/qwen3.5-plus-20260420", 0 },
    { 21, "OpenRouter Free",            LUMILA_PROVIDER_OPENROUTER, "openrouter/free", 0 },
};

/* Config-defined models (config.json "models": [...]). */
static GPtrArray *custom_models;

static void lumila_model_entry_free(LumilaModelEntry *e)
{
    g_free((gchar *)e->display_name);
    g_free((gchar *)e->model_name);
    g_free(e);
}

static void ensure_custom_models(void)
{
    if (!custom_models) {
        custom_models = g_ptr_array_new_with_free_func(
            (GDestroyNotify)lumila_model_entry_free);
    }
}

gint lumila_model_count(void)
{
    return LUMILA_NUM_PROVIDERS + (custom_models ? custom_models->len : 0);
}

const LumilaModelEntry *lumila_model_registry_lookup(gint provider_id)
{
    if (provider_id >= LUMILA_NUM_PROVIDERS && custom_models &&
        provider_id < LUMILA_NUM_PROVIDERS + (gint)custom_models->len) {
        return g_ptr_array_index(custom_models, provider_id - LUMILA_NUM_PROVIDERS);
    }
    if (provider_id < 0 || provider_id >= LUMILA_NUM_PROVIDERS) {
        provider_id = LUMILA_DEFAULT_MODEL_ID;
    }
    return &lumila_builtin_models[provider_id];
}

const gchar *lumila_model_registry_get_display_name(gint provider_id)
{
    return lumila_model_registry_lookup(provider_id)->display_name;
}

const gchar *lumila_model_registry_get_model_name(gint provider_id)
{
    return lumila_model_registry_lookup(provider_id)->model_name;
}

LumilaProviderType lumila_model_registry_get_type(gint provider_id)
{
    return lumila_model_registry_lookup(provider_id)->type;
}

gint lumila_model_registry_find_by_model_name(const gchar *model_name)
{
    if (!model_name || !*model_name) {
        return -1;
    }
    for (gint i = 0; i < lumila_model_count(); i++) {
        const LumilaModelEntry *e = lumila_model_registry_lookup(i);
        if (g_strcmp0(e->model_name, model_name) == 0) {
            return i;
        }
    }
    return -1;
}

gint lumila_model_registry_add(const gchar *display_name,
                               LumilaProviderType type,
                               const gchar *model_name,
                               guint flags)
{
    ensure_custom_models();
    LumilaModelEntry *e = g_new0(LumilaModelEntry, 1);
    e->provider_id = LUMILA_NUM_PROVIDERS + (gint)custom_models->len;
    e->display_name = g_strdup(display_name);
    e->type = type;
    e->model_name = g_strdup(model_name);
    e->flags = flags;
    g_ptr_array_add(custom_models, e);
    return e->provider_id;
}

guint lumila_model_flags_from_json(json_t *flags_array)
{
    static const struct { const gchar *name; guint flag; } map[] = {
        { "no_sampling",           LUMILA_MODEL_FLAG_NO_SAMPLING },
        { "max_completion_tokens", LUMILA_MODEL_FLAG_MAX_COMPLETION_TOKENS },
        { "reasoning_effort",      LUMILA_MODEL_FLAG_REASONING_EFFORT },
        { "anthropic_effort",      LUMILA_MODEL_FLAG_ANTHROPIC_EFFORT },
        { "thinking_level",        LUMILA_MODEL_FLAG_THINKING_LEVEL },
    };

    guint flags = LUMILA_MODEL_FLAG_NONE;
    if (!flags_array || !json_is_array(flags_array)) {
        return flags;
    }
    gsize i;
    json_t *v;
    json_array_foreach(flags_array, i, v) {
        if (!json_is_string(v)) {
            continue;
        }
        const gchar *name = json_string_value(v);
        for (gsize m = 0; m < G_N_ELEMENTS(map); m++) {
            if (g_strcmp0(name, map[m].name) == 0) {
                flags |= map[m].flag;
            }
        }
    }
    return flags;
}
