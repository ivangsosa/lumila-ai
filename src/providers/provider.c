#include "provider.h"
#include "openai.h"
#include "anthropic.h"
#include "google.h"
#include "moonshot.h"
#include "ollama.h"
#include "openrouter.h"
#include "deepseek.h"
#include "mistral.h"
#include <string.h>

const gchar *lumila_provider_get_key_name(LumilaProviderType type)
{
    switch (type) {
        case LUMILA_PROVIDER_OPENAI: return "openai";
        case LUMILA_PROVIDER_ANTHROPIC: return "anthropic";
        case LUMILA_PROVIDER_GOOGLE: return "google";
        case LUMILA_PROVIDER_MOONSHOT: return "moonshot";
        case LUMILA_PROVIDER_OPENROUTER: return "openrouter";
        case LUMILA_PROVIDER_OLLAMA: return "ollama";
        case LUMILA_PROVIDER_DEEPSEEK: return "deepseek";
        case LUMILA_PROVIDER_MISTRAL: return "mistral";
        default: return "unknown";
    }
}

const gchar *lumila_provider_get_name(LumilaProviderType type)
{
    switch (type) {
        case LUMILA_PROVIDER_OPENAI: return "OpenAI";
        case LUMILA_PROVIDER_ANTHROPIC: return "Anthropic";
        case LUMILA_PROVIDER_GOOGLE: return "Google";
        case LUMILA_PROVIDER_MOONSHOT: return "Moonshot";
        case LUMILA_PROVIDER_OPENROUTER: return "OpenRouter";
        case LUMILA_PROVIDER_OLLAMA: return "Ollama";
        case LUMILA_PROVIDER_DEEPSEEK: return "DeepSeek";
        case LUMILA_PROVIDER_MISTRAL: return "Mistral";
        default: return "Unknown";
    }
}

void lumila_provider_send_message(LumilaProvider *provider, const gchar *system_prompt,
                                   GArray *messages,
                                   LumilaResponseCallback callback, gpointer user_data)
{
    if (provider && provider->send_message) {
        provider->send_message(provider, system_prompt, messages, callback, user_data);
    }
}

void lumila_provider_send_message_stream(LumilaProvider *provider, const gchar *system_prompt,
                                          GArray *messages,
                                          LumilaChunkCallback chunk_cb,
                                          LumilaResponseCallback final_cb, gpointer user_data)
{
    if (provider && provider->send_message_stream) {
        provider->send_message_stream(provider, system_prompt, messages, chunk_cb, final_cb, user_data);
    } else if (provider && provider->send_message) {
        provider->send_message(provider, system_prompt, messages, final_cb, user_data);
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
        case LUMILA_PROVIDER_DEEPSEEK:
            return deepseek_provider_new();
        case LUMILA_PROVIDER_MISTRAL:
            return mistral_provider_new();
        case LUMILA_PROVIDER_MOONSHOT:
            return moonshot_provider_new();
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
        if (provider->pending_msg) {
            g_object_unref(provider->pending_msg);
            provider->pending_msg = NULL;
        }
        if (provider->session) {
            g_object_unref(provider->session);
        }
        if (provider->cancellable) {
            g_object_unref(provider->cancellable);
            provider->cancellable = NULL;
        }
        g_free(provider);
    }
}
