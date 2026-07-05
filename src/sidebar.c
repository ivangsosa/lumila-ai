#include "sidebar.h"
#include "chat.h"
#include "config.h"
#include "plugin.h"
#include "history_ui.h"
#include "providers/model_registry.h"
#include <geanyplugin.h>

static GtkWidget *sidebar_widget = NULL;
static GtkWidget *chat_view = NULL;
static GtkWidget *input_view = NULL;
static GtkWidget *provider_combo = NULL;
static GtkWidget *status_label = NULL;
static GtkWidget *action_button = NULL;
static GtkWidget *mode_combo = NULL;
static gboolean is_streaming = FALSE;

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

static void on_send_selection_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    (void)user_data;
    lumila_chat_send_selection();
}

static void on_send_file_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    (void)user_data;
    lumila_chat_send_current_file();
}

static void on_ask_toggled(GtkToggleButton *toggle, gpointer user_data)
{
    (void)user_data;
    lumila_chat_set_ask_mode(gtk_toggle_button_get_active(toggle));
}

static void on_export_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    (void)user_data;
    lumila_chat_export_markdown();
}

static void on_check_updates_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    (void)user_data;

    const gchar *instructions =
        "# Lumila AI - Update Instructions\n\n"
        "To update the plugin without losing your config.json or history:\n\n"
        "## Step 1: Close Geany (required)\n\n"
        "The .so file is locked in memory while Geany is running.\n"
        "Close Geany completely before replacing the file.\n\n"
        "```bash\n"
        "killall geany\n"
        "```\n\n"
        "## Step 2: Download the latest version\n\n"
        "```bash\n"
        "wget https://github.com/ivangsosa/lumila-ai/releases/latest/download/lumila-ai.so \\\n"
        "  -O ~/.config/geany/plugins/lumila-ai/lumila-ai.so.new\n"
        "```\n\n"
        "## Step 3: Backup and replace\n\n"
        "```bash\n"
        "mv ~/.config/geany/plugins/lumila-ai.so \\\n"
        "   ~/.config/geany/plugins/lumila-ai.so.bak\n"
        "mv ~/.config/geany/plugins/lumila-ai/lumila-ai.so.new \\\n"
        "   ~/.config/geany/plugins/lumila-ai.so\n"
        "```\n\n"
        "## Step 4: Restart Geany\n\n"
        "```bash\n"
        "geany &\n"
        "```\n\n"
        "Your config.json and conversation history are preserved.\n";

    GeanyDocument *doc = document_new_file("lumila-update.md", NULL, NULL);
    if (doc && doc->editor && doc->editor->sci) {
        sci_set_text(doc->editor->sci, instructions);
        document_set_text_changed(doc, TRUE);
    }
}

static void on_action_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    (void)user_data;

    if (is_streaming) {
        lumila_sidebar_cancel_request();
    } else {
        on_send_clicked(NULL, NULL);
    }
}

static void on_mode_code(GtkMenuItem *item, gpointer user_data)
{
    (void)item;
    (void)user_data;
    lumila_chat_set_ask_mode(FALSE);
    if (mode_combo) gtk_button_set_label(GTK_BUTTON(mode_combo), "Code");
}

static void on_mode_ask(GtkMenuItem *item, gpointer user_data)
{
    (void)item;
    (void)user_data;
    lumila_chat_set_ask_mode(TRUE);
    if (mode_combo) gtk_button_set_label(GTK_BUTTON(mode_combo), "Ask");
}

static void on_mode_plan(GtkMenuItem *item, gpointer user_data)
{
    (void)item;
    (void)user_data;
    /* Plan mode: ask mode off for now, will be expanded later */
    lumila_chat_set_ask_mode(FALSE);
    if (mode_combo) gtk_button_set_label(GTK_BUTTON(mode_combo), "Plan");
}

