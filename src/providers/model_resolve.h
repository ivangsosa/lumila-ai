#ifndef LUMILA_MODEL_RESOLVE_H
#define LUMILA_MODEL_RESOLVE_H

#include "provider.h"
#include "../config.h"

/* Single source of truth for which API model id to send:
 *   1. "custom_models" config override (non-empty only)
 *   2. provider->model_name (from the registry entry / config models[])
 *   3. provider-specific fallback
 * Include only from provider implementation files that already link
 * against config.c (NOT provider_base.c, which stays config-free for
 * unit tests). */
static inline const gchar *lumila_provider_resolve_model(LumilaProvider *provider,
                                                          const gchar *fallback)
{
    const gchar *custom = lumila_config_get_custom_model(provider->type);
    if (custom && *custom) {
        return custom;
    }
    if (provider->model_name && *provider->model_name) {
        return provider->model_name;
    }
    return fallback;
}

#endif
