#include "model_registry.h"

const LumilaModelEntry lumila_model_registry[LUMILA_NUM_PROVIDERS] = {
    { 0, "Claude Sonnet 4",       LUMILA_PROVIDER_ANTHROPIC,  0, "claude-sonnet-4-20250514"  },
    { 1, "Claude Opus 4",         LUMILA_PROVIDER_ANTHROPIC,  1, "claude-opus-4-20250514"    },
    { 2, "GPT-4.1",               LUMILA_PROVIDER_OPENAI,     0, "gpt-4.1"                    },
    { 3, "GPT-4.1 mini",          LUMILA_PROVIDER_OPENAI,     1, "gpt-4.1-mini"               },
    { 4, "Gemini 2.5 Flash",      LUMILA_PROVIDER_GOOGLE,     0, "gemini-2.5-flash-preview-05-20" },
    { 5, "Gemini 2.5 Pro",        LUMILA_PROVIDER_GOOGLE,     1, "gemini-2.5-pro-preview-05-20"   },
    { 6, "Ollama Llama 3.3",      LUMILA_PROVIDER_OLLAMA,     0, "llama3.3"                   },
    { 7, "Ollama Qwen3",          LUMILA_PROVIDER_OLLAMA,     1, "qwen3:8b"                   },
    { 8, "OpenRouter Auto",       LUMILA_PROVIDER_OPENROUTER, 0, "openrouter/auto"            },
    { 9, "DeepSeek Chat",         LUMILA_PROVIDER_DEEPSEEK,   0, "deepseek-chat"              },
    { 10, "DeepSeek Reasoner",    LUMILA_PROVIDER_DEEPSEEK,   1, "deepseek-reasoner"          },
    { 11, "Mistral Large",        LUMILA_PROVIDER_MISTRAL,    0, "mistral-large-latest"       },
    { 12, "Ollama Mistral Small", LUMILA_PROVIDER_OLLAMA,     2, "mistral-small:24b"          },
    { 13, "OpenRouter Free",      LUMILA_PROVIDER_OPENROUTER, 1, "openrouter/free"            },
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
