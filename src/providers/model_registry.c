#include "model_registry.h"

const LumilaModelEntry lumila_model_registry[LUMILA_NUM_PROVIDERS] = {
    { 0, "Claude Sonnet 4",       LUMILA_PROVIDER_ANTHROPIC,  0, "claude-sonnet-4-20250514"  },
    { 1, "Claude Opus 4",         LUMILA_PROVIDER_ANTHROPIC,  1, "claude-opus-4-20250514"    },
    { 2, "GPT-4.1",               LUMILA_PROVIDER_OPENAI,     0, "gpt-4.1"                    },
    { 3, "GPT-4.1 mini",          LUMILA_PROVIDER_OPENAI,     1, "gpt-4.1-mini"               },
    { 4, "Gemini 2.5 Flash",      LUMILA_PROVIDER_GOOGLE,     0, "gemini-2.5-flash"           },
    { 5, "Gemini 2.5 Pro",        LUMILA_PROVIDER_GOOGLE,     1, "gemini-2.5-pro"             },
    { 6, "Gemma 4 12B",           LUMILA_PROVIDER_GOOGLE,     2, "gemma-4-12b-it"             },
    { 7, "Ollama Llama 3.3",      LUMILA_PROVIDER_OLLAMA,     0, "llama3.3"                   },
    { 8, "Ollama Qwen3",          LUMILA_PROVIDER_OLLAMA,     1, "qwen3:8b"                   },
    { 9, "OpenRouter Auto",       LUMILA_PROVIDER_OPENROUTER, 0, "openrouter/auto"            },
    { 10, "DeepSeek V3",          LUMILA_PROVIDER_DEEPSEEK,   0, "deepseek-v3"                },
    { 11, "DeepSeek R1",          LUMILA_PROVIDER_DEEPSEEK,   1, "deepseek-r1"                },
    { 12, "Mistral Large",        LUMILA_PROVIDER_MISTRAL,    0, "mistral-large-latest"       },
    { 13, "Ollama Mistral Small", LUMILA_PROVIDER_OLLAMA,     2, "mistral-small:24b"          },
    { 14, "OpenRouter Free",      LUMILA_PROVIDER_OPENROUTER, 1, "openrouter/free"            },
    { 15, "Moonshot Kimi K2.6",   LUMILA_PROVIDER_MOONSHOT,   0, "kimi-k2-6"                  },
    { 16, "MAI-Code-1",           LUMILA_PROVIDER_OPENROUTER, 2, "microsoft/mai-code-1"     },
    { 17, "GPT-4.1 nano",         LUMILA_PROVIDER_OPENAI,     2, "gpt-4.1-nano"               },
    { 18, "Mistral Small 3.1",    LUMILA_PROVIDER_MISTRAL,    1, "mistral-small-3.1-latest"   },
    { 19, "OpenRouter GLM-4",     LUMILA_PROVIDER_OPENROUTER, 3, "thudm/glm-4"                },
    { 20, "OpenRouter Grok 3",    LUMILA_PROVIDER_OPENROUTER, 4, "x-ai/grok-3"                },
    { 21, "OpenRouter Qwen3-235B",LUMILA_PROVIDER_OPENROUTER, 5, "qwen/qwen3-235b-a22b"       },
};

const LumilaModelEntry *lumila_model_registry_lookup(gint provider_id)
{
    if (provider_id < 0 || provider_id >= LUMILA_NUM_PROVIDERS)
        provider_id = 9; /* OpenRouter Auto default */
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
