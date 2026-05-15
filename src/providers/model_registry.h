#ifndef LUMILA_MODEL_REGISTRY_H
#define LUMILA_MODEL_REGISTRY_H

#include "provider.h"

#define LUMILA_NUM_PROVIDERS 14

typedef struct {
    gint          provider_id;
    const gchar  *display_name;
    LumilaProviderType type;
    gint          model_index;
    const gchar  *model_name;
} LumilaModelEntry;

extern const LumilaModelEntry lumila_model_registry[LUMILA_NUM_PROVIDERS];

const LumilaModelEntry *lumila_model_registry_lookup(gint provider_id);
const gchar *lumila_model_registry_get_display_name(gint provider_id);
const gchar *lumila_model_registry_get_model_name(gint provider_id);
LumilaProviderType lumila_model_registry_get_type(gint provider_id);

#endif
