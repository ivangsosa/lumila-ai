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
        root = json_object();
        json_object_set_new(root, "version", json_string("0.1.0"));

        json_t *keys = json_object();
        json_object_set_new(root, "api_keys", keys);

        json_t *defaults = json_object();
        json_object_set_new(defaults, "temperature", json_real(0.7));
        json_object_set_new(defaults, "max_tokens", json_integer(1024));
        json_object_set_new(defaults, "top_p", json_real(1.0));
        json_object_set_new(defaults, "default_provider_id", json_integer(11));
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
        json_dump_file(config_root, config_file, JSON_INDENT(2));
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
    if (!config_root) return 0.7;

    json_t *defaults = json_object_get(config_root, "defaults");
    if (!defaults) return 0.7;

    json_t *temp = json_object_get(defaults, "temperature");
    if (temp && json_is_real(temp)) {
        return json_real_value(temp);
    }

    return 0.7;
}

gint lumila_config_get_max_tokens(void)
{
    if (!config_root) return 1024;

    json_t *defaults = json_object_get(config_root, "defaults");
    if (!defaults) return 1024;

    json_t *max_tok = json_object_get(defaults, "max_tokens");
    if (max_tok && json_is_integer(max_tok)) {
        return json_integer_value(max_tok);
    }

    return 1024;
}

gdouble lumila_config_get_top_p(void)
{
    if (!config_root) return 1.0;

    json_t *defaults = json_object_get(config_root, "defaults");
    if (!defaults) return 1.0;

    json_t *top = json_object_get(defaults, "top_p");
    if (top && json_is_real(top)) {
        return json_real_value(top);
    }

    return 1.0;
}

gint lumila_config_get_default_provider(void)
{
    if (!config_root) return 11; // OpenRouter por defecto

    json_t *defaults = json_object_get(config_root, "defaults");
    if (!defaults) return 11;

    json_t *provider = json_object_get(defaults, "default_provider_id");
    if (provider && json_is_integer(provider)) {
        return json_integer_value(provider);
    }
    
    return 11; // OpenRouter por defecto
}
