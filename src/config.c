#include "../config.h"
#include "config.h"
#include "providers/provider.h"
#include "plugin.h"
#include <geanyplugin.h>
#include <jansson.h>

#define CONFIG_FILE "lumila-ai/config.json"

static gchar *config_dir = NULL;
static gchar *config_file = NULL;
static json_t *config_root = NULL;
static json_t *api_keys_obj = NULL;

void lumila_config_init(void)
{
    config_dir = g_build_filename(geany_data->app->configdir, "plugins", NULL);
    config_file = g_build_filename(config_dir, CONFIG_FILE, NULL);

    // Ensure directory exists
    gchar *dir = g_path_get_dirname(config_file);
    g_mkdir_with_parents(dir, 0755);
    g_free(dir);

    // Load or create config
    json_error_t error;
    json_t *root = json_load_file(config_file, 0, &error);

    if (!root) {
        if (error.text[0]) {
            g_warning("Lumila: config parse error: %s (creating new config)", error.text);
        }
        root = json_object();
        json_object_set_new(root, "version", json_string(LUMILA_VERSION));

        json_t *keys = json_object();
        json_object_set_new(root, "api_keys", keys);

        json_t *defaults = json_object();
        json_object_set_new(defaults, "temperature", json_real(0.3));
        json_object_set_new(defaults, "max_tokens", json_integer(2048));
        json_object_set_new(defaults, "top_p", json_real(0.9));
        json_object_set_new(defaults, "repeat_penalty", json_real(1.1));
        json_object_set_new(defaults, "default_provider_id", json_integer(9));
        json_object_set_new(root, "defaults", defaults);
    }

    config_root = root;
    api_keys_obj = json_object_get(root, "api_keys");
    if (!api_keys_obj) {
        api_keys_obj = json_object();
        json_object_set_new(root, "api_keys", api_keys_obj);
    }
}

void lumila_config_cleanup(void)
{
    if (config_root) {
        // Save config
        if (json_dump_file(config_root, config_file, JSON_INDENT(2)) != 0) {
            g_warning("Lumila: failed to save config to %s", config_file);
        }
        json_decref(config_root);
        config_root = NULL;
        api_keys_obj = NULL;
    }

    g_free(config_dir);
    g_free(config_file);
}

const gchar *lumila_config_get_api_key(LumilaProviderType provider)
{
    if (!api_keys_obj) return NULL;

    const gchar *key_name = lumila_provider_get_key_name(provider);
    json_t *value = json_object_get(api_keys_obj, key_name);

    if (value && json_is_string(value)) {
        return json_string_value(value);
    }

    return NULL;
}

void lumila_config_set_api_key(LumilaProviderType provider, const gchar *key)
{
    if (!api_keys_obj) return;

    const gchar *key_name = lumila_provider_get_key_name(provider);
    json_object_set_new(api_keys_obj, key_name, json_string(key));
}

gdouble lumila_config_get_temperature(void)
{
    if (!config_root) return 0.3;

    json_t *defaults = json_object_get(config_root, "defaults");
    if (!defaults) return 0.3;

    json_t *temp = json_object_get(defaults, "temperature");
    if (temp && json_is_real(temp)) {
        return json_real_value(temp);
    }

    return 0.3;
}

gint lumila_config_get_max_tokens(void)
{
    if (!config_root) return 2048;

    json_t *defaults = json_object_get(config_root, "defaults");
    if (!defaults) return 2048;

    json_t *max_tok = json_object_get(defaults, "max_tokens");
    if (max_tok && json_is_integer(max_tok)) {
        return json_integer_value(max_tok);
    }

    return 2048;
}

gdouble lumila_config_get_top_p(void)
{
    if (!config_root) return 0.9;

    json_t *defaults = json_object_get(config_root, "defaults");
    if (!defaults) return 0.9;

    json_t *top = json_object_get(defaults, "top_p");
    if (top && json_is_real(top)) {
        return json_real_value(top);
    }

    return 0.9;
}

gdouble lumila_config_get_repeat_penalty(void)
{
    if (!config_root) return 1.1;

    json_t *defaults = json_object_get(config_root, "defaults");
    if (!defaults) return 1.1;

    json_t *repeat = json_object_get(defaults, "repeat_penalty");
    if (repeat && json_is_real(repeat)) {
        return json_real_value(repeat);
    }

    return 1.1;
}

gint lumila_config_get_default_provider(void)
{
    if (!config_root) return 9; // OpenRouter Auto por defecto

    json_t *defaults = json_object_get(config_root, "defaults");
    if (!defaults) return 9;

    json_t *provider = json_object_get(defaults, "default_provider_id");
    if (provider && json_is_integer(provider)) {
        return json_integer_value(provider);
    }

    return 9; // OpenRouter Auto por defecto
}

const gchar *lumila_config_get_custom_model(LumilaProviderType provider)
{
    if (!config_root) return NULL;

    json_t *custom = json_object_get(config_root, "custom_models");
    if (!custom || !json_is_object(custom)) return NULL;

    const gchar *key_name = lumila_provider_get_key_name(provider);
    json_t *model = json_object_get(custom, key_name);
    if (model && json_is_string(model)) {
        return json_string_value(model);
    }

    return NULL;
}

const gchar *lumila_config_get_endpoint(LumilaProviderType provider)
{
    if (!config_root) return NULL;

    json_t *endpoints = json_object_get(config_root, "endpoints");
    if (!endpoints || !json_is_object(endpoints)) return NULL;

    const gchar *key_name = lumila_provider_get_key_name(provider);
    json_t *ep = json_object_get(endpoints, key_name);
    if (ep && json_is_string(ep)) {
        return json_string_value(ep);
    }

    return NULL;
}

gint lumila_config_get_timeout(LumilaProviderType provider)
{
    if (!config_root) return 0;

    json_t *timeouts = json_object_get(config_root, "timeouts");
    if (!timeouts || !json_is_object(timeouts)) return 0;

    const gchar *key_name = lumila_provider_get_key_name(provider);
    json_t *t = json_object_get(timeouts, key_name);
    if (t && json_is_integer(t)) {
        return (gint) json_integer_value(t);
    }

    return 0;
}
