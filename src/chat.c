#include "chat.h"
#include "chat_ui.h"
#include "message.h"
#include "providers/provider.h"
#include "providers/model_registry.h"
#include "config.h"
#include "plugin.h"
#include "sidebar.h"
#include "history.h"
#include "utils.h"
#include "diff.h"
#include "autocomplete.h"
#include <geanyplugin.h>
#include <string.h>
#include <time.h>
#include <jansson.h>

#define SYSTEM_PROMPT \
    "You are Lumila, an AI coding assistant inside the Geany editor. " \
    "Help the user with coding tasks, explanations, and file edits. " \
    "\n" \
    "To edit an open file, use this exact format (no extra text inside the block):\n" \
    "```file:filename.ext\n" \
    "<complete new file content>\n" \
    "```\n" \
    "If the file content itself contains fenced code blocks (e.g. markdown), wrap the " \
    "file block in four backticks (````file:name) so nested fences do not end it early.\n" \
    "\n" \
    "If no file is open or the user just wants to chat, answer normally without using ```file: blocks. " \
    "Do not output system metadata, safety labels, or reasoning steps. " \
    "Respond directly with the answer or code.\n\n"

#define PLAN_PROMPT \
    "You are Lumila, an AI coding assistant inside the Geany editor. " \
    "The user wants you to PLAN before editing. Analyze the request and the " \
    "attached context, then produce a clear, step-by-step implementation plan " \
    "in markdown (files to touch, changes to make, risks). " \
    "DO NOT use ```file: blocks and DO NOT write the implementation yet; " \
    "the user will press \"Execute Plan\" to apply it.\n\n"

static void append_message_to_view(const gchar *role, const gchar *content);
static gboolean deferred_provider_free(gpointer user_data);
static gchar *get_current_timestamp(void);
static gchar *apply_file_edits(const gchar *response);
static void send_request(const gchar *system_prompt, GArray *messages_to_send);
static void retry_context_clear(void);

static GtkTextView *chat_view = NULL;
static GArray *messages = NULL;
static LumilaProvider *current_provider = NULL;
static gint current_provider_id = 0;
static gchar *conversation_id = NULL;
static gchar *conversation_created_at = NULL;
static gchar *history_dir = NULL;
static gchar *conversation_title = NULL;
static gboolean ask_mode = FALSE;
static gboolean plan_mode = FALSE;
static gboolean plan_pending = FALSE;
static gboolean is_streaming = FALSE;
static gboolean request_cancelled = FALSE;
static gboolean shutting_down = FALSE;
static guint request_seq = 0;

#define RETRY_MAX_ATTEMPTS 3

/* Idle callback data for thread-safe UI updates */
typedef struct {
    gchar *chunk;
    gboolean is_final;
    gchar *full_response;
    guint request_id;
} StreamIdleData;

/* Kept between attempts so a transient error can be retried without
 * rebuilding the context (and while the UI stays non-blocking). */
typedef struct {
    gchar *system_prompt;
    GArray *messages;
    gint attempts;
    guint source_id;
    guint request_id;
} RetryContext;

static RetryContext *pending_retry = NULL;

static void stream_idle_data_free(StreamIdleData *data)
{
    g_free(data->chunk);
    g_free(data->full_response);
    g_free(data);
}

static void messages_array_free(GArray *arr)
{
    if (!arr) return;
    for (guint i = 0; i < arr->len; i++) {
        LumilaMessage *m = &g_array_index(arr, LumilaMessage, i);
        g_free(m->role);
        g_free(m->content);
        g_free(m->timestamp);
    }
    g_array_free(arr, TRUE);
}

static gboolean retry_request_cb(gpointer data)
{
    (void)data;
    RetryContext *ctx = pending_retry;
    if (!ctx || shutting_down || request_cancelled) {
        return G_SOURCE_REMOVE;
    }
    ctx->source_id = 0;
    ctx->attempts++;

    gchar *st = g_strdup_printf(_("Retrying... (%d/%d)"), ctx->attempts, RETRY_MAX_ATTEMPTS);
    lumila_sidebar_set_status(st);
    g_free(st);

    send_request(ctx->system_prompt, ctx->messages);
    return G_SOURCE_REMOVE;
}

static void schedule_retry(void)
{
    if (!pending_retry) return;
    guint secs = 1u << pending_retry->attempts; /* 1, 2, 4 s */
    gchar *st = g_strdup_printf(_("Transient error. Retrying in %us..."), secs);
    lumila_sidebar_set_status(st);
    g_free(st);
    pending_retry->source_id = g_timeout_add_seconds(secs, retry_request_cb, pending_retry);
}

static void retry_context_clear(void)
{
    if (!pending_retry) return;
    if (pending_retry->source_id) {
        g_source_remove(pending_retry->source_id);
    }
    g_free(pending_retry->system_prompt);
    messages_array_free(pending_retry->messages);
    g_free(pending_retry);
    pending_retry = NULL;
}

/* Take ownership of the request payload so it can be retried later. */
static void retry_context_take(const gchar *system_prompt, GArray *messages_to_send)
{
    retry_context_clear();
    pending_retry = g_new0(RetryContext, 1);
    pending_retry->system_prompt = g_strdup(system_prompt);
    pending_retry->messages = messages_to_send; /* transfer */
    pending_retry->attempts = 0;
    pending_retry->source_id = 0;
    pending_retry->request_id = request_seq;
}

static gboolean stream_idle_callback(gpointer user_data)
{
    StreamIdleData *data = (StreamIdleData *)user_data;

    /* Stale request (superseded or cancelled) or teardown: drop quietly. */
    if (shutting_down || data->request_id != request_seq || !chat_view) {
        stream_idle_data_free(data);
        return G_SOURCE_REMOVE;
    }

    if (data->is_final) {
        is_streaming = FALSE;
        const gchar *resp = data->full_response;
        gboolean was_cancelled = request_cancelled;
        request_cancelled = FALSE;

        if (was_cancelled) {
            lumila_chat_ui_stream_end(chat_view);
            append_message_to_view("system", "[Cancelled]");
        } else if (resp && lumila_utils_is_transient_response(resp) &&
                   lumila_config_get_auto_retry() && pending_retry &&
                   pending_retry->attempts < RETRY_MAX_ATTEMPTS &&
                   pending_retry->request_id == data->request_id) {
            lumila_chat_ui_stream_end(chat_view);
            is_streaming = TRUE; /* keep the "one request" guard during the wait */
            schedule_retry();
            lumila_sidebar_set_input_sensitive(FALSE);
            stream_idle_data_free(data);
            return G_SOURCE_REMOVE;
        } else if (resp && lumila_utils_is_error_response(resp)) {
            lumila_chat_ui_stream_end(chat_view);
            append_message_to_view("assistant", resp);
            retry_context_clear();
        } else if (resp) {
            lumila_chat_ui_stream_end_and_render(chat_view, resp);

            LumilaMessage msg = {
                .role = g_strdup("assistant"),
                .content = g_strdup(resp),
                .timestamp = get_current_timestamp()
            };
            g_array_append_val(messages, msg);
            retry_context_clear();

            if (plan_mode) {
                plan_pending = TRUE;
                append_message_to_view("system",
                    "[Plan ready — use ... menu → \"Execute Plan\" to apply it]");
            } else if (!ask_mode) {
                gchar *summary = apply_file_edits(resp);
                if (summary && *summary) {
                    GtkTextBuffer *buffer = gtk_text_view_get_buffer(chat_view);
                    GtkTextIter end;
                    gtk_text_buffer_get_end_iter(buffer, &end);
                    gtk_text_buffer_insert(buffer, &end, "\n", -1);
                    gtk_text_buffer_insert(buffer, &end, summary, -1);
                    gtk_text_buffer_insert(buffer, &end, "\n", -1);
                }
                g_free(summary);
            }
        } else {
            lumila_chat_ui_stream_end(chat_view);
            append_message_to_view("assistant", "Error: Failed to get response");
            retry_context_clear();
        }
        lumila_sidebar_set_status(NULL);
        lumila_sidebar_set_input_sensitive(TRUE);
        if (current_provider && current_provider->last_usage > 0) {
            lumila_sidebar_set_token_estimate(current_provider->last_usage);
        }
    } else if (data->chunk) {
        lumila_chat_ui_stream_append(chat_view, data->chunk);
    }

    stream_idle_data_free(data);
    return G_SOURCE_REMOVE;
}

