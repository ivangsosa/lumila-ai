#include "sidebar.h"
#include "chat.h"
#include "config.h"
#include "plugin.h"
#include <geanyplugin.h>

static GtkWidget *sidebar_widget = NULL;
static GtkWidget *chat_view = NULL;
static GtkWidget *input_view = NULL;
static GtkWidget *provider_combo = NULL;

static void on_send_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    (void)user_data;

    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(input_view));
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    gchar *text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);

    if (text && *text) {
        lumila_chat_send_message(text);
        gtk_text_buffer_set_text(buffer, "", -1);
    }

    g_free(text);
}

static void on_provider_changed(GtkComboBox *combo, gpointer user_data)
{
    (void)user_data;
    gint active = gtk_combo_box_get_active(combo);
    lumila_chat_set_provider(active);
}

static void on_new_chat_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    (void)user_data;

    lumila_chat_new_conversation();
}

void lumila_sidebar_init(void)
{
    sidebar_widget = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(sidebar_widget), 6);

    // Provider selector
    GtkWidget *provider_label = gtk_label_new(_("Provider:"));
    gtk_box_pack_start(GTK_BOX(sidebar_widget), provider_label, FALSE, FALSE, 0);

    provider_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(provider_combo), "Claude Sonnet 4.5");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(provider_combo), "Claude Sonnet 4.6");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(provider_combo), "Claude Opus 4.5");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(provider_combo), "Claude Opus 4.6");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(provider_combo), "GPT-5.2-Codex");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(provider_combo), "GPT-5.3-Codex");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(provider_combo), "Gemini 3 Flash");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(provider_combo), "Gemini 3 Pro");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(provider_combo), "Gemini 3.1 Pro");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(provider_combo), "Ollama Llama 3.2");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(provider_combo), "Ollama Qwen 2.5");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(provider_combo), "OpenRouter");

    // Set default provider from config
    gint default_provider = lumila_config_get_default_provider();
    gtk_combo_box_set_active(GTK_COMBO_BOX(provider_combo), default_provider);

    g_signal_connect(provider_combo, "changed", G_CALLBACK(on_provider_changed), NULL);
    gtk_box_pack_start(GTK_BOX(sidebar_widget), provider_combo, FALSE, FALSE, 0);

    // Chat view
    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    chat_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(chat_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(chat_view), GTK_WRAP_WORD);
    gtk_container_add(GTK_CONTAINER(scrolled), chat_view);
    gtk_box_pack_start(GTK_BOX(sidebar_widget), scrolled, TRUE, TRUE, 0);

    lumila_chat_set_view(GTK_TEXT_VIEW(chat_view));

    // Input area
    GtkWidget *input_scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(input_scrolled),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(input_scrolled, -1, 80);

    input_view = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(input_view), GTK_WRAP_WORD);
    gtk_container_add(GTK_CONTAINER(input_scrolled), input_view);
    gtk_box_pack_start(GTK_BOX(sidebar_widget), input_scrolled, FALSE, FALSE, 0);

    // Buttons container
    GtkWidget *buttons_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);

    // New Chat button
    GtkWidget *new_chat_button = gtk_button_new_with_label(_("New Chat"));
    g_signal_connect(new_chat_button, "clicked", G_CALLBACK(on_new_chat_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(buttons_box), new_chat_button, TRUE, TRUE, 0);

    // Send button
    GtkWidget *send_button = gtk_button_new_with_label(_("Send"));
    g_signal_connect(send_button, "clicked", G_CALLBACK(on_send_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(buttons_box), send_button, TRUE, TRUE, 0);

    gtk_box_pack_start(GTK_BOX(sidebar_widget), buttons_box, FALSE, FALSE, 0);

    // Add to Geany sidebar
    gtk_widget_show_all(sidebar_widget);
    GeanyMainWidgets *widgets = geany_data->main_widgets;
    gtk_container_add(GTK_CONTAINER(widgets->sidebar_notebook), sidebar_widget);

    GtkWidget *tab_label = gtk_label_new(_("Lumila"));
    gint page_num = gtk_notebook_page_num(GTK_NOTEBOOK(widgets->sidebar_notebook), sidebar_widget);
    gtk_notebook_set_tab_label(GTK_NOTEBOOK(widgets->sidebar_notebook),
                               gtk_notebook_get_nth_page(GTK_NOTEBOOK(widgets->sidebar_notebook), page_num),
                               tab_label);
}

void lumila_sidebar_cleanup(void)
{
    if (sidebar_widget) {
        gtk_widget_destroy(sidebar_widget);
        sidebar_widget = NULL;
    }
}

GtkWidget *lumila_sidebar_get_widget(void)
{
    return sidebar_widget;
}