static void on_more_clicked(GtkButton *button, gpointer user_data)
{
    GtkWidget *menu = GTK_WIDGET(user_data);
    gtk_menu_popup_at_widget(GTK_MENU(menu), GTK_WIDGET(button),
                             GDK_GRAVITY_SOUTH, GDK_GRAVITY_NORTH, NULL);
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

    if (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter) {
        if (event->state & GDK_SHIFT_MASK) {
            return FALSE;
        }
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
        "button { background-color: #1a1a3e; border: 1px solid #2a2a5e; border-radius: 4px; padding: 4px 8px; }"
        "button:hover { background-color: #25255a; }"
        "textview { background-color: #0f0f23; color: #C8D3F5; }"
        ".input-frame { border-top: 1px solid #2a2a5e; margin-top: 6px; padding: 4px; }"
        ".input-frame textview { background-color: #141432; }"
        "comboboxtext { background-color: #1a1a3e; color: #C8D3F5; border: 1px solid #2a2a5e; padding: 2px; }"
        ".toolbar-small button { padding: 2px 6px; font-size: 10px; min-width: 24px; min-height: 24px; }"
        ".toolbar-small comboboxtext { padding: 1px; font-size: 10px; }";
    gtk_css_provider_load_from_data(css_provider, css_data, -1, NULL);
    GtkStyleContext *ctx = gtk_widget_get_style_context(sidebar_widget);
    gtk_style_context_add_provider(ctx, GTK_STYLE_PROVIDER(css_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css_provider);

    // Create stack for chat and history views
    GtkWidget *stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(stack), GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);

    // ---- CHAT PAGE ----
    GtkWidget *chat_page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);

    // === TOP TOOLBAR (right-aligned, small icons) ===
    GtkWidget *top_toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_style_context_add_class(gtk_widget_get_style_context(top_toolbar), "toolbar-small");
    gtk_widget_set_halign(top_toolbar, GTK_ALIGN_FILL);

    // Left spacer pushes buttons to the right
    GtkWidget *top_spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(top_spacer, TRUE);
    gtk_box_pack_start(GTK_BOX(top_toolbar), top_spacer, TRUE, TRUE, 0);

    GtkWidget *new_chat_btn = gtk_button_new_with_label("+");
    gtk_widget_set_tooltip_text(new_chat_btn, "New Chat");
    g_signal_connect(new_chat_btn, "clicked", G_CALLBACK(on_new_chat_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(top_toolbar), new_chat_btn, FALSE, FALSE, 0);

    GtkWidget *history_btn = gtk_button_new_with_label("\xE2\x8F\xB1");
    gtk_widget_set_tooltip_text(history_btn, "History");
    g_signal_connect(history_btn, "clicked", G_CALLBACK(on_history_clicked), stack);
    gtk_box_pack_start(GTK_BOX(top_toolbar), history_btn, FALSE, FALSE, 0);

    // More options menu (three dots)
    GtkWidget *more_menu = gtk_menu_new();
    GtkWidget *item_send_sel = gtk_menu_item_new_with_label("Send Selection");
    g_signal_connect(item_send_sel, "activate", G_CALLBACK(on_send_selection_clicked), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(more_menu), item_send_sel);

    GtkWidget *item_send_file = gtk_menu_item_new_with_label("Send File");
    g_signal_connect(item_send_file, "activate", G_CALLBACK(on_send_file_clicked), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(more_menu), item_send_file);

    GtkWidget *sep1 = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(more_menu), sep1);

    GtkWidget *item_export = gtk_menu_item_new_with_label("Export MD");
    g_signal_connect(item_export, "activate", G_CALLBACK(on_export_clicked), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(more_menu), item_export);

    GtkWidget *item_updates = gtk_menu_item_new_with_label("Check Updates");
    g_signal_connect(item_updates, "activate", G_CALLBACK(on_check_updates_clicked), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(more_menu), item_updates);

    gtk_widget_show_all(more_menu);

    GtkWidget *more_btn = gtk_button_new_with_label("...");
    gtk_widget_set_tooltip_text(more_btn, "More options");
    g_signal_connect(more_btn, "clicked", G_CALLBACK(on_more_clicked), more_menu);
    gtk_box_pack_start(GTK_BOX(top_toolbar), more_btn, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(chat_page), top_toolbar, FALSE, FALSE, 0);

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

    // Status label
    status_label = gtk_label_new("");
    gtk_widget_set_no_show_all(status_label, TRUE);
    gtk_box_pack_start(GTK_BOX(chat_page), status_label, FALSE, FALSE, 0);

    // === INPUT FRAME ===
    GtkWidget *input_frame = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_style_context_add_class(gtk_widget_get_style_context(input_frame), "input-frame");

    // Bottom toolbar: Mode + Model + Send/Cancel
    GtkWidget *bottom_toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_style_context_add_class(gtk_widget_get_style_context(bottom_toolbar), "toolbar-small");

    // Mode selector (GtkMenuButton opens upward)
    GtkWidget *mode_menu = gtk_menu_new();
    GtkWidget *item_code = gtk_menu_item_new_with_label("Code");
    g_signal_connect(item_code, "activate", G_CALLBACK(on_mode_code), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(mode_menu), item_code);

    GtkWidget *item_ask = gtk_menu_item_new_with_label("Ask");
    g_signal_connect(item_ask, "activate", G_CALLBACK(on_mode_ask), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(mode_menu), item_ask);

    GtkWidget *item_plan = gtk_menu_item_new_with_label("Plan");
    g_signal_connect(item_plan, "activate", G_CALLBACK(on_mode_plan), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(mode_menu), item_plan);

    gtk_widget_show_all(mode_menu);

    mode_combo = gtk_menu_button_new();
    gtk_menu_button_set_popup(GTK_MENU_BUTTON(mode_combo), mode_menu);
    gtk_button_set_label(GTK_BUTTON(mode_combo), "Code");
    gtk_menu_button_set_direction(GTK_MENU_BUTTON(mode_combo), GTK_ARROW_UP);
    gtk_box_pack_start(GTK_BOX(bottom_toolbar), mode_combo, FALSE, FALSE, 0);

    // Model selector - populated dynamically from registry
    provider_combo = gtk_combo_box_text_new();
    for (gint i = 0; i < LUMILA_NUM_PROVIDERS; i++) {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(provider_combo),
            lumila_model_registry_get_display_name(i));
    }

    gint default_provider = lumila_config_get_default_provider();
    if (default_provider < 0 || default_provider >= LUMILA_NUM_PROVIDERS)
        default_provider = 9;
    gtk_combo_box_set_active(GTK_COMBO_BOX(provider_combo), default_provider);
    g_signal_connect(provider_combo, "changed", G_CALLBACK(on_provider_changed), NULL);
    gtk_box_pack_start(GTK_BOX(bottom_toolbar), provider_combo, FALSE, FALSE, 0);

    // Spacer
    GtkWidget *bottom_spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(bottom_spacer, TRUE);
    gtk_box_pack_start(GTK_BOX(bottom_toolbar), bottom_spacer, TRUE, TRUE, 0);

    // Send/Cancel unified button
    action_button = gtk_button_new_with_label("Send");
    g_signal_connect(action_button, "clicked", G_CALLBACK(on_action_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(bottom_toolbar), action_button, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(input_frame), bottom_toolbar, FALSE, FALSE, 0);

    // Input text area
    GtkWidget *input_scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(input_scrolled),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(input_scrolled, -1, 60);

    input_view = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(input_view), GTK_WRAP_WORD);
    gtk_widget_add_events(input_view, GDK_KEY_PRESS_MASK);
    g_signal_connect(input_view, "key-press-event", G_CALLBACK(on_input_key_press), NULL);
    gtk_container_add(GTK_CONTAINER(input_scrolled), input_view);
    gtk_box_pack_start(GTK_BOX(input_frame), input_scrolled, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(chat_page), input_frame, FALSE, FALSE, 0);

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
    is_streaming = !sensitive;

    if (input_view) {
        gtk_widget_set_sensitive(input_view, sensitive);
        if (sensitive) {
            gtk_widget_grab_focus(input_view);
        }
    }
    if (action_button) {
        if (is_streaming) {
            gtk_button_set_label(GTK_BUTTON(action_button), "Cancel");
        } else {
            gtk_button_set_label(GTK_BUTTON(action_button), "Send");
        }
    }
}

void lumila_sidebar_cancel_request(void)
{
    lumila_chat_cancel_request();
}
