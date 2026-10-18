#include "autocomplete.h"
#include "chat.h"
#include "config.h"
#include "utils.h"
#include "plugin.h"
#include "providers/provider.h"
#include "providers/model_registry.h"
#include <geanyplugin.h>
#include <string.h>

#define AC_DEBOUNCE_MS  600
#define AC_MAX_PREFIX   4000
#define AC_MAX_SUFFIX   1000
#define AC_MAX_LINES    8

/* EOL/standard annotation style index (rendered as 512 + index). */
#define ANN_STYLE_GHOST 2
#define COLOR_GHOST_FG  0x909090   /* gray (0xBBGGRR) */

#define AC_SYSTEM_PROMPT \
    "You are a code completion engine inside a code editor. " \
    "Given the code around <CURSOR>, output ONLY the code that should " \
    "be inserted at <CURSOR>. Rules: raw code only, no markdown fences, " \
    "no explanation, no repeating the surrounding code, at most 8 " \
    "lines. If unsure, output an empty line.\n\n"

static LumilaProvider *ac_provider = NULL;
static gint ac_provider_id = -1;
static guint ac_seq = 0;
static guint debounce_source = 0;
static gint suppress_depth = 0;
static gboolean shutting_down = FALSE;

static GeanyDocument *pending_doc = NULL;
static gint pending_line = -1;
static gchar *pending_text = NULL;
static gboolean ghost_visible = FALSE;
static gboolean ghost_used_ann = FALSE;

/* Staleness data captured when a request is dispatched. */
static GeanyDocument *req_doc = NULL;
static gint req_line = -1;

static void clear_ghost(void)
{
    if (!ghost_visible) return;
    GeanyDocument *doc = pending_doc;
    if (DOC_VALID(doc) && doc->editor && doc->editor->sci) {
        ScintillaObject *sci = doc->editor->sci;
        scintilla_send_message(sci, SCI_EOLANNOTATIONSETTEXT,
                               pending_line, 0);
        /* Only clear the standard annotation when the ghost set one —
         * a diff-preview annotation on the same line is not ours. */
        if (ghost_used_ann) {
            scintilla_send_message(sci, SCI_ANNOTATIONSETTEXT,
                                   pending_line, 0);
        }
    }
    ghost_visible = FALSE;
    ghost_used_ann = FALSE;
    pending_doc = NULL;
    pending_line = -1;
    g_free(pending_text);
    pending_text = NULL;
}

static void cancel_debounce(void)
{
    if (debounce_source) {
        g_source_remove(debounce_source);
        debounce_source = 0;
    }
}

static gint resolve_ac_provider_id(void)
{
    const gchar *name = lumila_config_get_autocomplete_model();
    if (name && *name) {
        gint idx = lumila_model_registry_find_by_model_name(name);
        if (idx >= 0) return idx;
    }
    return lumila_chat_get_current_provider_id();
}

static gboolean deferred_ac_free(gpointer data)
{
    lumila_provider_free(data);
    return G_SOURCE_REMOVE;
}

/* Lazy dedicated provider: lumila_provider_send_message() resets the
 * shared cancellable/pending_msg, so it cannot reuse the chat instance
 * without risking a race. */
static LumilaProvider *autocomplete_provider(void)
{
    gint id = resolve_ac_provider_id();
    if (id < 0) return NULL;

    if (ac_provider && id == ac_provider_id) {
        return ac_provider;
    }
    if (ac_provider) {
        lumila_provider_cancel(ac_provider);
        g_idle_add(deferred_ac_free, ac_provider);
        ac_provider = NULL;
    }

    const LumilaModelEntry *entry = lumila_model_registry_lookup(id);
    ac_provider = lumila_provider_create(entry->type);
    ac_provider->model_id = entry->provider_id;
    ac_provider->model_name = entry->model_name;
    ac_provider->model_flags = entry->flags;
    ac_provider_id = id;
    return ac_provider;
}

static void on_ac_response(const gchar *response, gpointer user_data);

