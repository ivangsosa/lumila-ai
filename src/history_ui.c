#include "history_ui.h"
#include "history.h"
#include "chat.h"
#include <string.h>

static GtkWidget *history_list = NULL;
static GtkWidget *stack_ref = NULL;

static void on_continue_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    const gchar *filename = (const gchar *)user_data;
    lumila_chat_load_conversation(filename);
}

static void on_delete_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    const gchar *filename = (const gchar *)user_data;
    if (lumila_history_delete(filename)) {
        lumila_history_ui_refresh();
    }
}

static GtkWidget *create_history_row(LumilaHistoryEntry *entry)
{
    GtkWidget *row = gtk_list_box_row_new();
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(box), 6);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_hexpand(vbox, TRUE);

    gchar *title_text;
    if (entry->title && *entry->title)
        title_text = g_strdup_printf("<b>%s</b>", entry->title);
    else
        title_text = g_strdup_printf("<b>%s</b>", entry->filename);

    GtkWidget *title_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title_label), title_text);
    gtk_label_set_ellipsize(GTK_LABEL(title_label), PANGO_ELLIPSIZE_END);
    gtk_widget_set_halign(title_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(vbox), title_label, FALSE, FALSE, 0);
    g_free(title_text);

    gchar *meta = g_strdup_printf("%s  |  %s", entry->created_at, entry->model_name);
    gchar *meta_markup = g_strdup_printf("<span style='italic' color='#888888'>%s</span>", meta);
    GtkWidget *meta_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(meta_label), meta_markup);
    gtk_widget_set_halign(meta_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(vbox), meta_label, FALSE, FALSE, 0);
    g_free(meta_markup);
    g_free(meta);

    gtk_box_pack_start(GTK_BOX(box), vbox, TRUE, TRUE, 0);

    GtkWidget *continue_btn = gtk_button_new_with_label("Continue");
    g_signal_connect_data(continue_btn, "clicked", G_CALLBACK(on_continue_clicked),
                          g_strdup(entry->filename), (GClosureNotify)g_free, 0);
    gtk_box_pack_start(GTK_BOX(box), continue_btn, FALSE, FALSE, 0);

    GtkWidget *delete_btn = gtk_button_new_with_label("Delete");
    g_signal_connect_data(delete_btn, "clicked", G_CALLBACK(on_delete_clicked),
                          g_strdup(entry->filename), (GClosureNotify)g_free, 0);
    gtk_box_pack_start(GTK_BOX(box), delete_btn, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(row), box);
    return row;
}

void lumila_history_ui_refresh(void)
{
    if (!history_list) return;

    GList *children = gtk_container_get_children(GTK_CONTAINER(history_list));
    for (GList *l = children; l != NULL; l = l->next) {
        gtk_widget_destroy(GTK_WIDGET(l->data));
    }
    g_list_free(children);

    GList *entries = lumila_history_list();
    for (GList *l = entries; l != NULL; l = l->next) {
        LumilaHistoryEntry *entry = (LumilaHistoryEntry *)l->data;
        GtkWidget *row = create_history_row(entry);
        gtk_container_add(GTK_CONTAINER(history_list), row);
    }

    g_list_free_full(entries, (GDestroyNotify)lumila_history_entry_free);
    gtk_widget_show_all(history_list);
}

static void on_back_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    GtkWidget *stack = (GtkWidget *)user_data;
    gtk_stack_set_visible_child_name(GTK_STACK(stack), "chat");
}

GtkWidget *lumila_history_ui_create(GtkWidget *stack)
{
    stack_ref = stack;

    GtkWidget *page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(page), 6);

    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *back_btn = gtk_button_new_with_label("Back to Chat");
    g_signal_connect(back_btn, "clicked", G_CALLBACK(on_back_clicked), stack);
    gtk_box_pack_start(GTK_BOX(header), back_btn, FALSE, FALSE, 0);

    GtkWidget *title = gtk_label_new("<b>Conversation History</b>");
    gtk_label_set_markup(GTK_LABEL(title), "<b>Conversation History</b>");
    gtk_box_pack_start(GTK_BOX(header), title, TRUE, TRUE, 0);

    gtk_box_pack_start(GTK_BOX(page), header, FALSE, FALSE, 0);

    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    history_list = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(history_list), GTK_SELECTION_NONE);
    gtk_container_add(GTK_CONTAINER(scrolled), history_list);
    gtk_box_pack_start(GTK_BOX(page), scrolled, TRUE, TRUE, 0);

    lumila_history_ui_refresh();

    return page;
}
