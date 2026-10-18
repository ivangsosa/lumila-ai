#include "diff.h"
#include "chat.h"
#include "utils.h"
#include "plugin.h"
#include <geanyplugin.h>
#include <string.h>

/* Marker index for added/changed lines. Geany uses low markers (0-7)
 * and folding occupies 25-31, so 20 is a safe slot. */
#define LUMILA_MARKER_INS 20

/* Annotation style indexes (rendered as style offset 512 + index). */
#define ANN_STYLE_DEL 0

/* Scintilla colors are 0xBBGGRR. */
#define COLOR_DEL_FG  0x6B6BFF   /* soft red  #FF6B6B */
#define COLOR_INS_BG  0x578B2E   /* green     #2E8B57 */

/* Per-document decoration state. */
typedef struct {
    GeanyDocument *doc;
    /* gint: lines that carry a deletion annotation (cleared per line —
     * SCI_ANNOTATIONCLEARALL would also wipe ghost-text annotations). */
    GArray *annotated_lines;
} DocDeco;

static GList *pending_edits = NULL;   /* owned GList of LumilaFileEdit */
static GList *preview_docs = NULL;    /* owned DocDeco* */
static GList *docs_we_opened = NULL;  /* GeanyDocument* (borrowed) */
static GtkWidget *preview_dialog = NULL;
static gboolean applying = FALSE;

static DocDeco *find_deco(GeanyDocument *doc)
{
    for (GList *l = preview_docs; l; l = l->next) {
        DocDeco *deco = l->data;
        if (deco->doc == doc) return deco;
    }
    return NULL;
}

static void clear_doc_decorations(DocDeco *deco)
{
    GeanyDocument *doc = deco->doc;
    if (!DOC_VALID(doc) || !doc->editor || !doc->editor->sci) return;
    ScintillaObject *sci = doc->editor->sci;
    scintilla_send_message(sci, SCI_MARKERDELETEALL, LUMILA_MARKER_INS, 0);
    if (deco->annotated_lines) {
        for (guint i = 0; i < deco->annotated_lines->len; i++) {
            gint line = g_array_index(deco->annotated_lines, gint, i);
            scintilla_send_message(sci, SCI_ANNOTATIONSETTEXT, line, 0);
        }
    }
}

static void deco_free(DocDeco *deco)
{
    if (!deco) return;
    clear_doc_decorations(deco);
    if (deco->annotated_lines) {
        g_array_free(deco->annotated_lines, TRUE);
    }
    g_free(deco);
}

/* Drop one document from the preview: clear its decorations and discard
 * its matching pending edit (the user edited the buffer mid-preview). */
static void drop_doc_preview(GeanyDocument *doc)
{
    DocDeco *deco = find_deco(doc);
    if (deco) {
        preview_docs = g_list_remove(preview_docs, deco);
        deco_free(deco);
    }
    docs_we_opened = g_list_remove(docs_we_opened, doc);

    for (GList *l = pending_edits; l; l = l->next) {
        LumilaFileEdit *e = l->data;
        if (lumila_chat_doc_for_edit(e->file_name) == doc) {
            pending_edits = g_list_remove(pending_edits, e);
            g_free(e->file_name);
            g_free(e->content);
            g_free(e);
            break;
        }
    }

    /* Nothing left to decide: close the dialog as if discarded. */
    if (!pending_edits && preview_dialog) {
        gtk_widget_destroy(preview_dialog);
        preview_dialog = NULL;
    }
}

/* Guard: edits made by the user inside a decorated document drop that
 * file out of the preview (applying it would clobber their change). */
static gboolean on_diff_editor_notify(GObject *obj, GeanyEditor *editor,
                                      SCNotification *nt, gpointer user_data)
{
    (void)obj;
    (void)user_data;
    if (applying || nt->nmhdr.code != SCN_MODIFIED) return FALSE;
    if (!(nt->modificationType & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT))) {
        return FALSE;
    }
    if (!editor || !editor->document) return FALSE;
    if (find_deco(editor->document)) {
        drop_doc_preview(editor->document);
    }
    return FALSE;
}

