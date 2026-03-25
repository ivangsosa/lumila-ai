#include "provider.h"
#include "openai.h"
#include "anthropic.h"
#include "google.h"
#include "ollama.h"
#include "openrouter.h"
#include <string.h>

const gchar *lumila_provider_get_key_name(LumilaProviderType type)
{
    switch (type) {
        case LUMILA_PROVIDER_OPENAI: return "openai";
        case LUMILA_PROVIDER_ANTHROPIC: return "anthropic";
        case LUMILA_PROVIDER_GOOGLE: return "google";
        case LUMILA_PROVIDER_KIMI: return "kimi";
        case LUMILA_PROVIDER_OPENROUTER: return "openrouter";
        case LUMILA_PROVIDER_OLLAMA: return "ollama";
        default: return "unknown";
    }
}

const gchar *lumila_provider_get_name(LumilaProviderType type)
{
    switch (type) {
        case LUMILA_PROVIDER_OPENAI: return "OpenAI";
        case LUMILA_PROVIDER_ANTHROPIC: return "Anthropic";
        case LUMILA_PROVIDER_GOOGLE: return "Google";
        case LUMILA_PROVIDER_KIMI: return "Kimi";
        case LUMILA_PROVIDER_OPENROUTER: return "OpenRouter";
        case LUMILA_PROVIDER_OLLAMA: return "Ollama";
        default: return "Unknown";
    }
}

void lumila_provider_send_message(LumilaProvider *provider, const gchar *message,
                                   LumilaResponseCallback callback, gpointer user_data)
{
    if (provider && provider->send_message) {
        provider->send_message(provider, message, callback, user_data);
    }
}

void lumila_provider_cancel(LumilaProvider *provider)
{
    if (provider && provider->cancel) {
        provider->cancel(provider);
    }
}

LumilaProvider *lumila_provider_create(LumilaProviderType type)
{
    switch (type) {
        case LUMILA_PROVIDER_OPENAI:
            return openai_provider_new();
        case LUMILA_PROVIDER_ANTHROPIC:
            return anthropic_provider_new();
        case LUMILA_PROVIDER_GOOGLE:
            return google_provider_new();
        case LUMILA_PROVIDER_OLLAMA:
            return ollama_provider_new();
        case LUMILA_PROVIDER_OPENROUTER:
            return openrouter_provider_new();
        case LUMILA_PROVIDER_KIMI:
            // Not implemented yet, fallback to OpenAI
            return openai_provider_new();
        default:
            return openai_provider_new();
    }
}

void lumila_provider_free(LumilaProvider *provider)
{
    if (provider) {
        if (provider->cancel) {
            provider->cancel(provider);
        }
        if (provider->session) {
            g_object_unref(provider->session);
        }
        if (provider->cancellable) {
            g_object_unref(provider->cancellable);
        }
        g_free(provider);
    }
}
