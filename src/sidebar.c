#include "sidebar.h"
#include "chat.h"
#include "config.h"
#include "plugin.h"
#include "history_ui.h"
#include <geanyplugin.h>

static GtkWidget *sidebar_widget = NULL;
static GtkWidget *chat_view = NULL;
static GtkWidget *input_view = NULL;
static GtkWidget *provider_combo = NULL;
static GtkWidget *status_label = NULL;
static GtkWidget *send_button = NULL;
static GtkWidget *cancel_button = NULL;

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

static void on_cancel_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    (void)user_data;

    lumila_sidebar_cancel_request();
}

static void on_history_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    GtkWidget *stack = (GtkWidget *)user_data;
    lumila_history_ui_refresh();
    gtk_stack_set_visible_child_name(GTK_STACK(stack), "history");
}

static gboolean on_input_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
    (void)widget;
    (void)user_data;

    if ((event->state & GDK_CONTROL_MASK) &&
        (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter)) {
        on_send_clicked(NULL, NULL);
        return TRUE;
    }

    if (event->keyval == GDK_KEY_Escape) {
        lumila_sidebar_cancel_request();
        return TRUE;
    }

    return FALSE;
}

void lumila_sidebar_init(void)
{
    sidebar_widget = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(sidebar_widget), 6);

    // Apply CSS for modern dark sidebar
    GtkCssProvider *css_provider = gtk_css_provider_new();
    const gchar *css_data =
        "* { background-color: #0f0f23; color: #C8D3F5; }"
        "button { background-color: #1a1a3e; border: 1px solid #2a2a5e; border-radius: 4px; padding: 6px; }"
        "button:hover { background-color: #25255a; }"
        "textview { background-color: #0f0f23; color: #C8D3F5; }"
        "comboboxtext { background-color: #1a1a3e; color: #C8D3F5; border: 1px solid #2a2a5e; }";
    gtk_css_provider_load_from_data(css_provider, css_data, -1, NULL);
    GtkStyleContext *ctx = gtk_widget_get_style_context(sidebar_widget);
    gtk_style_context_add_provider(ctx, GTK_STYLE_PROVIDER(css_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css_provider);

    // Create stack for chat and history views
    GtkWidget *stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(stack), GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);

    // ---- CHAT PAGE ----
    GtkWidget *chat_page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);

    // Provider selector
    GtkWidget *provider_label = gtk_label_new(_("Provider:"));
    gtk_box_pack_start(GTK_BOX(chat_page), provider_label, FALSE, FALSE, 0);

    provider_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(provider_combo), "Claude Sonnet 4");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(provider_combo), "Claude Opus 4");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(provider_combo), "GPT-4.1");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(provider_combo), "GPT-4.1 mini");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(provider_combo), "Gemini 2.5 Flash");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(provider_combo), "Gemini 2.5 Pro");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(provider_combo), "Ollama Llama 3.3");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(provider_combo), "Ollama Qwen3");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(provider_combo), "OpenRouter Quasar");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(provider_combo), "DeepSeek Chat");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(provider_combo), "DeepSeek Reasoner");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(provider_combo), "Mistral Large");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(provider_combo), "Ollama Mistral Small");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(provider_combo), "OpenRouter Free");

    gint default_provider = lumila_config_get_default_provider();
    gtk_combo_box_set_active(GTK_COMBO_BOX(provider_combo), default_provider);
    g_signal_connect(provider_combo, "changed", G_CALLBACK(on_provider_changed), NULL);
    gtk_box_pack_start(GTK_BOX(chat_page), provider_combo, FALSE, FALSE, 0);

    // Chat view
    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    chat_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(chat_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(chat_view), GTK_WRAP_WORD);
    gtk_container_add(GTK_CONTAINER(scrolled), chat_view);
    gtk_box_pack_start(GTK_BOX(chat_page), scrolled, TRUE, TRUE, 0);

    lumila_chat_set_view(GTK_TEXT_VIEW(chat_view));

    // Input area
    GtkWidget *input_scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(input_scrolled),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(input_scrolled, -1, 80);

    input_view = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(input_view), GTK_WRAP_WORD);
    gtk_widget_add_events(input_view, GDK_KEY_PRESS_MASK);
    g_signal_connect(input_view, "key-press-event", G_CALLBACK(on_input_key_press), NULL);
    gtk_container_add(GTK_CONTAINER(input_scrolled), input_view);
    gtk_box_pack_start(GTK_BOX(chat_page), input_scrolled, FALSE, FALSE, 0);

    // Status label
    status_label = gtk_label_new("");
    gtk_widget_set_no_show_all(status_label, TRUE);
    gtk_box_pack_start(GTK_BOX(chat_page), status_label, FALSE, FALSE, 0);

    // Buttons container
    GtkWidget *buttons_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);

    GtkWidget *history_button = gtk_button_new_with_label(_("History"));
    g_signal_connect(history_button, "clicked", G_CALLBACK(on_history_clicked), stack);
    gtk_box_pack_start(GTK_BOX(buttons_box), history_button, TRUE, TRUE, 0);

    GtkWidget *new_chat_button = gtk_button_new_with_label(_("New Chat"));
    g_signal_connect(new_chat_button, "clicked", G_CALLBACK(on_new_chat_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(buttons_box), new_chat_button, TRUE, TRUE, 0);

    send_button = gtk_button_new_with_label(_("Send"));
    g_signal_connect(send_button, "clicked", G_CALLBACK(on_send_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(buttons_box), send_button, TRUE, TRUE, 0);

    cancel_button = gtk_button_new_with_label(_("Cancel"));
    g_signal_connect(cancel_button, "clicked", G_CALLBACK(on_cancel_clicked), NULL);
    gtk_widget_set_sensitive(cancel_button, FALSE);
    gtk_box_pack_start(GTK_BOX(buttons_box), cancel_button, TRUE, TRUE, 0);

    gtk_box_pack_start(GTK_BOX(chat_page), buttons_box, FALSE, FALSE, 0);

    gtk_stack_add_named(GTK_STACK(stack), chat_page, "chat");

    // ---- HISTORY PAGE ----
    GtkWidget *history_page = lumila_history_ui_create(stack);
    gtk_stack_add_named(GTK_STACK(stack), history_page, "history");

    gtk_stack_set_visible_child_name(GTK_STACK(stack), "chat");
    gtk_box_pack_start(GTK_BOX(sidebar_widget), stack, TRUE, TRUE, 0);

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

void lumila_sidebar_set_status(const gchar *status)
{
    if (!status_label) return;

    if (status && *status) {
        gtk_label_set_text(GTK_LABEL(status_label), status);
        gtk_widget_show(status_label);
    } else {
        gtk_widget_hide(status_label);
    }
}

void lumila_sidebar_set_input_sensitive(gboolean sensitive)
{
    if (input_view) {
        gtk_widget_set_sensitive(input_view, sensitive);
        if (sensitive) {
            gtk_widget_grab_focus(input_view);
        }
    }
    if (send_button) {
        gtk_widget_set_sensitive(send_button, sensitive);
    }
    if (cancel_button) {
        gtk_widget_set_sensitive(cancel_button, !sensitive);
    }
}

void lumila_sidebar_cancel_request(void)
{
    lumila_chat_cancel_request();
}