static void request_completion(void)
{
    if (!lumila_config_get_autocomplete() || lumila_chat_is_streaming()) {
        return;
    }
    GeanyDocument *doc = document_get_current();
    if (!DOC_VALID(doc) || doc->readonly ||
        !doc->editor || !doc->editor->sci) {
        return;
    }
    ScintillaObject *sci = doc->editor->sci;
    if (sci_has_selection(sci)) return;
    /* Don't fight Geany's own symbol autocomplete popup. */
    if (scintilla_send_message(sci, SCI_AUTOCACTIVE, 0, 0)) return;

    LumilaProvider *provider = autocomplete_provider();
    if (!provider) return;
    /* Ollama needs no key; everyone else silently skips when unset. */
    if (provider->type != LUMILA_PROVIDER_OLLAMA &&
        !lumila_config_get_api_key(provider->type)) {
        return;
    }

    const gint pos = sci_get_current_position(sci);
    const gint doc_len = sci_get_length(sci);
    const gint start = MAX(0, pos - AC_MAX_PREFIX);
    const gint end = MIN(doc_len, pos + AC_MAX_SUFFIX);
    gchar *prefix = sci_get_contents_range(sci, start, pos);
    gchar *suffix = sci_get_contents_range(sci, pos, end);

    /* Trivial prefix (whitespace-only file start) → not worth a call.
     * Non-destructive scan: g_strstrip would truncate the context the
     * model sees. */
    gboolean has_text = FALSE;
    for (const gchar *p = prefix; p && *p && !has_text; p++) {
        if (!g_ascii_isspace(*p)) has_text = TRUE;
    }
    if (!has_text) {
        g_free(prefix);
        g_free(suffix);
        return;
    }

    const gchar *fname = doc->file_name
        ? g_path_get_basename(doc->file_name) : "untitled";
    const gchar *ftype = doc->file_type ? doc->file_type->name : "text";
    gchar *user_msg = g_strdup_printf(
        "File: %s (%s)\n%s\n<CURSOR>\n%s", fname, ftype, prefix, suffix);
    g_free(prefix);
    g_free(suffix);

    LumilaMessage msg = {
        .role = (gchar *)"user",
        .content = user_msg,
        .timestamp = NULL
    };
    GArray *msgs = g_array_new(FALSE, FALSE, sizeof(LumilaMessage));
    g_array_append_val(msgs, msg);

    req_doc = doc;
    req_line = sci_get_line_from_position(sci, pos);
    lumila_provider_send_message(provider, AC_SYSTEM_PROMPT, msgs,
                                 on_ac_response, GUINT_TO_POINTER(ac_seq));
    g_array_free(msgs, TRUE);
    g_free(user_msg);
}

static gboolean on_debounce(gpointer data)
{
    if (GPOINTER_TO_UINT(data) != ac_seq || shutting_down) {
        return G_SOURCE_REMOVE;
    }
    debounce_source = 0;
    request_completion();
    return G_SOURCE_REMOVE;
}

static gboolean on_editor_notify(GObject *obj, GeanyEditor *editor,
                                 SCNotification *nt, gpointer user_data)
{
    (void)obj;
    (void)user_data;
    if (shutting_down || suppress_depth > 0) return FALSE;
    if (!lumila_config_get_autocomplete()) return FALSE;
    if (!editor || !editor->document || !DOC_VALID(editor->document)) {
        return FALSE;
    }

    /* A ghost left in another document (tab switch) is stale. */
    if (ghost_visible && pending_doc != editor->document) {
        clear_ghost();
    }

    switch (nt->nmhdr.code) {
        case SCN_CHARADDED:
            /* The char just typed replaces the ghost: clear and
             * reschedule. */
            clear_ghost();
            cancel_debounce();
            ac_seq++;
            debounce_source = g_timeout_add(AC_DEBOUNCE_MS, on_debounce,
                                            GUINT_TO_POINTER(ac_seq));
            break;
        case SCN_MODIFIED:
            if (nt->modificationType & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT)) {
                clear_ghost();
                cancel_debounce();
                ac_seq++;
                debounce_source = g_timeout_add(AC_DEBOUNCE_MS,
                                                on_debounce,
                                                GUINT_TO_POINTER(ac_seq));
            }
            break;
        case SCN_UPDATEUI:
            /* Caret/selection moved away from the ghost line → dismiss.
             * (The debounce itself survives: typing moves the caret too,
             * and the response path re-checks position staleness.) */
            if ((nt->updated & SC_UPDATE_SELECTION) && ghost_visible &&
                pending_doc == editor->document) {
                ScintillaObject *sci = editor->document->editor->sci;
                gint line = sci_get_line_from_position(
                    sci, sci_get_current_position(sci));
                if (line != pending_line) {
                    clear_ghost();
                    ac_seq++;
                }
            }
            break;
        default:
            break;
    }
    return FALSE;
}

/* Strip fences the model may emit despite instructions and cap the
 * suggestion at AC_MAX_LINES lines. Returns NULL for nothing usable. */