static void decorate_doc(GeanyDocument *doc, const gchar *new_content)
{
    if (!DOC_VALID(doc) || !doc->editor || !doc->editor->sci) return;
    ScintillaObject *sci = doc->editor->sci;

    gchar *old_content = sci_get_contents(sci, -1);
    GArray *ops = lumila_utils_diff_lines(old_content, new_content);
    g_free(old_content);
    if (!ops || ops->len == 0) {
        lumila_utils_diff_ops_free(ops);
        return;
    }

    DocDeco *deco = find_deco(doc);
    if (!deco) {
        deco = g_new0(DocDeco, 1);
        deco->doc = doc;
        deco->annotated_lines = g_array_new(FALSE, FALSE, sizeof(gint));
        preview_docs = g_list_append(preview_docs, deco);
    }

    scintilla_send_message(sci, SCI_STYLESETFORE,
                           512 + ANN_STYLE_DEL, COLOR_DEL_FG);
    scintilla_send_message(sci, SCI_MARKERDEFINE,
                           LUMILA_MARKER_INS, SC_MARK_BACKGROUND);
    scintilla_send_message(sci, SCI_MARKERSETBACK,
                           LUMILA_MARKER_INS, COLOR_INS_BG);
    scintilla_send_message(sci, SCI_MARKERSETFORE,
                           LUMILA_MARKER_INS, COLOR_INS_BG);
    scintilla_send_message(sci, SCI_ANNOTATIONSETVISIBLE,
                           ANNOTATION_BOXED, 0);

    /* Deleted lines are shown as red annotation text attached under the
     * previous surviving line (or line 0 when the deletion is at the top
     * of the file). */
    GString *del_text = NULL;
    gint last_new_line = 0;

    for (guint i = 0; i < ops->len; i++) {
        LumilaDiffOp *op = &g_array_index(ops, LumilaDiffOp, i);
        if (op->kind == LUMILA_DIFF_DEL) {
            if (!del_text) del_text = g_string_new("");
            if (del_text->len > 0) g_string_append_c(del_text, '\n');
            g_string_append_printf(del_text, "- %s", op->text);
        } else {
            if (del_text) {
                scintilla_send_message(sci, SCI_ANNOTATIONSETTEXT,
                                       last_new_line, (sptr_t)del_text->str);
                scintilla_send_message(sci, SCI_ANNOTATIONSETSTYLE,
                                       last_new_line, ANN_STYLE_DEL);
                g_array_append_val(deco->annotated_lines, last_new_line);
                g_string_free(del_text, TRUE);
                del_text = NULL;
            }
            if (op->kind == LUMILA_DIFF_INS) {
                /* The marker line indexes the NEW content; the current
                 * buffer may be shorter (e.g. a nearly empty file being
                 * filled). Clamp so we never set a marker past EOF. */
                gint max_line = sci_get_line_count(sci) - 1;
                if (op->new_line <= max_line) {
                    sci_set_marker_at_line(sci, op->new_line,
                                           LUMILA_MARKER_INS);
                }
            }
            last_new_line = MIN(op->new_line, sci_get_line_count(sci) - 1);
        }
    }
    /* Deletion run at EOF: attach to the previous line. */
    if (del_text) {
        scintilla_send_message(sci, SCI_ANNOTATIONSETTEXT,
                               last_new_line, (sptr_t)del_text->str);
        scintilla_send_message(sci, SCI_ANNOTATIONSETSTYLE,
                               last_new_line, ANN_STYLE_DEL);
        g_array_append_val(deco->annotated_lines, last_new_line);
        g_string_free(del_text, TRUE);
    }

    lumila_utils_diff_ops_free(ops);
}

static void close_docs_we_opened(void)
{
    for (GList *l = docs_we_opened; l; l = l->next) {
        GeanyDocument *doc = l->data;
        /* Unmodified docs close without prompting; ones the user typed
         * into are already removed by the edit guard. */
        if (DOC_VALID(doc)) {
            document_close(doc);
        }
    }
}

static void clear_decorations(void)
{
    g_list_free_full(preview_docs, (GDestroyNotify)deco_free);
    preview_docs = NULL;
}

