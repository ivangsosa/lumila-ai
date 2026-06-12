#include "history.h"
#include "message.h"
#include "providers/model_registry.h"
#include <string.h>
#include <jansson.h>
#include <geanyplugin.h>
#include <glib/gstdio.h>

extern GeanyData *geany_data;

static gchar *get_history_dir(void)
{
    return g_build_filename(geany_data->app->configdir,
                            "plugins", "lumila-ai", "history", NULL);
}

GList *lumila_history_list(void)
{
    GList *list = NULL;
    gchar *dir = get_history_dir();
    GDir *gdir = g_dir_open(dir, 0, NULL);
    if (!gdir) {
        g_free(dir);
        return list;
    }

    const gchar *name;
    while ((name = g_dir_read_name(gdir)) != NULL) {
        if (!g_str_has_suffix(name, ".json"))
            continue;

        gchar *path = g_build_filename(dir, name, NULL);
        json_error_t err;
        json_t *root = json_load_file(path, 0, &err);
        if (root) {
            LumilaHistoryEntry *entry = g_new0(LumilaHistoryEntry, 1);
            entry->filename = g_strdup(name);

            json_t *title = json_object_get(root, "title");
            if (title && json_is_string(title))
                entry->title = g_strdup(json_string_value(title));
            else
                entry->title = g_strdup(name);

            json_t *created = json_object_get(root, "created_at");
            if (created && json_is_string(created))
                entry->created_at = g_strdup(json_string_value(created));
            else
                entry->created_at = g_strdup("");

            json_t *pid = json_object_get(root, "provider_id");
            if (pid && json_is_integer(pid)) {
                gint id = json_integer_value(pid);
                const gchar *model = lumila_model_registry_get_display_name(id);
                entry->model_name = g_strdup(model ? model : "Unknown");
            } else {
                entry->model_name = g_strdup("Unknown");
            }

            list = g_list_insert_sorted(list, entry,
                (GCompareFunc) g_strcmp0);
            json_decref(root);
        }
        g_free(path);
    }

    g_dir_close(gdir);
    g_free(dir);
    return list;
}

void lumila_history_entry_free(LumilaHistoryEntry *entry)
{
    if (!entry) return;
    g_free(entry->filename);
    g_free(entry->title);
    g_free(entry->created_at);
    g_free(entry->model_name);
    g_free(entry);
}

static void free_history_message(gpointer data)
{
    LumilaMessage *msg = (LumilaMessage *)data;
    g_free(msg->role);
    g_free(msg->content);
    g_free(msg->timestamp);
}

GArray *lumila_history_load_messages(const gchar *filename)
{
    gchar *dir = get_history_dir();
    gchar *path = g_build_filename(dir, filename, NULL);
    g_free(dir);

    GArray *msgs = NULL;
    json_error_t err;
    json_t *root = json_load_file(path, 0, &err);
    g_free(path);

    if (!root) return NULL;

    json_t *arr = json_object_get(root, "messages");
    if (arr && json_is_array(arr)) {
        msgs = g_array_new(FALSE, FALSE, sizeof(LumilaMessage));
        g_array_set_clear_func(msgs, free_history_message);
        size_t i;
        json_t *msg_obj;
        json_array_foreach(arr, i, msg_obj) {
            LumilaMessage msg;
            json_t *r = json_object_get(msg_obj, "role");
            json_t *c = json_object_get(msg_obj, "content");
            json_t *t = json_object_get(msg_obj, "timestamp");
            msg.role = g_strdup(json_string_value(r));
            msg.content = g_strdup(json_string_value(c));
            msg.timestamp = g_strdup(t && json_is_string(t) ? json_string_value(t) : "");
            g_array_append_val(msgs, msg);
        }
    }

    json_decref(root);
    return msgs;
}

void lumila_history_free_messages(GArray *msgs)
{
    if (msgs) {
        g_array_free(msgs, TRUE);
    }
}

gchar *lumila_history_load_title(const gchar *filename)
{
    gchar *dir = get_history_dir();
    gchar *path = g_build_filename(dir, filename, NULL);
    g_free(dir);

    json_error_t err;
    json_t *root = json_load_file(path, 0, &err);
    g_free(path);

    if (!root) return NULL;

    json_t *title = json_object_get(root, "title");
    gchar *result = NULL;
    if (title && json_is_string(title)) {
        const gchar *val = json_string_value(title);
        if (val && *val) {
            result = g_strdup(val);
        }
    }
    json_decref(root);
    return result;
}

gboolean lumila_history_delete(const gchar *filename)
{
    gchar *dir = get_history_dir();
    gchar *path = g_build_filename(dir, filename, NULL);
    g_free(dir);
    gboolean ok = g_remove(path) == 0;
    g_free(path);
    return ok;
}

gboolean lumila_history_rename(const gchar *old_filename, const gchar *new_title)
{
    gchar *dir = get_history_dir();
    gchar *path = g_build_filename(dir, old_filename, NULL);
    g_free(dir);

    json_error_t err;
    json_t *root = json_load_file(path, 0, &err);
    if (!root) {
        g_free(path);
        return FALSE;
    }

    json_object_set_new(root, "title", json_string(new_title));
    gboolean ok = json_dump_file(root, path, JSON_INDENT(2)) == 0;
    json_decref(root);
    g_free(path);
    return ok;
}
