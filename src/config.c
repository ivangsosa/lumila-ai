#include "../config.h"
#include "config.h"
#include "providers/provider.h"
#include "plugin.h"
#include <geanyplugin.h>
#include <jansson.h>
#include <glib/gstdio.h>

#define CONFIG_FILE "lumila-ai/config.json"

static gchar *config_dir = NULL;
static gchar *config_file = NULL;
static json_t *config_root = NULL;
static json_t *api_keys_obj = NULL;
static gint64 config_mtime = 0;

static void update_config_mtime(void)
{
    struct stat st;
    config_mtime = (config_file && g_stat(config_file, &st) == 0)
        ? (gint64) st.st_mtime : 0;
}

/* If config.json changed on disk since our last read/write (e.g. the
 * user edited it by hand), reload before mutating so a manual edit is
 * not silently overwritten by a setter. */
static void reload_config_if_changed(void)
{
    if (!config_file || !config_root) return;
    struct stat st;
    if (g_stat(config_file, &st) != 0 || (gint64) st.st_mtime == config_mtime) {
        return;
    }
    json_error_t error;
    json_t *root = json_load_file(config_file, 0, &error);
    config_mtime = (gint64) st.st_mtime;
    if (!root) return; /* keep in-memory config if the file is unparseable */

    json_decref(config_root);
    config_root = root;
    api_keys_obj = json_object_get(root, "api_keys");
    if (!api_keys_obj) {
        api_keys_obj = json_object();
        json_object_set_new(root, "api_keys", api_keys_obj);
    }
}

static json_t *get_defaults(void)
{
    if (!config_root) return NULL;
    return json_object_get(config_root, "defaults");
}

/* Real-valued defaults key; accepts both JSON real and integer. */
static gdouble get_defaults_real(const gchar *key, gdouble fallback)
{
    json_t *defaults = get_defaults();
    if (!defaults) return fallback;
    json_t *v = json_object_get(defaults, key);
    if (v && json_is_real(v)) return json_real_value(v);
    if (v && json_is_integer(v)) return (gdouble) json_integer_value(v);
    return fallback;
}

static gint get_defaults_int(const gchar *key, gint fallback)
{
    json_t *defaults = get_defaults();
    if (!defaults) return fallback;
    json_t *v = json_object_get(defaults, key);
    if (v && json_is_integer(v)) return (gint) json_integer_value(v);
    return fallback;
}

static gboolean get_defaults_bool(const gchar *key, gboolean fallback)
{
    json_t *defaults = get_defaults();
    if (!defaults) return fallback;
    json_t *v = json_object_get(defaults, key);
    if (v && json_is_boolean(v)) return json_is_true(v);
    return fallback;
}

static const gchar *get_defaults_string(const gchar *key, const gchar *fallback)
{
    json_t *defaults = get_defaults();
    if (!defaults) return fallback;
    json_t *v = json_object_get(defaults, key);
    if (v && json_is_string(v) && *json_string_value(v)) return json_string_value(v);
    return fallback;
}

static json_t *ensure_defaults(void)
{
    if (!config_root) return NULL;
    json_t *defaults = json_object_get(config_root, "defaults");
    if (!defaults) {
        defaults = json_object();
        json_object_set_new(config_root, "defaults", defaults);
    }
    return defaults;
}

static void set_defaults_bool(const gchar *key, gboolean value)
{
    reload_config_if_changed();
    json_t *defaults = ensure_defaults();
    if (defaults) {
        json_object_set_new(defaults, key, json_boolean(value));
        lumila_config_save();
    }
}

static void set_defaults_string(const gchar *key, const gchar *value)
{
    reload_config_if_changed();
    json_t *defaults = ensure_defaults();
    if (defaults) {
        json_object_set_new(defaults, key, json_string(value ? value : ""));
        lumila_config_save();
    }
}

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
        json_object_set_new(defaults, "max_tokens", json_integer(8192));
        json_object_set_new(defaults, "top_p", json_real(0.9));
        json_object_set_new(defaults, "repeat_penalty", json_real(1.1));
        json_object_set_new(defaults, "default_provider_id", json_integer(9));
        json_object_set_new(defaults, "reasoning_effort", json_string("medium"));
        json_object_set_new(defaults, "thinking_level", json_string("medium"));
        json_object_set_new(defaults, "context_max_chars", json_integer(20000));
        json_object_set_new(defaults, "confirm_edits", json_true());
        json_object_set_new(defaults, "auto_retry", json_true());
        json_object_set_new(defaults, "history_max_messages", json_integer(40));
        json_object_set_new(defaults, "plan_mode", json_false());
        json_object_set_new(defaults, "autocomplete", json_false());
        json_object_set_new(defaults, "autocomplete_model", json_string(""));
        json_object_set_new(defaults, "diff_preview", json_true());
        json_object_set_new(root, "defaults", defaults);

        json_t *custom = json_object();
        json_object_set_new(root, "custom_models", custom);

        json_t *endpoints = json_object();
        json_object_set_new(root, "endpoints", endpoints);

        json_t *timeouts = json_object();
        json_object_set_new(root, "timeouts", timeouts);

        json_t *models = json_array();
        json_object_set_new(root, "models", models);
    }

    config_root = root;
    api_keys_obj = json_object_get(root, "api_keys");
    if (!api_keys_obj) {
        api_keys_obj = json_object();
        json_object_set_new(root, "api_keys", api_keys_obj);
    }
    update_config_mtime();
}

