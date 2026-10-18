#include "contextmenu.h"
#include "chat.h"
#include "autocomplete.h"
#include "plugin.h"
#include <geanyplugin.h>
#include <string.h>

static GtkWidget *menu_sep = NULL;
static GtkWidget *menu_item = NULL;
static GtkWidget *mi_explain = NULL;
static GtkWidget *mi_refactor = NULL;
static GtkWidget *mi_docstring = NULL;
static GtkWidget *mi_typehints = NULL;
static GtkWidget *mi_send_sel = NULL;
static GtkWidget *mi_accept_ac = NULL;

/* Captured on "update-editor-menu" right before the popup is shown. */
static gchar *ctx_word = NULL;
static GeanyDocument *ctx_doc = NULL;

static GeanyDocument *ctx_active_doc(void)
{
    GeanyDocument *doc = ctx_doc;
    if (!DOC_VALID(doc)) {
        doc = document_get_current();
    }
    return doc;
}

/* Send the editor selection wrapped in a task prompt. Same embedding
 * convention as lumila_chat_send_selection (skip the auto file context:
 * the selection IS the context). */
static void ctx_send_selection_with_task(const gchar *task)
{
    GeanyDocument *doc = ctx_active_doc();
    if (!DOC_VALID(doc) || !doc->editor || !doc->editor->sci) return;

    ScintillaObject *sci = doc->editor->sci;
    gchar *sel = sci_get_selection_contents(sci);
    if (!sel || !*sel) {
        g_free(sel);
        return;
    }
    gchar *fname = g_path_get_basename(
        doc->file_name ? doc->file_name : "untitled");
    gchar *msg = g_strdup_printf("%s\n\n[Selection from %s]\n```\n%s\n```",
                                 task, fname, sel);
    lumila_chat_send_message_ex(msg, FALSE, TRUE, FALSE);

    g_free(msg);
    g_free(fname);
    g_free(sel);
}

static void on_explain(GtkMenuItem *item, gpointer data)
{
    (void)item;
    (void)data;
    GeanyDocument *doc = ctx_active_doc();
    if (DOC_VALID(doc) && doc->editor && doc->editor->sci &&
        sci_has_selection(doc->editor->sci)) {
        ctx_send_selection_with_task(
            "Explain the following code in detail:");
    } else if (ctx_word) {
        /* No selection: explain the word under the cursor with the
         * whole active file attached for context. */
        gchar *msg = g_strdup_printf("Explain `%s` in this file.", ctx_word);
        lumila_chat_send_message_ex(msg, FALSE, FALSE, FALSE);
        g_free(msg);
    }
}

static void on_refactor(GtkMenuItem *item, gpointer data)
{
    (void)item;
    (void)data;
    ctx_send_selection_with_task(
        "Refactor the following code to improve readability and "
        "performance. Keep the same functionality:");
}

static void on_docstring(GtkMenuItem *item, gpointer data)
{
    (void)item;
    (void)data;
    ctx_send_selection_with_task(
        "Generate documentation (docstrings/comments) for the following "
        "code. Output the updated code:");
}

static void on_typehints(GtkMenuItem *item, gpointer data)
{
    (void)item;
    (void)data;
    ctx_send_selection_with_task(
        "Add type hints/annotations to the following code where "
        "appropriate. Output the updated code:");
}

static void on_send_selection(GtkMenuItem *item, gpointer data)
{
    (void)item;
    (void)data;
    lumila_chat_send_selection();
}

static void on_accept_completion(GtkMenuItem *item, gpointer data)
{
    (void)item;
    (void)data;
    lumila_autocomplete_accept();
}

static void on_update_editor_menu(GObject *obj, const gchar *word, gint pos,
                                  GeanyDocument *doc, gpointer user_data)
{
    (void)obj;
    (void)pos;
    (void)user_data;

    g_free(ctx_word);
    ctx_word = (word && *word) ? g_strdup(word) : NULL;
    ctx_doc = doc;

    const gboolean streaming = lumila_chat_is_streaming();
    const gboolean has_sel = DOC_VALID(doc) && doc->editor &&
        doc->editor->sci && sci_has_selection(doc->editor->sci);

    if (mi_explain) {
        gtk_widget_set_sensitive(mi_explain,
                                 !streaming && (has_sel || ctx_word != NULL));
    }
    if (mi_refactor)  gtk_widget_set_sensitive(mi_refactor, !streaming && has_sel);
    if (mi_docstring) gtk_widget_set_sensitive(mi_docstring, !streaming && has_sel);
    if (mi_typehints) gtk_widget_set_sensitive(mi_typehints, !streaming && has_sel);
    if (mi_send_sel)  gtk_widget_set_sensitive(mi_send_sel, !streaming && has_sel);
    if (mi_accept_ac) {
        gtk_widget_set_sensitive(mi_accept_ac,
                                 lumila_autocomplete_has_pending());
    }
}

void lumila_context_menu_init(void)
{
    GtkWidget *editor_menu = geany->main_widgets->editor_menu;
    if (!editor_menu) return;

    menu_sep = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(editor_menu), menu_sep);

    GtkWidget *sub = gtk_menu_new();
    menu_item = gtk_menu_item_new_with_label("Lumila AI");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), sub);
    gtk_menu_shell_append(GTK_MENU_SHELL(editor_menu), menu_item);

    mi_explain = gtk_menu_item_new_with_label(_("Explain This"));
    g_signal_connect(mi_explain, "activate", G_CALLBACK(on_explain), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(sub), mi_explain);

    mi_refactor = gtk_menu_item_new_with_label(_("Refactor Selection"));
    g_signal_connect(mi_refactor, "activate", G_CALLBACK(on_refactor), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(sub), mi_refactor);

    mi_docstring = gtk_menu_item_new_with_label(_("Generate Docstring"));
    g_signal_connect(mi_docstring, "activate",
                     G_CALLBACK(on_docstring), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(sub), mi_docstring);

    mi_typehints = gtk_menu_item_new_with_label(_("Add Type Hints"));
    g_signal_connect(mi_typehints, "activate",
                     G_CALLBACK(on_typehints), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(sub), mi_typehints);

    GtkWidget *sep = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(sub), sep);

    mi_send_sel = gtk_menu_item_new_with_label(_("Send Selection to Chat"));
    g_signal_connect(mi_send_sel, "activate",
                     G_CALLBACK(on_send_selection), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(sub), mi_send_sel);

    mi_accept_ac = gtk_menu_item_new_with_label(_("Accept AI Completion"));
    gtk_widget_set_tooltip_text(mi_accept_ac,
        "Insert the pending AI completion suggestion");
    g_signal_connect(mi_accept_ac, "activate",
                     G_CALLBACK(on_accept_completion), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(sub), mi_accept_ac);

    gtk_widget_show_all(menu_item);
    gtk_widget_show(menu_sep);

    plugin_signal_connect(geany_plugin, NULL, "update-editor-menu", FALSE,
                          G_CALLBACK(on_update_editor_menu), NULL);
}

void lumila_context_menu_cleanup(void)
{
    /* Items added to editor_menu must be destroyed by the plugin. */
    if (menu_item) {
        gtk_widget_destroy(menu_item);   /* destroys the submenu too */
        menu_item = NULL;
    }
    if (menu_sep) {
        gtk_widget_destroy(menu_sep);
        menu_sep = NULL;
    }
    mi_explain = mi_refactor = mi_docstring = mi_typehints = NULL;
    mi_send_sel = mi_accept_ac = NULL;
    g_free(ctx_word);
    ctx_word = NULL;
    ctx_doc = NULL;
}