static void on_stream_chunk(const gchar *chunk, gboolean is_final, gpointer user_data)
{
    (void)is_final;

    if (!chunk || !*chunk || shutting_down) return;

    /* user_data carries the request id captured when the request was
     * dispatched — reading the global request_seq here would wrongly
     * match a newer request sent after a cancel. */
    StreamIdleData *data = g_new0(StreamIdleData, 1);
    data->chunk = g_strdup(chunk);
    data->is_final = FALSE;
    data->request_id = GPOINTER_TO_UINT(user_data);
    g_idle_add(stream_idle_callback, data);
}

static void on_stream_final(const gchar *response, gpointer user_data)
{
    StreamIdleData *data = g_new0(StreamIdleData, 1);
    data->is_final = TRUE;
    data->full_response = response ? g_strdup(response) : NULL;
    data->request_id = GPOINTER_TO_UINT(user_data);
    g_idle_add(stream_idle_callback, data);
}

/* Register models declared in config.json ("models": [...]) so they show
 * up in the sidebar combo without recompiling. */
static void register_config_models(void)
{
    json_t *models = lumila_config_get_models();
    if (!models) return;

    gsize i;
    json_t *obj;
    json_array_foreach(models, i, obj) {
        if (!json_is_object(obj)) continue;
        json_t *jname = json_object_get(obj, "name");
        json_t *jprovider = json_object_get(obj, "provider");
        json_t *jmodel = json_object_get(obj, "model");
        if (!json_is_string(jname) || !json_is_string(jprovider) ||
            !json_is_string(jmodel)) {
            continue;
        }
        gint type = lumila_provider_type_from_key_name(json_string_value(jprovider));
        if (type < 0) {
            g_warning("Lumila: unknown provider \"%s\" in models[]",
                      json_string_value(jprovider));
            continue;
        }
        /* Skip duplicates: re-initializing the plugin registers the same
         * models[] again. */
        gboolean dup = FALSE;
        for (gint i = 0; i < lumila_model_count(); i++) {
            const LumilaModelEntry *e = lumila_model_registry_lookup(i);
            if (g_strcmp0(e->model_name, json_string_value(jmodel)) == 0 &&
                g_strcmp0(e->display_name, json_string_value(jname)) == 0) {
                dup = TRUE;
                break;
            }
        }
        if (dup) continue;
        guint flags = lumila_model_flags_from_json(json_object_get(obj, "flags"));
        lumila_model_registry_add(json_string_value(jname),
                                  (LumilaProviderType)type,
                                  json_string_value(jmodel), flags);
    }
}

void lumila_chat_init(void)
{
    /* Reset module state: this also runs when Geany re-enables the plugin
     * after a cleanup that left shutting_down set. */
    shutting_down = FALSE;
    is_streaming = FALSE;
    request_cancelled = FALSE;
    plan_pending = FALSE;

    messages = g_array_new(FALSE, FALSE, sizeof(LumilaMessage));

    /* Config-defined models must exist before the default is resolved. */
    register_config_models();

    /* Default model: prefer the persisted model name, fall back to the
     * legacy numeric id. */
    const gchar *default_model = lumila_config_get_default_model();
    gint resolved = default_model
        ? lumila_model_registry_find_by_model_name(default_model) : -1;
    current_provider_id = (resolved >= 0) ? resolved
        : lumila_config_get_default_provider();

    const LumilaModelEntry *entry = lumila_model_registry_lookup(current_provider_id);

    current_provider = lumila_provider_create(entry->type);
    current_provider->model_id = entry->provider_id;
    current_provider->model_name = entry->model_name;
    current_provider->model_flags = entry->flags;

    // Load persisted modes
    ask_mode = lumila_config_get_ask_mode();
    plan_mode = lumila_config_get_plan_mode();

    // Create history directory
    const gchar *config_dir = geany->app->configdir;
    history_dir = g_build_filename(config_dir, "plugins", "lumila-ai", "history", NULL);
    g_mkdir_with_parents(history_dir, 0755);

    // Generate conversation ID (counter avoids same-second collisions)
    static guint conv_counter = 0;
    GDateTime *now = g_date_time_new_now_local();
    gchar *ts = g_date_time_format(now, "%Y%m%d-%H%M%S");
    conversation_id = g_strdup_printf("conversation-%s-%u", ts, conv_counter++);
    g_free(ts);
    g_date_time_unref(now);

    conversation_created_at = get_current_timestamp();
}

void lumila_chat_cleanup(void)
{
    shutting_down = TRUE;
    request_seq++;
    retry_context_clear();

    if (current_provider) {
        lumila_provider_cancel(current_provider);
        /* Defer the free: cancelled soup requests still invoke their
         * completion callback (with the provider as user_data) on the
         * next main-loop turn. The module is resident so the idle
         * callback stays valid. */
        g_idle_add(deferred_provider_free, current_provider);
        current_provider = NULL;
    }
    chat_view = NULL;

    // Save history before cleanup
    lumila_chat_save_history();

    if (messages) {
        for (guint i = 0; i < messages->len; i++) {
            LumilaMessage *msg = &g_array_index(messages, LumilaMessage, i);
            g_free(msg->role);
            g_free(msg->content);
            g_free(msg->timestamp);
        }
        g_array_free(messages, TRUE);
        messages = NULL;
    }

    g_free(conversation_id);
    conversation_id = NULL;
    g_free(conversation_created_at);
    conversation_created_at = NULL;
    g_free(history_dir);
    history_dir = NULL;
    g_free(conversation_title);
    conversation_title = NULL;
}

void lumila_chat_set_view(GtkTextView *view)
{
    chat_view = view;
    lumila_chat_ui_init(view);
}

static void append_message_to_view(const gchar *role, const gchar *content)
{
    if (!chat_view || !content) return;

    if (g_str_equal(role, "user")) {
        lumila_chat_ui_append_user_message(chat_view, content);
    } else {
        lumila_chat_ui_append_ai_message(chat_view, content);
    }
}