gboolean lumila_config_save(void)
{
    if (!config_root || !config_file) {
        return FALSE;
    }
    /* Atomic write: dump to a temp file in the same dir, then rename, so
     * a crash mid-write cannot corrupt config.json. */
    gchar *tmp = g_strdup_printf("%s.tmp", config_file);
    if (json_dump_file(config_root, tmp, JSON_INDENT(2)) != 0) {
        g_warning("Lumila: failed to write %s", tmp);
        g_free(tmp);
        return FALSE;
    }
    g_chmod(tmp, 0600);
    if (g_rename(tmp, config_file) != 0) {
        g_warning("Lumila: failed to rename %s -> %s", tmp, config_file);
        g_unlink(tmp);
        g_free(tmp);
        return FALSE;
    }
    g_free(tmp);
    update_config_mtime();
    return TRUE;
}

void lumila_config_cleanup(void)
{
    if (config_root) {
        lumila_config_save();
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
        const gchar *val = json_string_value(value);
        return (val && *val) ? val : NULL;
    }

    return NULL;
}

void lumila_config_set_api_key(LumilaProviderType provider, const gchar *key)
{
    if (!api_keys_obj) return;
    reload_config_if_changed();

    const gchar *key_name = lumila_provider_get_key_name(provider);
    json_object_set_new(api_keys_obj, key_name, json_string(key ? key : ""));
    lumila_config_save();
}

gdouble lumila_config_get_temperature(void)
{
    return get_defaults_real("temperature", 0.3);
}

gint lumila_config_get_max_tokens(void)
{
    return get_defaults_int("max_tokens", 8192);
}

gdouble lumila_config_get_top_p(void)
{
    return get_defaults_real("top_p", 0.9);
}

gdouble lumila_config_get_repeat_penalty(void)
{
    return get_defaults_real("repeat_penalty", 1.1);
}

gint lumila_config_get_default_provider(void)
{
    return get_defaults_int("default_provider_id", 9); // OpenRouter Auto por defecto
}

const gchar *lumila_config_get_default_model(void)
{
    return get_defaults_string("default_model", NULL);
}

void lumila_config_set_default_model(const gchar *model_name)
{
    set_defaults_string("default_model", model_name);
}

const gchar *lumila_config_get_custom_model(LumilaProviderType provider)
{
    if (!config_root) return NULL;

    json_t *custom = json_object_get(config_root, "custom_models");
    if (!custom || !json_is_object(custom)) return NULL;

    const gchar *key_name = lumila_provider_get_key_name(provider);
    json_t *model = json_object_get(custom, key_name);
    if (model && json_is_string(model)) {
        const gchar *val = json_string_value(model);
        return (val && *val) ? val : NULL;
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
        const gchar *val = json_string_value(ep);
        return (val && *val) ? val : NULL;
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

gboolean lumila_config_get_ask_mode(void)
{
    return get_defaults_bool("ask_mode", FALSE);
}

void lumila_config_set_ask_mode(gboolean enabled)
{
    set_defaults_bool("ask_mode", enabled);
}

gboolean lumila_config_get_plan_mode(void)
{
    return get_defaults_bool("plan_mode", FALSE);
}

void lumila_config_set_plan_mode(gboolean enabled)
{
    set_defaults_bool("plan_mode", enabled);
}

gint lumila_config_get_context_max_chars(void)
{
    return get_defaults_int("context_max_chars", 20000);
}

gboolean lumila_config_get_confirm_edits(void)
{
    return get_defaults_bool("confirm_edits", TRUE);
}

void lumila_config_set_confirm_edits(gboolean enabled)
{
    set_defaults_bool("confirm_edits", enabled);
}

gboolean lumila_config_get_auto_retry(void)
{
    return get_defaults_bool("auto_retry", TRUE);
}

gint lumila_config_get_history_max_messages(void)
{
    return get_defaults_int("history_max_messages", 40);
}

gboolean lumila_config_get_autocomplete(void)
{
    return get_defaults_bool("autocomplete", FALSE);
}

void lumila_config_set_autocomplete(gboolean enabled)
{
    set_defaults_bool("autocomplete", enabled);
}

const gchar *lumila_config_get_autocomplete_model(void)
{
    return get_defaults_string("autocomplete_model", NULL);
}

gboolean lumila_config_get_diff_preview(void)
{
    return get_defaults_bool("diff_preview", TRUE);
}

void lumila_config_set_diff_preview(gboolean enabled)
{
    set_defaults_bool("diff_preview", enabled);
}

const gchar *lumila_config_get_reasoning_effort(void)
{
    return get_defaults_string("reasoning_effort", "medium");
}

const gchar *lumila_config_get_thinking_level(void)
{
    return get_defaults_string("thinking_level", "medium");
}

json_t *lumila_config_get_models(void)
{
    if (!config_root) return NULL;
    json_t *models = json_object_get(config_root, "models");
    return (models && json_is_array(models)) ? models : NULL;
}
