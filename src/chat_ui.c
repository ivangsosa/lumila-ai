#include "chat_ui.h"
#include <string.h>

static GtkTextTag *user_tag = NULL;
static GtkTextTag *ai_tag = NULL;
static GtkTextTag *background_tag = NULL;

void lumila_chat_ui_init(GtkTextView *view)
{
    if (!view) return;

    GtkTextBuffer *buffer = gtk_text_view_get_buffer(view);

    // Create text tags for styling
    user_tag = gtk_text_buffer_create_tag(buffer, "user",
        "justification", GTK_JUSTIFY_RIGHT,
        "left-margin", 100,
        "right-margin", 10,
        "pixels-above-lines", 5,
        "pixels-below-lines", 5,
        "background", "#2D5F8D",
        "foreground", "#FFFFFF",
        "wrap-mode", GTK_WRAP_WORD,
        NULL);

    ai_tag = gtk_text_buffer_create_tag(buffer, "ai",
        "justification", GTK_JUSTIFY_LEFT,
        "left-margin", 10,
        "right-margin", 100,
        "pixels-above-lines", 5,
        "pixels-below-lines", 5,
        "foreground", "#E0E0E0",
        "wrap-mode", GTK_WRAP_WORD,
        NULL);

    background_tag = gtk_text_buffer_create_tag(buffer, "background",
        "background", "#000000",
        NULL);

    // Apply CSS for black background
    GtkCssProvider *css_provider = gtk_css_provider_new();
    const gchar *css_data = 
        "textview {"
        "  background-color: #000000;"
        "  color: #E0E0E0;"
        "}"
        "textview text {"
        "  background-color: #000000;"
        "}";

    gtk_css_provider_load_from_data(css_provider, css_data, -1, NULL);

    GtkStyleContext *context = gtk_widget_get_style_context(GTK_WIDGET(view));
    gtk_style_context_add_provider(context,
                                    GTK_STYLE_PROVIDER(css_provider),
                                    GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    g_object_unref(css_provider);
}

void lumila_chat_ui_append_user_message(GtkTextView *view, const gchar *message)
{
    if (!view || !message) return;

    GtkTextBuffer *buffer = gtk_text_view_get_buffer(view);
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(buffer, &end);

    // Add spacing if not first message
    if (gtk_text_buffer_get_char_count(buffer) > 0) {
        gtk_text_buffer_insert(buffer, &end, "\n", -1);
    }

    // Insert "You: " prefix
    gtk_text_buffer_insert_with_tags(buffer, &end, "You: ", -1, user_tag, NULL);

    // Insert message content
    gtk_text_buffer_insert_with_tags(buffer, &end, message, -1, user_tag, NULL);
    gtk_text_buffer_insert(buffer, &end, "\n", -1);

    // Scroll to end
    GtkTextMark *mark = gtk_text_buffer_get_insert(buffer);
    gtk_text_view_scroll_to_mark(view, mark, 0.0, FALSE, 0.0, 0.0);
}

void lumila_chat_ui_append_ai_message(GtkTextView *view, const gchar *message)
{
    if (!view || !message) return;

    GtkTextBuffer *buffer = gtk_text_view_get_buffer(view);
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(buffer, &end);

    // Add spacing if not first message
    if (gtk_text_buffer_get_char_count(buffer) > 0) {
        gtk_text_buffer_insert(buffer, &end, "\n", -1);
    }

    // Insert "Lumila: " prefix
    gtk_text_buffer_insert_with_tags(buffer, &end, "Lumila: ", -1, ai_tag, NULL);

    // Insert message content
    gtk_text_buffer_insert_with_tags(buffer, &end, message, -1, ai_tag, NULL);
    gtk_text_buffer_insert(buffer, &end, "\n", -1);

    // Scroll to end
    GtkTextMark *mark = gtk_text_buffer_get_insert(buffer);
    gtk_text_view_scroll_to_mark(view, mark, 0.0, FALSE, 0.0, 0.0);
}

void lumila_chat_ui_clear(GtkTextView *view)
{
    if (!view) return;

    GtkTextBuffer *buffer = gtk_text_view_get_buffer(view);
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    gtk_text_buffer_delete(buffer, &start, &end);
}