static gchar *get_current_timestamp(void)
{
    GDateTime *now = g_date_time_new_now_local();
    gchar *timestamp = g_date_time_format(now, "%Y-%m-%d %H:%M:%S");
    g_date_time_unref(now);
    return timestamp;
}

static gchar *get_open_files_context(gboolean skip_active)
{
    GString *context = g_string_new("");
    GeanyDocument *doc;
    gint i = 0;
    /* Character budget for the auto-attached context.
     * The active file goes first, then the rest. */
    gsize remaining = (gsize)MAX(lumila_config_get_context_max_chars(), 1024);

    /* Primero el archivo activo */
    GeanyDocument *active = document_get_current();
    if (active && !skip_active && active->file_name &&
        active->editor && active->editor->sci) {
        gchar *filename = g_path_get_basename(active->file_name);
        gchar *content = sci_get_contents(active->editor->sci, -1);
        if (content && *content) {
            gsize len = strlen(content);
            if (len > remaining) len = remaining;
            g_string_append_printf(context, "\n\n[Archivo activo: %s]\n```\n%.*s\n```\n",
                                   filename, (gint)len, content);
            remaining -= len;
        }
        g_free(content);
        g_free(filename);
    }

    /* Luego el resto de archivos abiertos */
    while ((doc = document_index(i)) != NULL && remaining > 0) {
        if (doc == active) { i++; continue; }
        if (doc->file_name && doc->editor && doc->editor->sci) {
            gchar *filename = g_path_get_basename(doc->file_name);
            gchar *content = sci_get_contents(doc->editor->sci, -1);

            if (content && *content) {
                gsize len = strlen(content);
                if (len > remaining) len = remaining;
                g_string_append_printf(context, "\n\n[Archivo abierto: %s]\n```\n%.*s\n```\n",
                                       filename, (gint)len, content);
                remaining -= len;
            }

            g_free(content);
            g_free(filename);
        }
        i++;
    }

    gchar *result = g_string_free(context, FALSE);
    return result;
}

/* project_get_base_path() is private; use geany_data->app->project.
 * Returns a borrowed pointer to the open project's base dir, or NULL. */
static const gchar *get_project_base(void)
{
    if (geany_data && geany_data->app && geany_data->app->project &&
        geany_data->app->project->base_path) {
        return geany_data->app->project->base_path;
    }
    return NULL;
}

/* Resolve an edit filename to an absolute path relative to the project
 * or the current document directory. Caller frees. */
gchar *lumila_chat_resolve_edit_path(const gchar *file_name)
{
    if (!file_name || !*file_name) return NULL;
    if (g_path_is_absolute(file_name)) {
        return g_strdup(file_name);
    }
    const gchar *base = get_project_base();
    if (base && *base) {
        gchar *path = g_build_filename(base, file_name, NULL);
        return path;
    }
    GeanyDocument *doc = document_get_current();
    if (doc && doc->file_name) {
        gchar *dir = g_path_get_dirname(doc->file_name);
        gchar *path = g_build_filename(dir, file_name, NULL);
        g_free(dir);
        return path;
    }
    return g_strdup(file_name);
}

/* Find an open document matching the edit filename: exact path match,
 * then suffix match (doc path ends with "/name"), then basename. */
GeanyDocument *lumila_chat_doc_for_edit(const gchar *file_name)
{
    gchar *resolved = lumila_chat_resolve_edit_path(file_name);
    gchar *basename = g_path_get_basename(file_name);
    GeanyDocument *match = NULL;
    GeanyDocument *basename_match = NULL;
    gint i = 0;
    GeanyDocument *doc;

    while ((doc = document_index(i)) != NULL) {
        if (doc->file_name) {
            if (resolved && g_strcmp0(doc->file_name, resolved) == 0) {
                match = doc;
                break;
            }
            gsize flen = strlen(file_name);
            gsize dlen = strlen(doc->file_name);
            if (dlen > flen && doc->file_name[dlen - flen - 1] == '/' &&
                g_strcmp0(doc->file_name + dlen - flen, file_name) == 0) {
                match = doc;
                break;
            }
            if (!basename_match) {
                gchar *doc_base = g_path_get_basename(doc->file_name);
                if (g_strcmp0(doc_base, basename) == 0) {
                    basename_match = doc;
                }
                g_free(doc_base);
            }
        }
        i++;
    }

    g_free(resolved);
    g_free(basename);
    return match ? match : basename_match;
}

static void replace_document_content(GeanyDocument *doc, const gchar *content)
{
    if (!doc || !doc->is_valid || !doc->editor || !doc->editor->sci) return;
    ScintillaObject *sci = doc->editor->sci;
    /* Targeted replace keeps the undo stack usable (sci_set_text does not). */
    sci_set_target_start(sci, 0);
    sci_set_target_end(sci, sci_get_length(sci));
    sci_replace_target(sci, content, FALSE);
    document_set_text_changed(doc, TRUE);
}

gchar *lumila_chat_apply_edits(GList *edits)
{
    GString *summary = g_string_new("");

    /* Programmatic buffer changes must not trigger the AI autocomplete
     * (each SC_MOD_INSERTTEXT would schedule a request). */
    lumila_autocomplete_suppress(TRUE);
    for (GList *l = edits; l; l = l->next) {
        LumilaFileEdit *edit = l->data;
        GeanyDocument *target = lumila_chat_doc_for_edit(edit->file_name);
        gchar *resolved = NULL;

        if (target) {
            replace_document_content(target, edit->content);
            g_string_append_printf(summary, "\n\xE2\x9C\x85 Edited: %s", edit->file_name);
        } else if ((resolved = lumila_chat_resolve_edit_path(edit->file_name)) &&
                   g_file_test(resolved, G_FILE_TEST_EXISTS)) {
            /* File exists on disk but is not open: open it, then replace. */
            GeanyDocument *opened = document_open_file(resolved, FALSE, NULL, NULL);
            if (opened && opened->editor && opened->editor->sci) {
                replace_document_content(opened, edit->content);
                g_string_append_printf(summary, "\n\xE2\x9C\x85 Edited: %s", edit->file_name);
            } else {
                g_string_append_printf(summary, "\n\xE2\x9A\xA0 Could not open: %s", edit->file_name);
            }
        } else {
            GeanyFiletype *ft = filetypes_detect_from_file(edit->file_name);
            GeanyDocument *new_doc = document_new_file(
                resolved ? resolved : edit->file_name, ft, edit->content);
            if (new_doc && new_doc->editor && new_doc->editor->sci) {
                g_string_append_printf(summary, "\n\xF0\x9F\x93\x84 Created: %s", edit->file_name);
            }
        }
        g_free(resolved);
    }
    lumila_autocomplete_suppress(FALSE);

    return g_string_free(summary, FALSE);
}

void lumila_chat_append_system(const gchar *text)
{
    append_message_to_view("system", text);
}

gint lumila_chat_get_current_provider_id(void)
{
    return current_provider_id;
}

