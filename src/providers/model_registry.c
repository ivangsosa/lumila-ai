#include "model_registry.h"

const LumilaModelEntry lumila_model_registry[LUMILA_NUM_PROVIDERS] = {
    { 0, "Claude 3.5 Sonnet",     LUMILA_PROVIDER_ANTHROPIC, 0, "claude-3-5-sonnet-20241022" },
    { 1, "Claude 3 Opus",         LUMILA_PROVIDER_ANTHROPIC, 1, "claude-3-opus-20240229"     },
    { 2, "GPT-4o",                LUMILA_PROVIDER_OPENAI,    0, "gpt-4o"                     },
    { 3, "GPT-4o mini",           LUMILA_PROVIDER_OPENAI,    1, "gpt-4o-mini"                },
    { 4, "Gemini 1.5 Flash",      LUMILA_PROVIDER_GOOGLE,    0, "gemini-1.5-flash"           },
    { 5, "Gemini 1.5 Pro",        LUMILA_PROVIDER_GOOGLE,    1, "gemini-1.5-pro"             },
    { 6, "Ollama Llama 3.2",      LUMILA_PROVIDER_OLLAMA,    0, "llama3.2:3b"                },
    { 7, "Ollama Qwen 2.5",       LUMILA_PROVIDER_OLLAMA,    1, "qwen2.5:7b"                 },
    { 8, "OpenRouter",            LUMILA_PROVIDER_OPENROUTER,0, "openrouter"                 },
};

const LumilaModelEntry *lumila_model_registry_lookup(gint provider_id)
{
    if (provider_id < 0 || provider_id >= LUMILA_NUM_PROVIDERS)
        provider_id = 8; /* OpenRouter default */
    return &lumila_model_registry[provider_id];
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
