#include "chat_ui.h"
#include <string.h>
#include <time.h>

static GtkTextTag *user_tag = NULL;
static GtkTextTag *ai_tag = NULL;
static GtkTextTag *background_tag = NULL;
static GtkTextTag *user_name_tag = NULL;
static GtkTextTag *ai_name_tag = NULL;
static GtkTextTag *timestamp_tag = NULL;
static GtkTextTag *code_block_tag = NULL;
static GtkTextTag *code_keyword_tag = NULL;
static GtkTextTag *code_string_tag = NULL;
static GtkTextTag *code_comment_tag = NULL;
static GtkTextTag *code_number_tag = NULL;
static GtkTextTag *code_function_tag = NULL;

void lumila_chat_ui_init(GtkTextView *view)
{
    if (!view) return;

    GtkTextBuffer *buffer = gtk_text_view_get_buffer(view);

    // Create text tags for styling - user message without background
    user_tag = gtk_text_buffer_create_tag(buffer, "user",
        "justification", GTK_JUSTIFY_RIGHT,
        "left-margin", 80,
        "right-margin", 10,
        "pixels-above-lines", 10,
        "pixels-below-lines", 10,
        "foreground", "#82AAFF",  // Azul moderno brillante
        "wrap-mode", GTK_WRAP_WORD,
        NULL);

    ai_tag = gtk_text_buffer_create_tag(buffer, "ai",
        "justification", GTK_JUSTIFY_LEFT,
        "left-margin", 10,
        "right-margin", 80,
        "pixels-above-lines", 10,
        "pixels-below-lines", 10,
        "foreground", "#C8D3F5",  // Blanco azulado suave
        "wrap-mode", GTK_WRAP_WORD,
        NULL);

    // Sender name tags
    user_name_tag = gtk_text_buffer_create_tag(buffer, "user_name",
        "justification", GTK_JUSTIFY_RIGHT,
        "left-margin", 80,
        "right-margin", 10,
        "pixels-above-lines", 14,
        "pixels-below-lines", 4,
        "foreground", "#82AAFF",
        "weight", PANGO_WEIGHT_BOLD,
        NULL);

    ai_name_tag = gtk_text_buffer_create_tag(buffer, "ai_name",
        "justification", GTK_JUSTIFY_LEFT,
        "left-margin", 10,
        "right-margin", 80,
        "pixels-above-lines", 14,
        "pixels-below-lines", 4,
        "foreground", "#C3E88D",  // Verde moderno
        "weight", PANGO_WEIGHT_BOLD,
        NULL);

    // Timestamp tag
    timestamp_tag = gtk_text_buffer_create_tag(buffer, "timestamp",
        "justification", GTK_JUSTIFY_RIGHT,
        "left-margin", 60,
        "right-margin", 10,
        "pixels-above-lines", 2,
        "pixels-below-lines", 8,
        "foreground", "#888888",
        "size-points", 8.0,
        "style", PANGO_STYLE_ITALIC,
        NULL);

    // Code block tag
    code_block_tag = gtk_text_buffer_create_tag(buffer, "code_block",
        "background", "#1E1E1E",
        "foreground", "#D4D4D4",
        "family", "Monospace",
        "wrap-mode", GTK_WRAP_NONE,
        NULL);

    // Syntax highlighting tags
    code_keyword_tag = gtk_text_buffer_create_tag(buffer, "code_keyword",
        "foreground", "#569CD6",  // Azul para keywords
        "weight", PANGO_WEIGHT_BOLD,
        NULL);

    code_string_tag = gtk_text_buffer_create_tag(buffer, "code_string",
        "foreground", "#CE9178",  // Naranja para strings
        NULL);

    code_comment_tag = gtk_text_buffer_create_tag(buffer, "code_comment",
        "foreground", "#6A9955",  // Verde para comentarios
        "style", PANGO_STYLE_ITALIC,
        NULL);

    code_number_tag = gtk_text_buffer_create_tag(buffer, "code_number",
        "foreground", "#B5CEA8",  // Verde claro para números
        NULL);

    code_function_tag = gtk_text_buffer_create_tag(buffer, "code_function",
        "foreground", "#DCDCAA",  // Amarillo para funciones
        NULL);

    background_tag = gtk_text_buffer_create_tag(buffer, "background",
        "background", "#000000",
        NULL);

    // Apply CSS for modern dark background
    GtkCssProvider *css_provider = gtk_css_provider_new();
    const gchar *css_data =
        "textview {"
        "  background-color: #0f0f23;"
        "  color: #C8D3F5;"
        "}"
        "textview text {"
        "  background-color: #0f0f23;"
        "}";

    gtk_css_provider_load_from_data(css_provider, css_data, -1, NULL);

    GtkStyleContext *context = gtk_widget_get_style_context(GTK_WIDGET(view));
    gtk_style_context_add_provider(context,
                                    GTK_STYLE_PROVIDER(css_provider),
                                    GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    g_object_unref(css_provider);
}

static gchar *get_current_time_string(void)
{
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    gchar *time_str = g_strdup_printf("%02d:%02d", tm_info->tm_hour, tm_info->tm_min);
    return time_str;
}

// Keywords for syntax highlighting
static const gchar *keywords[] = {
    "function", "class", "if", "else", "for", "while", "return", "var", "let", "const",
    "import", "from", "export", "async", "await", "try", "catch", "finally", "throw",
    "def", "import", "as", "from", "if", "elif", "else", "for", "while", "return",
    "class", "def", "print", "None", "True", "False", "and", "or", "not", "in",
    "public", "private", "protected", "static", "void", "int", "string", "boolean",
    "new", "this", "super", "extends", "implements", "interface", "package", "null",
    "echo", "<?php", "?>", "$", "array", "isset", "unset", "foreach", "NULL", "TRUE", "FALSE",
    "SELECT", "FROM", "WHERE", "INSERT", "UPDATE", "DELETE", "JOIN", "CREATE", "TABLE",
    "html", "head", "body", "div", "span", "script", "style", "DOCTYPE",
    NULL
};

static gboolean is_keyword(const gchar *word)
{
    for (gint i = 0; keywords[i] != NULL; i++) {
        if (g_strcmp0(word, keywords[i]) == 0) {
            return TRUE;
        }
    }
    return FALSE;
}

static void insert_code_with_highlighting(GtkTextBuffer *buffer, GtkTextIter *iter, const gchar *line)
{
    const gchar *p = line;
    GString *token = g_string_new("");
    
    while (*p) {
        // Skip whitespace
        if (g_ascii_isspace(*p)) {
            if (token->len > 0) {
                gtk_text_buffer_insert(buffer, iter, token->str, -1);
                g_string_truncate(token, 0);
            }
            gchar spaces[2] = {*p, '\0'};
            gtk_text_buffer_insert(buffer, iter, spaces, -1);
            p++;
            continue;
        }
        
        // Check for comments (// or #)
        if ((*p == '/' && *(p+1) == '/') || *p == '#') {
            if (token->len > 0) {
                gtk_text_buffer_insert(buffer, iter, token->str, -1);
                g_string_truncate(token, 0);
            }
            // Insert rest of line as comment
            gtk_text_buffer_insert_with_tags(buffer, iter, p, -1, code_comment_tag, NULL);
            break;
        }
        
        // Check for strings
        if (*p == '"' || *p == '\'') {
            if (token->len > 0) {
                gtk_text_buffer_insert(buffer, iter, token->str, -1);
                g_string_truncate(token, 0);
            }
            gchar delimiter = *p;
            g_string_append_c(token, *p++);
            while (*p && *p != delimiter) {
                if (*p == '\\' && *(p+1)) {
                    g_string_append_c(token, *p++);
                }
                g_string_append_c(token, *p++);
            }
            if (*p == delimiter) {
                g_string_append_c(token, *p++);
            }
            gtk_text_buffer_insert_with_tags(buffer, iter, token->str, -1, code_string_tag, NULL);
            g_string_truncate(token, 0);
            continue;
        }
        
        // Check for numbers
        if (g_ascii_isdigit(*p)) {
            if (token->len > 0) {
                gtk_text_buffer_insert(buffer, iter, token->str, -1);
                g_string_truncate(token, 0);
            }
            while (*p && (g_ascii_isdigit(*p) || *p == '.')) {
                g_string_append_c(token, *p++);
            }
            gtk_text_buffer_insert_with_tags(buffer, iter, token->str, -1, code_number_tag, NULL);
            g_string_truncate(token, 0);
            continue;
        }
        
        // Build word token
        if (g_ascii_isalnum(*p) || *p == '_') {
            g_string_append_c(token, *p++);
        } else {
            if (token->len > 0) {
                // Check if keyword
                if (is_keyword(token->str)) {
                    gtk_text_buffer_insert_with_tags(buffer, iter, token->str, -1, code_keyword_tag, NULL);
                } else {
                    gtk_text_buffer_insert(buffer, iter, token->str, -1);
                }
                g_string_truncate(token, 0);
            }
            // Insert punctuation/symbol
            gchar symbol[2] = {*p, '\0'};
            gtk_text_buffer_insert(buffer, iter, symbol, -1);
            p++;
        }
    }
    
    // Handle remaining token
    if (token->len > 0) {
        if (is_keyword(token->str)) {
            gtk_text_buffer_insert_with_tags(buffer, iter, token->str, -1, code_keyword_tag, NULL);
        } else {
            gtk_text_buffer_insert(buffer, iter, token->str, -1);
        }
    }
    
    g_string_free(token, TRUE);
}

void lumila_chat_ui_append_user_message(GtkTextView *view, const gchar *message)
{
    if (!view || !message) return;

    GtkTextBuffer *buffer = gtk_text_view_get_buffer(view);
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(buffer, &end);

    // Add spacing between messages
    if (gtk_text_buffer_get_char_count(buffer) > 0) {
        gtk_text_buffer_insert(buffer, &end, "\n", -1);
    }

    // Insert sender name
    gtk_text_buffer_insert_with_tags(buffer, &end, "You", -1, user_name_tag, NULL);
    gtk_text_buffer_insert(buffer, &end, "\n", -1);

    // Insert message content with user bubble styling
    gtk_text_buffer_insert_with_tags(buffer, &end, message, -1, user_tag, NULL);
    gtk_text_buffer_insert(buffer, &end, "\n", -1);

    // Insert timestamp
    // gchar *time_str = get_current_time_string();
    // gtk_text_buffer_insert_with_tags(buffer, &end, time_str, -1, timestamp_tag, NULL);
    // g_free(time_str);
    // gtk_text_buffer_insert(buffer, &end, "\n", -1);

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

    // Add spacing between messages
    if (gtk_text_buffer_get_char_count(buffer) > 0) {
        gtk_text_buffer_insert(buffer, &end, "\n", -1);
    }

    // Insert sender name
    gtk_text_buffer_insert_with_tags(buffer, &end, "Lumila", -1, ai_name_tag, NULL);
    gtk_text_buffer_insert(buffer, &end, "\n", -1);

    // Check for code blocks and format them
    gchar **lines = g_strsplit(message, "\n", -1);
    gboolean in_code_block = FALSE;

    for (gint i = 0; lines[i] != NULL; i++) {
        gchar *line = lines[i];

        // Check for code block markers
        if (g_str_has_prefix(line, "```")) {
            in_code_block = !in_code_block;
            if (!in_code_block) {
                gtk_text_buffer_insert(buffer, &end, "\n", -1);
            }
            continue;
        }

        if (in_code_block) {
            // Insert code with syntax highlighting
            insert_code_with_highlighting(buffer, &end, line);
            gtk_text_buffer_insert(buffer, &end, "\n", -1);
        } else {
            // Insert regular message with AI styling
            gtk_text_buffer_insert_with_tags(buffer, &end, line, -1, ai_tag, NULL);
            if (lines[i+1] != NULL) {
                gtk_text_buffer_insert(buffer, &end, "\n", -1);
            }
        }
    }

    g_strfreev(lines);
    gtk_text_buffer_insert(buffer, &end, "\n", -1);

    // Insert timestamp
    // gchar *time_str = get_current_time_string();
    // gtk_text_buffer_insert_with_tags(buffer, &end, time_str, -1, timestamp_tag, NULL);
    // g_free(time_str);
    // gtk_text_buffer_insert(buffer, &end, "\n", -1);

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