static gchar *apply_file_edits(const gchar *response)
{
    if (!response) return NULL;

    /* Drop any preview still open from a previous response. */
    lumila_diff_clear_all();

    gchar *cleaned = NULL;
    GList *edits = lumila_utils_parse_file_edits(response, &cleaned);
    if (!edits) {
        g_free(cleaned);
        return NULL;
    }

    if (lumila_config_get_confirm_edits()) {
        if (lumila_config_get_diff_preview()) {
            /* Decorate affected files inline and show a non-modal
             * Apply/Discard dialog; the edits list is owned by the
             * diff module from here on. */
            lumila_diff_preview_begin(edits);
            g_free(cleaned);
            return g_strdup(
                _("\n\xF0\x9F\x94\x8D Diff preview shown in the editor "
                  "(Apply/Discard in the dialog)"));
        }

        GString *listing = g_string_new(
            _("The model wants to apply these file changes:"));
        for (GList *l = edits; l; l = l->next) {
            LumilaFileEdit *e = l->data;
            gint lines = 0;
            for (const gchar *p = e->content; *p; p++) {
                if (*p == '\n') lines++;
            }
            lines++;
            GeanyDocument *existing = lumila_chat_doc_for_edit(e->file_name);
            gchar *resolved = lumila_chat_resolve_edit_path(e->file_name);
            gboolean on_disk = resolved && g_file_test(resolved, G_FILE_TEST_EXISTS);
            g_string_append_printf(listing, "\n  \xE2\x80\xA2 %s (%d lines)%s",
                e->file_name, lines,
                (existing || on_disk) ? "" : "  [new file]");
            g_free(resolved);
        }
        g_string_append(listing, _("\n\nApply changes?"));

        GtkWidget *dialog = gtk_message_dialog_new(
            GTK_WINDOW(geany->main_widgets->window),
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE, "%s", listing->str);
        gtk_dialog_add_buttons(GTK_DIALOG(dialog),
            _("_Apply"), GTK_RESPONSE_YES,
            _("_Discard"), GTK_RESPONSE_NO, NULL);
        gint rc = gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        g_string_free(listing, TRUE);

        if (rc != GTK_RESPONSE_YES) {
            gchar *summary = g_strdup(_("\n\xE2\x9D\x8C Edits discarded by user"));
            lumila_utils_file_edits_free(edits);
            g_free(cleaned);
            return summary;
        }
    }

    gchar *summary = lumila_chat_apply_edits(edits);
    lumila_utils_file_edits_free(edits);
    g_free(cleaned);
    return summary;
}

static void on_response_received(const gchar *response, gpointer user_data)
{
    guint request_id = GPOINTER_TO_UINT(user_data);

    /* Called from libsoup async (main loop already), still guard staleness. */
    if (shutting_down || request_id != request_seq) return;

    is_streaming = FALSE;
    gboolean was_cancelled = request_cancelled;
    request_cancelled = FALSE;

    if (was_cancelled) {
        append_message_to_view("system", "[Cancelled]");
    } else if (response && lumila_utils_is_transient_response(response) &&
               lumila_config_get_auto_retry() && pending_retry &&
               pending_retry->attempts < RETRY_MAX_ATTEMPTS &&
               pending_retry->request_id == request_id) {
        is_streaming = TRUE; /* keep the "one request" guard during the wait */
        schedule_retry();
        lumila_sidebar_set_input_sensitive(FALSE);
        return;
    } else if (response && lumila_utils_is_error_response(response)) {
        append_message_to_view("assistant", response);
        retry_context_clear();
    } else if (response) {
        gchar *cleaned_response = NULL;
        gchar *summary = NULL;

        if (plan_mode) {
            plan_pending = TRUE;
        } else if (!ask_mode) {
            summary = apply_file_edits(response);
            (void)cleaned_response;
        }

        LumilaMessage msg = {
            .role = g_strdup("assistant"),
            .content = g_strdup(response),
            .timestamp = get_current_timestamp()
        };
        g_array_append_val(messages, msg);
        append_message_to_view("assistant", response);

        if (plan_mode && plan_pending) {
            append_message_to_view("system",
                "[Plan ready — use ... menu → \"Execute Plan\" to apply it]");
        }
        if (summary && *summary) {
            append_message_to_view("system", summary);
        }
        g_free(summary);
        retry_context_clear();
    } else {
        append_message_to_view("assistant", "Error: Failed to get response");
        retry_context_clear();
    }

    lumila_sidebar_set_status(NULL);
    lumila_sidebar_set_input_sensitive(TRUE);
    if (current_provider && current_provider->last_usage > 0) {
        lumila_sidebar_set_token_estimate(current_provider->last_usage);
    }
}

gboolean lumila_chat_is_streaming(void)
{
    return is_streaming;
}

void lumila_chat_cancel_request(void)
{
    gboolean was_active = is_streaming;

    request_seq++;
    request_cancelled = TRUE;
    is_streaming = FALSE;
    retry_context_clear();

    if (current_provider) {
        lumila_provider_cancel(current_provider);
    }

    /* The cancelled request's final callback is dropped by the
     * request-id check, so close the stream and mark it here. Skip
     * entirely when nothing was in flight — Esc at rest should be a
     * no-op (stream_end would append a stray newline). */
    if (chat_view && was_active) {
        lumila_chat_ui_stream_end(chat_view);
        append_message_to_view("system", "[Cancelled]");
    }
    lumila_sidebar_set_status(NULL);
    lumila_sidebar_set_input_sensitive(TRUE);
}

static gchar *get_current_file_content(void)
{
    GeanyDocument *doc = document_get_current();
    if (!doc || !doc->editor || !doc->editor->sci) return g_strdup("");
    gchar *content = sci_get_contents(doc->editor->sci, -1);
    gchar *filename = doc->file_name ? g_path_get_basename(doc->file_name) : g_strdup("current");
    gchar *result = g_strdup_printf("[File: %s]\n```\n%s\n```", filename, content ? content : "");
    g_free(content);
    g_free(filename);
    return result;
}

static gchar *get_git_diff(void)
{
    /* Run in the project/document directory, not in Geany's cwd. */
    gchar *cwd = NULL;
    const gchar *base = get_project_base();
    if (base && *base) {
        cwd = g_strdup(base);
    } else {
        GeanyDocument *doc = document_get_current();
        if (doc && doc->file_name) {
            cwd = g_path_get_dirname(doc->file_name);
        }
    }

    gchar *output = NULL;
    gint exit_status = 0;
    g_spawn_sync(cwd, (gchar *[]){"git", "diff", "--cached", NULL}, NULL,
                 G_SPAWN_SEARCH_PATH, NULL, NULL, &output, NULL, &exit_status, NULL);
    if (exit_status != 0 || !output || !*output) {
        g_free(output);
        output = NULL;
        g_spawn_sync(cwd, (gchar *[]){"git", "diff", NULL}, NULL,
                     G_SPAWN_SEARCH_PATH, NULL, NULL, &output, NULL, &exit_status, NULL);
    }
    g_free(cwd);
    if (!output || !*output) {
        g_free(output);
        return g_strdup("No changes detected.");
    }
    /* Cap the diff so a huge commit does not blow the context. */
    if (strlen(output) > 30000) {
        gchar *trunc = g_strndup(output, 30000);
        g_free(output);
        gchar *tmp = g_strconcat(trunc, "\n... (diff truncated)\n", NULL);
        g_free(trunc);
        output = tmp;
    }
    return output;
}