static void free_state(void)
{
    clear_decorations();
    if (pending_edits) {
        lumila_utils_file_edits_free(pending_edits);
        pending_edits = NULL;
    }
    g_list_free(docs_we_opened);
    docs_we_opened = NULL;
}

void lumila_diff_clear_all(void)
{
    if (preview_dialog) {
        gtk_widget_destroy(preview_dialog);
        preview_dialog = NULL;
    }
    free_state();
}

gboolean lumila_diff_has_preview(void)
{
    return pending_edits != NULL || preview_docs != NULL;
}

static void finish_preview(gboolean apply)
{
    if (apply && pending_edits) {
        /* Clear decorations BEFORE replacing the buffer, while the
         * annotation line indexes still match the old content. */
        clear_decorations();
        applying = TRUE;
        gchar *summary = lumila_chat_apply_edits(pending_edits);
        applying = FALSE;
        if (summary && *summary) {
            lumila_chat_append_system(summary);
        }
        g_free(summary);
        /* Applied docs stay open (they now carry changes). */
    } else {
        close_docs_we_opened();
        lumila_chat_append_system(_("\n\xE2\x9D\x8C Edits discarded by user"));
    }
    free_state();
}

static void on_preview_response(GtkDialog *dialog, gint response,
                                gpointer user_data)
{
    (void)user_data;
    gtk_widget_destroy(GTK_WIDGET(dialog));
    preview_dialog = NULL;
    /* GTK_RESPONSE_DELETE_EVENT (window close) lands here too = Discard. */
    finish_preview(response == GTK_RESPONSE_YES);
}

void lumila_diff_preview_begin(GList *edits)
{
    /* Auto-discard anything still pending from a previous response. */
    lumila_diff_clear_all();

    pending_edits = edits;

    GString *listing = g_string_new(
        _("The model wants to apply these file changes:"));

    for (GList *l = edits; l; l = l->next) {
        LumilaFileEdit *e = l->data;
        gint lines = 0;
        for (const gchar *p = e->content; *p; p++) {
            if (*p == '\n') lines++;
        }
        lines++;

        GeanyDocument *doc = lumila_chat_doc_for_edit(e->file_name);
        gboolean opened_for_preview = FALSE;
        gboolean on_disk = FALSE;
        if (!doc) {
            gchar *resolved = lumila_chat_resolve_edit_path(e->file_name);
            on_disk = resolved && g_file_test(resolved, G_FILE_TEST_EXISTS);
            if (on_disk) {
                doc = document_open_file(resolved, FALSE, NULL, NULL);
                opened_for_preview = (doc != NULL);
            }
            g_free(resolved);
        }

        g_string_append_printf(listing, "\n  \xE2\x80\xA2 %s (%d lines)%s",
            e->file_name, lines,
            (doc || on_disk) ? "" : "  [new file]");

        if (doc) {
            decorate_doc(doc, e->content);
        }
        if (opened_for_preview) {
            docs_we_opened = g_list_prepend(docs_we_opened, doc);
        }
    }

    g_string_append(listing, _("\n\nApply changes?"));

    preview_dialog = gtk_message_dialog_new(
        GTK_WINDOW(geany->main_widgets->window),
        GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE, "%s", listing->str);
    gtk_dialog_add_buttons(GTK_DIALOG(preview_dialog),
        _("_Apply"), GTK_RESPONSE_YES,
        _("_Discard"), GTK_RESPONSE_NO, NULL);
    gtk_window_set_default_size(GTK_WINDOW(preview_dialog), 380, -1);
    g_signal_connect(preview_dialog, "response",
                     G_CALLBACK(on_preview_response), NULL);
    g_string_free(listing, TRUE);
    gtk_widget_show(preview_dialog);
}

void lumila_diff_init(void)
{
    plugin_signal_connect(geany_plugin, NULL, "editor-notify", FALSE,
                          G_CALLBACK(on_diff_editor_notify), NULL);
}

void lumila_diff_cleanup(void)
{
    lumila_diff_clear_all();
}
