#include "history.h"
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
                const gchar *model = NULL;
                switch (id) {
                    case 0: model = "Claude Sonnet 4"; break;
                    case 1: model = "Claude Opus 4"; break;
                    case 2: model = "GPT-4.1"; break;
                    case 3: model = "GPT-4.1 mini"; break;
                    case 4: model = "Gemini 2.5 Flash"; break;
                    case 5: model = "Gemini 2.5 Pro"; break;
                    case 6: model = "Gemma 4 12B"; break;
                    case 7: model = "Ollama Llama 3.3"; break;
                    case 8: model = "Ollama Qwen3"; break;
                    case 9: model = "OpenRouter Auto"; break;
                    case 10: model = "DeepSeek V3"; break;
                    case 11: model = "DeepSeek R1"; break;
                    case 12: model = "Mistral Large"; break;
                    case 13: model = "Ollama Mistral Small"; break;
                    case 14: model = "OpenRouter Free"; break;
                    case 15: model = "Moonshot Kimi K2.6"; break;
                    case 16: model = "MAI-Code-1"; break;
                    case 17: model = "GPT-4.1 nano"; break;
                    case 18: model = "Mistral Small 3.1"; break;
                    case 19: model = "OpenRouter GLM-4"; break;
                    case 20: model = "OpenRouter Grok 3"; break;
                    case 21: model = "OpenRouter Qwen3-235B"; break;
                    default: model = "Unknown"; break;
                }
                entry->model_name = g_strdup(model);
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

typedef struct {
    gchar *role;
    gchar *content;
    gchar *timestamp;
} HistoryMessage;

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
        msgs = g_array_new(FALSE, FALSE, sizeof(HistoryMessage));
        size_t i;
        json_t *msg_obj;
        json_array_foreach(arr, i, msg_obj) {
            HistoryMessage msg;
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

gboolean lumila_history_delete(const gchar *filename)
{
    gchar *dir = get_history_dir();
    gchar *path = g_build_filename(dir, filename, NULL);
    g_free(dir);
    gboolean ok = g_remove(path) == 0;
    g_free(path);
    return ok;
}

static void free_history_message(gpointer data)
{
    HistoryMessage *msg = (HistoryMessage *)data;
    g_free(msg->role);
    g_free(msg->content);
    g_free(msg->timestamp);
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