/* Per-file cap for @references. */
#define REF_MAX_CHARS 20000

static gboolean is_sensitive_path(const gchar *path)
{
    const gchar *home = g_get_home_dir();
    if (!home) return FALSE;
    static const gchar *sensitive[] = {
        "/.ssh", "/.gnupg", "/.aws", "/.config/gcloud", "/.kube", NULL
    };
    gchar *abs = g_path_is_absolute(path) ? g_strdup(path)
        : g_build_filename(g_get_current_dir(), path, NULL);
    gboolean hit = FALSE;
    for (gint i = 0; sensitive[i]; i++) {
        gchar *prefix = g_build_filename(home, sensitive[i], NULL);
        if (g_str_has_prefix(abs, prefix)) hit = TRUE;
        g_free(prefix);
        if (hit) break;
    }
    if (!hit) {
        gchar *base = g_path_get_basename(abs);
        if (g_strcmp0(base, ".env") == 0 || g_str_has_prefix(base, ".env.")) {
            hit = TRUE;
        }
        g_free(base);
    }
    g_free(abs);
    return hit;
}

static gchar *extract_references(const gchar *message)
{
    GString *refs = g_string_new("");
    const gchar *p = message;

    while ((p = strchr(p, '@')) != NULL) {
        /* Word boundary: @ must start the string or follow whitespace/(. */
        if (p != message && !g_ascii_isspace(*(p - 1)) &&
            *(p - 1) != '(' && *(p - 1) != '"' && *(p - 1) != '\'') {
            p++;
            continue;
        }
        p++;
        const gchar *start = p;
        while (*p && !g_ascii_isspace(*p) && *p != '\n' && *p != '\r') p++;
        if (p == start) continue;

        gchar *filename = g_strndup(start, p - start);
        /* Trim trailing punctuation often typed after paths */
        while (strlen(filename) > 0 &&
               (filename[strlen(filename) - 1] == '.' ||
                filename[strlen(filename) - 1] == ',' ||
                filename[strlen(filename) - 1] == ':' ||
                filename[strlen(filename) - 1] == ')')) {
            filename[strlen(filename) - 1] = '\0';
        }
        if (!*filename || strchr(filename, '@')) {
            g_free(filename);
            continue;
        }

        /* Expand ~/ and resolve relative to project/doc dir, then cwd. */
        gchar *path = NULL;
        if (g_str_has_prefix(filename, "~/")) {
            path = g_build_filename(g_get_home_dir(), filename + 2, NULL);
        } else if (g_path_is_absolute(filename)) {
            path = g_strdup(filename);
        } else {
            const gchar *base = get_project_base();
            GeanyDocument *doc = document_get_current();
            gchar *cand = NULL;
            if (base && *base) {
                cand = g_build_filename(base, filename, NULL);
                if (g_file_test(cand, G_FILE_TEST_EXISTS)) path = cand;
                else { g_free(cand); cand = NULL; }
            }
            if (!path && doc && doc->file_name) {
                gchar *dir = g_path_get_dirname(doc->file_name);
                cand = g_build_filename(dir, filename, NULL);
                if (g_file_test(cand, G_FILE_TEST_EXISTS)) path = cand;
                else { g_free(cand); cand = NULL; }
                g_free(dir);
            }
        }

        if (!path || !g_file_test(path, G_FILE_TEST_EXISTS)) {
            g_free(path);
            g_free(filename);
            continue;
        }

        if (is_sensitive_path(path)) {
            g_string_append_printf(refs,
                "\n\n[Referenced file skipped (sensitive): %s]", filename);
            g_free(path);
            g_free(filename);
            continue;
        }

        gchar *content = NULL;
        gsize len = 0;
        if (g_file_get_contents(path, &content, &len, NULL) && content && len > 0) {
            /* Skip binary-ish files */
            if (memchr(content, '\0', len > 4096 ? 4096 : len)) {
                g_free(content);
                g_free(path);
                g_free(filename);
                continue;
            }
            if (len > REF_MAX_CHARS) {
                gchar *trunc = g_strndup(content, REF_MAX_CHARS);
                g_free(content);
                content = g_strconcat(trunc, "\n... (truncated)\n", NULL);
                g_free(trunc);
            }
            g_string_append_printf(refs, "\n\n[Referenced file: %s]\n```\n%s\n```",
                                   path, content);
        }
        g_free(content);
        g_free(path);
        g_free(filename);
    }

    return g_string_free(refs, FALSE);
}

/* Best-effort syntax/compile check for /fix: returns captured stderr or
 * NULL when the filetype has no checker or the check passed cleanly. */
static gchar *run_syntax_check(const gchar *path)
{
    const gchar *ext = strrchr(path, '.');
    if (!ext) return NULL;
    ext++;

    gchar *quoted = g_shell_quote(path);
    gchar *full_cmd = NULL;
    if (g_str_equal(ext, "c") || g_str_equal(ext, "h"))
        full_cmd = g_strdup_printf("timeout 15 gcc -fsyntax-only %s 2>&1", quoted);
    else if (g_str_equal(ext, "cpp") || g_str_equal(ext, "cxx") || g_str_equal(ext, "cc") ||
             g_str_equal(ext, "hpp"))
        full_cmd = g_strdup_printf("timeout 15 g++ -fsyntax-only %s 2>&1", quoted);
    else if (g_str_equal(ext, "py"))
        full_cmd = g_strdup_printf("timeout 15 python3 -m py_compile %s 2>&1", quoted);
    else if (g_str_equal(ext, "js"))
        full_cmd = g_strdup_printf("timeout 15 node --check %s 2>&1", quoted);
    else if (g_str_equal(ext, "sh"))
        full_cmd = g_strdup_printf("timeout 15 bash -n %s 2>&1", quoted);
    else if (g_str_equal(ext, "php"))
        full_cmd = g_strdup_printf("timeout 15 php -l %s 2>&1", quoted);
    g_free(quoted);
    if (!full_cmd) return NULL;

    gchar *output = NULL;
    gint exit_status = 0;
    g_spawn_command_line_sync(full_cmd, &output, NULL, &exit_status, NULL);
    g_free(full_cmd);

    if (exit_status == 0 || !output || !*output) {
        g_free(output);
        return NULL;
    }
    return output;
}