static gchar *sanitize_completion(const gchar *response)
{
    if (!response) return NULL;
    gchar *s = g_strdup(response);
    g_strstrip(s);

    if (g_str_has_prefix(s, "```")) {
        gchar *nl = strchr(s, '\n');
        gchar *end = nl ? strstr(nl + 1, "```") : NULL;
        if (nl && end) {
            *end = '\0';
            gchar *inner = g_strdup(nl + 1);
            g_free(s);
            s = inner;
            g_strstrip(s);
        }
    }

    /* Keep at most AC_MAX_LINES lines. */
    gchar *cut = s;
    for (gint i = 0; i < AC_MAX_LINES && cut; i++) {
        cut = strchr(cut, '\n');
        if (cut) cut++;
    }
    if (cut) *cut = '\0';

    if (!*s) {
        g_free(s);
        return NULL;
    }
    return s;
}

static void show_ghost(GeanyDocument *doc, gint line, const gchar *text)
{
    ScintillaObject *sci = doc->editor->sci;
    clear_ghost();

    g_free(pending_text);
    pending_text = g_strdup(text);
    pending_doc = doc;
    pending_line = line;
    ghost_visible = TRUE;

    scintilla_send_message(sci, SCI_STYLESETFORE,
                           512 + ANN_STYLE_GHOST, COLOR_GHOST_FG);
    scintilla_send_message(sci, SCI_STYLESETITALIC,
                           512 + ANN_STYLE_GHOST, TRUE);
    scintilla_send_message(sci, SCI_EOLANNOTATIONSETVISIBLE,
                           EOLANNOTATION_STANDARD, 0);

    gchar *first = g_strdup(text);
    gchar *rest = strchr(first, '\n');
    if (rest) {
        *rest = '\0';
        rest++;
    }
    scintilla_send_message(sci, SCI_EOLANNOTATIONSETTEXT,
                           line, (sptr_t)first);
    scintilla_send_message(sci, SCI_EOLANNOTATIONSETSTYLE,
                           line, ANN_STYLE_GHOST);
    if (rest && *rest) {
        /* Remaining lines as an indented annotation under the line. */
        scintilla_send_message(sci, SCI_ANNOTATIONSETVISIBLE,
                               ANNOTATION_INDENTED, 0);
        scintilla_send_message(sci, SCI_ANNOTATIONSETTEXT,
                               line, (sptr_t)rest);
        scintilla_send_message(sci, SCI_ANNOTATIONSETSTYLE,
                               line, ANN_STYLE_GHOST);
        ghost_used_ann = TRUE;
    }
    g_free(first);
}

static void on_ac_response(const gchar *response, gpointer user_data)
{
    if (shutting_down || GPOINTER_TO_UINT(user_data) != ac_seq) return;
    if (!response || lumila_utils_is_error_response(response)) return;

    GeanyDocument *doc = document_get_current();
    if (doc != req_doc || !DOC_VALID(doc) ||
        !doc->editor || !doc->editor->sci) {
        return;
    }
    ScintillaObject *sci = doc->editor->sci;
    const gint line = sci_get_line_from_position(
        sci, sci_get_current_position(sci));
    if (line != req_line) return;

    gchar *sug = sanitize_completion(response);
    if (!sug) return;
    show_ghost(doc, line, sug);
    g_free(sug);
}

void lumila_autocomplete_accept(void)
{
    if (!ghost_visible || !pending_doc || !pending_text) return;
    GeanyDocument *doc = document_get_current();
    if (doc != pending_doc || !DOC_VALID(doc) ||
        !doc->editor || !doc->editor->sci) {
        clear_ghost();
        return;
    }
    ScintillaObject *sci = doc->editor->sci;
    const gint pos = sci_get_current_position(sci);

    suppress_depth++;
    sci_start_undo_action(sci);
    scintilla_send_message(sci, SCI_INSERTTEXT, pos, (sptr_t)pending_text);
    scintilla_send_message(sci, SCI_SETEMPTYSELECTION,
                           pos + (gint)strlen(pending_text), 0);
    sci_end_undo_action(sci);
    suppress_depth--;

    clear_ghost();
    ac_seq++;
}

void lumila_autocomplete_suppress(gboolean on)
{
    suppress_depth += on ? 1 : -1;
    if (suppress_depth < 0) suppress_depth = 0;
}

gboolean lumila_autocomplete_has_pending(void)
{
    return ghost_visible;
}

void lumila_autocomplete_init(void)
{
    shutting_down = FALSE;
    suppress_depth = 0;
    ac_seq++;
    plugin_signal_connect(geany_plugin, NULL, "editor-notify", FALSE,
                          G_CALLBACK(on_editor_notify), NULL);
}

void lumila_autocomplete_cleanup(void)
{
    shutting_down = TRUE;
    ac_seq++;
    clear_ghost();
    cancel_debounce();
    if (ac_provider) {
        lumila_provider_cancel(ac_provider);
        g_idle_add(deferred_ac_free, ac_provider);
        ac_provider = NULL;
    }
    ac_provider_id = -1;
    req_doc = NULL;
    req_line = -1;
}
