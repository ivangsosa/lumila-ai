#include "model_registry.h"

const LumilaModelEntry lumila_model_registry[LUMILA_NUM_PROVIDERS] = {
    { 0,  "Claude Sonnet 5",            LUMILA_PROVIDER_ANTHROPIC,  0, "claude-sonnet-5"                    },
    { 1,  "Claude Opus 5",              LUMILA_PROVIDER_ANTHROPIC,  1, "claude-opus-5"                      },
    { 2,  "Claude Haiku 4.5",           LUMILA_PROVIDER_ANTHROPIC,  2, "claude-haiku-4-5"                   },
    { 3,  "GPT-5.6 Sol",                LUMILA_PROVIDER_OPENAI,     0, "gpt-5.6-sol"                        },
    { 4,  "GPT-5.6 Terra",              LUMILA_PROVIDER_OPENAI,     1, "gpt-5.6-terra"                      },
    { 5,  "GPT-5.6 Luna",               LUMILA_PROVIDER_OPENAI,     2, "gpt-5.6-luna"                       },
    { 6,  "Gemini 3.6 Flash",           LUMILA_PROVIDER_GOOGLE,     0, "gemini-3.6-flash"                   },
    { 7,  "Gemini 3.1 Pro Preview",     LUMILA_PROVIDER_GOOGLE,     1, "gemini-3.1-pro-preview"             },
    { 8,  "Gemini 2.5 Pro",             LUMILA_PROVIDER_GOOGLE,     2, "gemini-2.5-pro"                     },
    { 9,  "OpenRouter Auto",            LUMILA_PROVIDER_OPENROUTER, 0, "openrouter/auto"                    },
    { 10, "DeepSeek V4 Flash",          LUMILA_PROVIDER_DEEPSEEK,   0, "deepseek-v4-flash"                  },
    { 11, "DeepSeek V4 Pro",            LUMILA_PROVIDER_DEEPSEEK,   1, "deepseek-v4-pro"                    },
    { 12, "Mistral Large 3",            LUMILA_PROVIDER_MISTRAL,    0, "mistral-large-latest"               },
    { 13, "Mistral Small 4",            LUMILA_PROVIDER_MISTRAL,    1, "mistral-small-latest"               },
    { 14, "Ollama Llama 4 Scout",       LUMILA_PROVIDER_OLLAMA,     0, "llama4"                             },
    { 15, "Ollama Qwen3.6 27B",         LUMILA_PROVIDER_OLLAMA,     1, "qwen3.6:27b"                        },
    { 16, "Ollama Mistral Small 3.1",   LUMILA_PROVIDER_OLLAMA,     2, "mistral-small3.1:24b"               },
    { 17, "Moonshot Kimi K3",           LUMILA_PROVIDER_MOONSHOT,   0, "kimi-k3"                            },
    { 18, "OpenRouter GLM-4.6",         LUMILA_PROVIDER_OPENROUTER, 1, "z-ai/glm-4.6"                       },
    { 19, "OpenRouter Grok 4",          LUMILA_PROVIDER_OPENROUTER, 2, "x-ai/grok-4"                        },
    { 20, "OpenRouter Qwen3.5 Plus",    LUMILA_PROVIDER_OPENROUTER, 3, "qwen/qwen3.5-plus-20260420"         },
    { 21, "OpenRouter Llama 3.3 Free",  LUMILA_PROVIDER_OPENROUTER, 4, "meta-llama/llama-3.3-70b-instruct:free" },
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