static gchar *process_slash_command(const gchar *message, gboolean *embeds_file_ctx)
{
    if (embeds_file_ctx) *embeds_file_ctx = FALSE;
    if (!message || message[0] != '/') return g_strdup(message);

    const gchar *cmd = message + 1;
    while (*cmd == ' ') cmd++;

/* The command name must end at a word boundary (space/EOL), otherwise
 * "/doctor" would match "/doc", "/fixer" would match "/fix"... */
#define SLASH_CMD(name) \
    (g_str_has_prefix(cmd, name) && \
     (cmd[sizeof(name) - 1] == '\0' || cmd[sizeof(name) - 1] == ' ' || \
      cmd[sizeof(name) - 1] == '\n'))

    if (SLASH_CMD("explain")) {
        const gchar *rest = cmd + 7;
        while (*rest == ' ') rest++;
        if (embeds_file_ctx) *embeds_file_ctx = TRUE;
        gchar *file_ctx = get_current_file_content();
        gchar *result = g_strdup_printf("Explain the following code in detail:\n\n%s\n\nSpecific question: %s", file_ctx, rest);
        g_free(file_ctx);
        return result;
    }
    if (SLASH_CMD("refactor")) {
        const gchar *rest = cmd + 8;
        while (*rest == ' ') rest++;
        if (embeds_file_ctx) *embeds_file_ctx = TRUE;
        gchar *file_ctx = get_current_file_content();
        gchar *result = g_strdup_printf("Refactor the following code to improve readability and performance. Keep the same functionality:\n\n%s\n\nSpecific request: %s", file_ctx, rest);
        g_free(file_ctx);
        return result;
    }
    if (SLASH_CMD("test")) {
        const gchar *rest = cmd + 4;
        while (*rest == ' ') rest++;
        if (embeds_file_ctx) *embeds_file_ctx = TRUE;
        gchar *file_ctx = get_current_file_content();
        gchar *result = g_strdup_printf("Write comprehensive unit tests for the following code:\n\n%s\n\nSpecific request: %s", file_ctx, rest);
        g_free(file_ctx);
        return result;
    }
    if (SLASH_CMD("doc")) {
        const gchar *rest = cmd + 3;
        while (*rest == ' ') rest++;
        if (embeds_file_ctx) *embeds_file_ctx = TRUE;
        gchar *file_ctx = get_current_file_content();
        gchar *result = g_strdup_printf("Generate documentation (docstrings/comments) for the following code:\n\n%s\n\nSpecific request: %s", file_ctx, rest);
        g_free(file_ctx);
        return result;
    }
    if (SLASH_CMD("fix")) {
        const gchar *rest = cmd + 3;
        while (*rest == ' ') rest++;
        if (embeds_file_ctx) *embeds_file_ctx = TRUE;
        gchar *file_ctx = get_current_file_content();
        /* Attach real compiler/linter output when we can produce it. */
        gchar *diag = NULL;
        GeanyDocument *doc = document_get_current();
        if (doc && doc->file_name) {
            diag = run_syntax_check(doc->file_name);
        }
        gchar *result;
        if (diag && *diag) {
            result = g_strdup_printf(
                "Fix the following code. These are the current compiler/linter errors:\n\n"
                "```\n%s\n```\n\nCode:\n\n%s\n\nSpecific issue: %s",
                diag, file_ctx, rest);
        } else {
            result = g_strdup_printf(
                "Find and fix any bugs, issues or compilation errors in the following code. "
                "Explain what was wrong and provide the corrected version:\n\n%s\n\nSpecific issue: %s",
                file_ctx, rest);
        }
        g_free(diag);
        g_free(file_ctx);
        return result;
    }
    if (SLASH_CMD("commit")) {
        gchar *diff = get_git_diff();
        gchar *result = g_strdup_printf("Generate a concise, conventional commit message for these changes. Use the format: type(scope): description\n\nChanges:\n```diff\n%s\n```", diff);
        g_free(diff);
        return result;
    }
    if (SLASH_CMD("review")) {
        const gchar *rest = cmd + 6;
        while (*rest == ' ') rest++;
        if (embeds_file_ctx) *embeds_file_ctx = TRUE;
        gchar *file_ctx = get_current_file_content();
        gchar *result = g_strdup_printf("Review the following code for bugs, security issues, performance problems and code quality. Provide specific, actionable suggestions:\n\n%s\n\nFocus areas: %s", file_ctx, rest);
        g_free(file_ctx);
        return result;
    }
#undef SLASH_CMD

    return g_strdup(message);
}

void lumila_chat_set_ask_mode(gboolean enabled)
{
    ask_mode = enabled;
    if (enabled) {
        plan_mode = FALSE;
        plan_pending = FALSE;
        lumila_config_set_plan_mode(FALSE);
    }
    lumila_config_set_ask_mode(enabled);
}

void lumila_chat_set_plan_mode(gboolean enabled)
{
    plan_mode = enabled;
    if (enabled) {
        ask_mode = FALSE;
        lumila_config_set_ask_mode(FALSE);
    }
    lumila_config_set_plan_mode(enabled);
}

gboolean lumila_chat_get_plan_mode(void)
{
    return plan_mode;
}

gboolean lumila_chat_get_plan_pending(void)
{
    return plan_pending;
}

void lumila_chat_execute_plan(void)
{
    if (!plan_mode || !plan_pending || is_streaming) return;
    plan_pending = FALSE;
    /* Re-send with instructions to implement; plan_mode is still on, so
     * build the prompt with the normal system prompt instead. */
    static const gchar *exec_msg =
        "Plan approved. Implement it now, emitting ```file:name blocks for "
        "every file that must be created or modified.";
    lumila_chat_send_message_ex(exec_msg, TRUE, FALSE, TRUE);
}

static gchar *build_system_prompt(gboolean force_code_prompt)
{
    if (plan_mode && !force_code_prompt) {
        return g_strdup(PLAN_PROMPT);
    }
    if (ask_mode) {
        return g_strdup(
            "You are Lumila, an AI coding assistant inside the Geany editor. "
            "The user wants a consultation without file modifications. "
            "DO NOT use ```file: blocks or suggest file edits. "
            "Do not output system metadata, safety labels, or reasoning steps. "
            "Respond directly with explanations, suggestions, and code examples.\n\n"
        );
    }
    return g_strdup(SYSTEM_PROMPT);
}

/* Rough chars/4 estimate shown in the UI. */
static gint64 estimate_request_tokens(const gchar *system_prompt, GArray *msgs)
{
    gsize chars = system_prompt ? strlen(system_prompt) : 0;
    if (msgs) {
        for (guint i = 0; i < msgs->len; i++) {
            LumilaMessage *m = &g_array_index(msgs, LumilaMessage, i);
            if (m->content) chars += strlen(m->content);
        }
    }
    return (gint64)(chars / 4);
}

/* Cap the message history sent to the provider to the configured count,
 * always keeping the last (current user) message. */
static void trim_messages_for_budget(GArray *arr)
{
    gint max = lumila_config_get_history_max_messages();
    while (max > 0 && (gint)arr->len > max) {
        /* Drop oldest after index 0 (which is the first user message) —
         * keep message 0 too? No: drop oldest entirely. */
        LumilaMessage *m = &g_array_index(arr, LumilaMessage, 0);
        g_free(m->role);
        g_free(m->content);
        g_free(m->timestamp);
        g_array_remove_index(arr, 0);
    }
}

static void send_request(const gchar *system_prompt, GArray *messages_to_send)
{
    if (!current_provider) return;

    is_streaming = TRUE;
    lumila_sidebar_set_status(_("Thinking..."));
    lumila_sidebar_set_input_sensitive(FALSE);
    lumila_sidebar_set_token_estimate(
        estimate_request_tokens(system_prompt, messages_to_send));

    if (current_provider->send_message_stream) {
        lumila_chat_ui_stream_start(chat_view);
        lumila_provider_send_message_stream(current_provider, system_prompt,
                                               messages_to_send,
                                               on_stream_chunk, on_stream_final,
                                               GUINT_TO_POINTER(request_seq));
    } else {
        lumila_provider_send_message(current_provider, system_prompt,
                                      messages_to_send, on_response_received,
                                      GUINT_TO_POINTER(request_seq));
    }
}

void lumila_chat_send_message_ex(const gchar *message, gboolean allow_refs,
                                 gboolean skip_active_ctx, gboolean force_code_prompt)
{
    if (!message || !*message) return;
    if (is_streaming) return;  /* one request at a time */

    /* New user message supersedes anything pending. */
    request_seq++;
    request_cancelled = FALSE;
    retry_context_clear();

    // Extract @references (skipped for Send Selection/File, whose content
    // legitimately contains '@').
    gchar *refs = allow_refs ? extract_references(message) : NULL;

    // Process slash commands
    gboolean embeds_file_ctx = FALSE;
    gchar *processed = process_slash_command(message, &embeds_file_ctx);
    gboolean skip_active = skip_active_ctx || embeds_file_ctx;

    // Add user message
    LumilaMessage msg = {
        .role = g_strdup("user"),
        .content = g_strdup(message),
        .timestamp = get_current_timestamp()
    };
    g_array_append_val(messages, msg);
    append_message_to_view("user", message);

    // Extract title from first user message
    if (!conversation_title || !*conversation_title) {
        gchar *first_line = g_strdup(message);
        gchar *newline = strchr(first_line, '\n');
        if (newline) *newline = '\0';
        if (strlen(first_line) > 60) {
            gchar *truncated = g_strndup(first_line, 60);
            g_free(first_line);
            first_line = g_strdup_printf("%s...", truncated);
            g_free(truncated);
        }
        g_free(conversation_title);
        conversation_title = first_line;
    }

    // Get context from open files
    gchar *files_context = get_open_files_context(skip_active);

    // Build system prompt: base prompt + file context + references
    gchar *base_prompt = build_system_prompt(force_code_prompt);
    GString *sys_parts = g_string_new(base_prompt);
    if (files_context && *files_context) {
        g_string_append_printf(sys_parts, "\n%s", files_context);
    }
    if (refs && *refs) {
        g_string_append_printf(sys_parts, "\n%s", refs);
    }
    gchar *system_prompt = g_string_free(sys_parts, FALSE);

    /* Build messages array for multi-turn conversation.
     * Copy conversation history, replacing the last user message content
     * with the processed version (slash commands resolved). */
    GArray *messages_to_send = g_array_new(FALSE, FALSE, sizeof(LumilaMessage));
    for (guint i = 0; i < messages->len; i++) {
        LumilaMessage *m = &g_array_index(messages, LumilaMessage, i);
        LumilaMessage copy;
        if (i == messages->len - 1 && g_str_equal(m->role, "user")) {
            copy.role = g_strdup(m->role);
            copy.content = g_strdup(processed);
            copy.timestamp = g_strdup(m->timestamp);
        } else {
            copy.role = g_strdup(m->role);
            copy.content = g_strdup(m->content);
            copy.timestamp = g_strdup(m->timestamp);
        }
        g_array_append_val(messages_to_send, copy);
    }
    trim_messages_for_budget(messages_to_send);

    /* Keep the payload for possible retries (ownership transferred). */
    retry_context_take(system_prompt, messages_to_send);

    send_request(system_prompt, messages_to_send);

    g_free(system_prompt);
    g_free(base_prompt);
    g_free(processed);
    g_free(refs);
    g_free(files_context);
}

void lumila_chat_send_message(const gchar *message)
{
    lumila_chat_send_message_ex(message, TRUE, FALSE, FALSE);
}

void lumila_chat_send_selection(void)
{
    if (is_streaming) return;

    GeanyDocument *doc = document_get_current();
    if (!doc || !doc->editor || !doc->editor->sci) {
        append_message_to_view("system", "Error: No active document");
        return;
    }

    ScintillaObject *sci = doc->editor->sci;
    gint start = sci_get_selection_start(sci);
    gint end = sci_get_selection_end(sci);

    if (start >= end) {
        append_message_to_view("system", "Error: No text selected");
        return;
    }

    gchar *selection = sci_get_contents_range(sci, start, end);
    if (!selection || !*selection) {
        g_free(selection);
        append_message_to_view("system", "Error: Selection is empty");
        return;
    }

    gchar *filename = g_path_get_basename(doc->file_name ? doc->file_name : "untitled");
    gchar *message = g_strdup_printf("[Selection from %s]\n```\n%s\n```\n", filename, selection);

    lumila_chat_send_message_ex(message, FALSE, TRUE, FALSE);

    g_free(message);
    g_free(selection);
    g_free(filename);
}

void lumila_chat_send_current_file(void)
{
    if (is_streaming) return;

    GeanyDocument *doc = document_get_current();
    if (!doc || !doc->editor || !doc->editor->sci) {
        append_message_to_view("system", "Error: No active document");
        return;
    }

    gchar *content = sci_get_contents(doc->editor->sci, -1);
    if (!content || !*content) {
        g_free(content);
        append_message_to_view("system", "Error: File is empty");
        return;
    }

    gchar *filename = g_path_get_basename(doc->file_name ? doc->file_name : "untitled");
    gchar *message = g_strdup_printf("[Full content of %s]\n```\n%s\n```\n", filename, content);

    lumila_chat_send_message_ex(message, FALSE, TRUE, FALSE);

    g_free(message);
    g_free(filename);
    g_free(content);
}

void lumila_chat_export_markdown(void)
{
    if (!messages || messages->len == 0) {
        append_message_to_view("system", "Error: No messages to export");
        return;
    }

    gchar *export_dir = g_build_filename(history_dir, "..", "exports", NULL);
    g_mkdir_with_parents(export_dir, 0755);

    GDateTime *now = g_date_time_new_now_local();
    gchar *timestamp = g_date_time_format(now, "%Y%m%d-%H%M%S");
    gchar *filename = g_strdup_printf("conversation-%s.md", timestamp);
    gchar *filepath = g_build_filename(export_dir, filename, NULL);
    g_free(timestamp);
    g_date_time_unref(now);

    GString *md = g_string_new("# Lumila AI Conversation\n\n");

    for (guint i = 0; i < messages->len; i++) {
        LumilaMessage *msg = &g_array_index(messages, LumilaMessage, i);
        const gchar *role_label = g_str_equal(msg->role, "user") ? "User" : "Assistant";
        g_string_append_printf(md, "## %s (%s)\n\n", role_label, msg->timestamp);
        g_string_append_printf(md, "%s\n\n---\n\n", msg->content);
    }

    GError *error = NULL;
    if (!g_file_set_contents(filepath, md->str, -1, &error)) {
        gchar *err = g_strdup_printf("Error exporting: %s", error->message);
        append_message_to_view("system", err);
        g_free(err);
        g_error_free(error);
    } else {
        gchar *info = g_strdup_printf("Exported to: %s", filepath);
        append_message_to_view("system", info);
        g_free(info);
    }

    g_string_free(md, TRUE);
    g_free(filepath);
    g_free(filename);
    g_free(export_dir);
}

static gboolean deferred_provider_free(gpointer user_data)
{
    LumilaProvider *old = (LumilaProvider *)user_data;
    lumila_provider_free(old);
    return G_SOURCE_REMOVE;
}

void lumila_chat_set_provider(gint provider_id)
{
    /* The sidebar combo fires "changed" when it is populated with the
     * already-selected default; skip the redundant provider rebuild. */
    if (provider_id == current_provider_id && current_provider) {
        return;
    }
    current_provider_id = provider_id;
    request_seq++;
    retry_context_clear();

    /* Switching models mid-request: the old request's callbacks are
     * dropped by the request-id check, so restore the UI here —
     * otherwise the input stays disabled forever. */
    if (is_streaming) {
        is_streaming = FALSE;
        request_cancelled = TRUE;
        if (chat_view) {
            lumila_chat_ui_stream_end(chat_view);
            append_message_to_view("system", "[Cancelled — model changed]");
        }
        lumila_sidebar_set_status(NULL);
        lumila_sidebar_set_input_sensitive(TRUE);
    }

    if (current_provider) {
        /* Cancel and defer free so pending async callbacks can fire safely */
        lumila_provider_cancel(current_provider);
        g_idle_add(deferred_provider_free, current_provider);
    }

    const LumilaModelEntry *entry = lumila_model_registry_lookup(provider_id);

    current_provider = lumila_provider_create(entry->type);
    current_provider->model_id = entry->provider_id;
    current_provider->model_name = entry->model_name;
    current_provider->model_flags = entry->flags;

    /* Persist the selection so it survives restarts. */
    lumila_config_set_default_model(entry->model_name);
}

void lumila_chat_save_history(void)
{
    if (!messages || messages->len == 0 || !history_dir || !conversation_id) return;

    // Create JSON structure
    json_t *root = json_object();
    json_object_set_new(root, "id", json_string(conversation_id));
    json_object_set_new(root, "created_at",
        json_string(conversation_created_at ? conversation_created_at : ""));
    json_object_set_new(root, "provider_id", json_integer(current_provider_id));
    const LumilaModelEntry *entry = lumila_model_registry_lookup(current_provider_id);
    json_object_set_new(root, "model_name", json_string(entry->model_name));
    json_object_set_new(root, "title", json_string(conversation_title ? conversation_title : ""));

    // Add messages array
    json_t *msgs_array = json_array();
    for (guint i = 0; i < messages->len; i++) {
        LumilaMessage *msg = &g_array_index(messages, LumilaMessage, i);
        json_t *msg_obj = json_object();
        json_object_set_new(msg_obj, "role", json_string(msg->role));
        json_object_set_new(msg_obj, "content", json_string(msg->content));
        json_object_set_new(msg_obj, "timestamp", json_string(msg->timestamp ? msg->timestamp : ""));
        json_array_append_new(msgs_array, msg_obj);
    }
    json_object_set_new(root, "messages", msgs_array);

    // Save to file
    gchar *filename = g_strdup_printf("%s.json", conversation_id);
    gchar *filepath = g_build_filename(history_dir, filename, NULL);
    if (json_dump_file(root, filepath, JSON_INDENT(2)) != 0) {
        g_warning("Lumila: failed to save history to %s", filepath);
    }

    g_free(filename);
    g_free(filepath);
    json_decref(root);
}

void lumila_chat_new_conversation(void)
{
    /* Invalidate and cancel any in-flight request so its chunks cannot
     * leak into the fresh conversation. */
    request_seq++;
    request_cancelled = FALSE;
    is_streaming = FALSE;
    retry_context_clear();
    if (current_provider) {
        lumila_provider_cancel(current_provider);
    }

    // Save current conversation
    lumila_chat_save_history();

    /* A pending diff preview references the old response — drop it. */
    lumila_diff_clear_all();

    // Clear messages
    if (messages) {
        for (guint i = 0; i < messages->len; i++) {
            LumilaMessage *msg = &g_array_index(messages, LumilaMessage, i);
            g_free(msg->role);
            g_free(msg->content);
            g_free(msg->timestamp);
        }
        g_array_set_size(messages, 0);
    }

    // Clear UI
    if (chat_view) {
        lumila_chat_ui_clear(chat_view);
    }

    // Generate new conversation ID
    g_free(conversation_id);
    g_free(conversation_title);
    conversation_title = NULL;
    g_free(conversation_created_at);
    conversation_created_at = get_current_timestamp();
    plan_pending = FALSE;
    static guint conv_counter = 0;
    GDateTime *now = g_date_time_new_now_local();
    gchar *ts = g_date_time_format(now, "%Y%m%d-%H%M%S");
    conversation_id = g_strdup_printf("conversation-%s-%u", ts, conv_counter++);
    g_free(ts);
    g_date_time_unref(now);

    lumila_sidebar_set_input_sensitive(TRUE);
}

void lumila_chat_set_title(const gchar *title)
{
    g_free(conversation_title);
    conversation_title = g_strdup(title);
}

void lumila_chat_load_conversation(const gchar *filename)
{
    if (!filename) return;

    /* Invalidate, cancel any in-flight request and pending retries. */
    request_seq++;
    request_cancelled = TRUE;
    retry_context_clear();
    plan_pending = FALSE;
    is_streaming = FALSE;
    if (current_provider) {
        lumila_provider_cancel(current_provider);
    }
    lumila_sidebar_set_status(NULL);
    lumila_sidebar_set_input_sensitive(TRUE);

    // Save current conversation first
    lumila_chat_save_history();

    // Clear current messages and UI
    if (messages) {
        for (guint i = 0; i < messages->len; i++) {
            LumilaMessage *msg = &g_array_index(messages, LumilaMessage, i);
            g_free(msg->role);
            g_free(msg->content);
            g_free(msg->timestamp);
        }
        g_array_set_size(messages, 0);
    }
    if (chat_view) {
        lumila_chat_ui_clear(chat_view);
    }

    // Load from history file
    GArray *loaded = lumila_history_load_messages(filename);
    if (!loaded) return;

    for (guint i = 0; i < loaded->len; i++) {
        LumilaMessage *src = &g_array_index(loaded, LumilaMessage, i);
        LumilaMessage msg = {
            .role = g_strdup(src->role),
            .content = g_strdup(src->content),
            .timestamp = g_strdup(src->timestamp)
        };
        g_array_append_val(messages, msg);
        append_message_to_view(msg.role, msg.content);
    }

    lumila_history_free_messages(loaded);

    // Update conversation ID from filename
    g_free(conversation_id);
    gsize flen = strlen(filename);
    if (flen > 5 && g_str_has_suffix(filename, ".json")) {
        conversation_id = g_strndup(filename, flen - 5);
    } else {
        conversation_id = g_strdup(filename);
    }

    // Restore title and created_at from history
    g_free(conversation_title);
    conversation_title = lumila_history_load_title(filename);
    g_free(conversation_created_at);
    conversation_created_at = lumila_history_load_created_at(filename);
}
