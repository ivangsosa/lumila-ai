#include "chat.h"
#include "chat_ui.h"
#include "message.h"
#include "providers/provider.h"
#include "providers/model_registry.h"
#include "config.h"
#include "plugin.h"
#include "sidebar.h"
#include "history.h"
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
    "\n" \
    "If no file is open or the user just wants to chat, answer normally without using ```file: blocks. " \
    "Do not output system metadata, safety labels, or reasoning steps. " \
    "Respond directly with the answer or code.\n\n"

static void append_message_to_view(const gchar *role, const gchar *content);
static gchar *get_current_timestamp(void);
static gchar *apply_file_edits(const gchar *response);

static GtkTextView *chat_view = NULL;
static GArray *messages = NULL;
static LumilaProvider *current_provider = NULL;
static gint current_provider_id = 0;
static gchar *conversation_id = NULL;
static gchar *history_dir = NULL;
static gchar *conversation_title = NULL;
static GString *stream_response = NULL;
static gboolean ask_mode = FALSE;

/* Idle callback data for thread-safe UI updates */
typedef struct {
    gchar *chunk;
    gboolean is_final;
    gchar *full_response;
} StreamIdleData;

static gboolean stream_idle_callback(gpointer user_data)
{
    StreamIdleData *data = (StreamIdleData *)user_data;

    if (data->is_final) {
        if (data->full_response) {
            gchar *cleaned = ask_mode ? NULL : apply_file_edits(data->full_response);
            lumila_chat_ui_stream_end_and_render(chat_view, data->full_response);
            LumilaMessage msg = {
                .role = g_strdup("assistant"),
                .content = g_strdup(data->full_response),
                .timestamp = get_current_timestamp()
            };
            g_array_append_val(messages, msg);
            if (cleaned) {
                if (strlen(cleaned) > strlen(data->full_response)) {
                    GtkTextBuffer *buffer = gtk_text_view_get_buffer(chat_view);
                    GtkTextIter end;
                    gtk_text_buffer_get_end_iter(buffer, &end);
                    gtk_text_buffer_insert(buffer, &end, "\n", -1);
                    gtk_text_buffer_insert(buffer, &end, cleaned + strlen(data->full_response), -1);
                }
                g_free(cleaned);
            }
        } else {
            lumila_chat_ui_stream_end(chat_view);
            append_message_to_view("assistant", "Error: Failed to get response");
        }
        lumila_sidebar_set_status(NULL);
        lumila_sidebar_set_input_sensitive(TRUE);
        g_free(data->full_response);
    } else if (data->chunk) {
        lumila_chat_ui_stream_append(chat_view, data->chunk);
    }

    g_free(data->chunk);
    g_free(data);
    return G_SOURCE_REMOVE;
}

static void on_stream_chunk(const gchar *chunk, gboolean is_final, gpointer user_data)
{
    (void)user_data;
    (void)is_final;

    if (!chunk || !*chunk) return;

    if (!stream_response)
        stream_response = g_string_new("");
    g_string_append(stream_response, chunk);

    StreamIdleData *data = g_new0(StreamIdleData, 1);
    data->chunk = g_strdup(chunk);
    data->is_final = FALSE;
    g_idle_add(stream_idle_callback, data);
}

static void on_stream_final(const gchar *response, gpointer user_data)
{
    (void)user_data;

    StreamIdleData *data = g_new0(StreamIdleData, 1);
    data->is_final = TRUE;
    data->full_response = response ? g_strdup(response) : NULL;
    if (stream_response) {
        g_string_free(stream_response, TRUE);
        stream_response = NULL;
    }
    g_idle_add(stream_idle_callback, data);
}

void lumila_chat_init(void)
{
    messages = g_array_new(FALSE, FALSE, sizeof(LumilaMessage));

    // Get default provider from config
    current_provider_id = lumila_config_get_default_provider();

    const LumilaModelEntry *entry = lumila_model_registry_lookup(current_provider_id);

    current_provider = lumila_provider_create(entry->type);
    current_provider->model_id = entry->model_index;

    // Create history directory
    const gchar *config_dir = geany->app->configdir;
    history_dir = g_build_filename(config_dir, "plugins", "lumila-ai", "history", NULL);
    g_mkdir_with_parents(history_dir, 0755);

    // Generate conversation ID
    GDateTime *now = g_date_time_new_now_local();
    conversation_id = g_date_time_format(now, "conversation-%Y%m%d-%H%M%S");
    g_date_time_unref(now);
}

void lumila_chat_cleanup(void)
{
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

    if (current_provider) {
        lumila_provider_free(current_provider);
        current_provider = NULL;
    }

    if (stream_response) {
        g_string_free(stream_response, TRUE);
        stream_response = NULL;
    }

    g_free(conversation_id);
    g_free(history_dir);
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

static gchar *get_open_files_context(void)
{
    GString *context = g_string_new("");
    GeanyDocument *doc;
    gint i = 0;

    while ((doc = document_index(i)) != NULL) {
        if (doc->file_name && doc->editor && doc->editor->sci) {
            gchar *filename = g_path_get_basename(doc->file_name);
            const gchar *content = sci_get_contents(doc->editor->sci, -1);

            if (content && *content) {
                g_string_append_printf(context, "\n\n[Archivo abierto: %s]\n```\n%s\n```\n",
                                       filename, content);
            }

            g_free(filename);
        }
        i++;
    }

    gchar *result = g_string_free(context, FALSE);
    return result;
}

static gchar *apply_file_edits(const gchar *response)
{
    if (!response) return NULL;

    GString *cleaned = g_string_new("");
    GString *edit_summary = g_string_new("");
    gchar **lines = g_strsplit(response, "\n", -1);
    gboolean in_file_block = FALSE;
    GString *file_code = NULL;
    gchar *file_name = NULL;
    gint edits_count = 0;

    for (gint i = 0; lines[i] != NULL; i++) {
        const gchar *line = lines[i];

        if (!in_file_block && g_str_has_prefix(line, "```file:")) {
            in_file_block = TRUE;
            file_name = g_strdup(line + 8);
            g_strstrip(file_name);
            file_code = g_string_new("");
            continue;
        }

        if (in_file_block && g_str_has_prefix(line, "```") && !g_str_has_prefix(line, "```file:")) {
            in_file_block = FALSE;

            GeanyDocument *target_doc = NULL;
            GeanyDocument *doc;
            gint j = 0;

            while ((doc = document_index(j)) != NULL) {
                if (doc->file_name && doc->editor && doc->editor->sci) {
                    gchar *basename = g_path_get_basename(doc->file_name);
                    if (g_strcmp0(basename, file_name) == 0) {
                        target_doc = doc;
                        g_free(basename);
                        break;
                    }
                    g_free(basename);
                }
                j++;
            }

            if (target_doc) {
                sci_set_text(target_doc->editor->sci, file_code->str);
                g_string_append_printf(edit_summary, "\n✅ Edited: %s", file_name);
                edits_count++;
            } else {
                GeanyDocument *new_doc = document_new_file(NULL, NULL, NULL);
                if (new_doc && new_doc->editor && new_doc->editor->sci) {
                    sci_set_text(new_doc->editor->sci, file_code->str);
                    g_string_append_printf(edit_summary, "\n📄 Created: %s", file_name);
                    edits_count++;
                }
            }

            g_free(file_name);
            file_name = NULL;
            g_string_free(file_code, TRUE);
            file_code = NULL;
            continue;
        }

        if (in_file_block) {
            if (file_code->len > 0) {
                g_string_append_c(file_code, '\n');
            }
            g_string_append(file_code, line);
        } else {
            if (cleaned->len > 0) {
                g_string_append_c(cleaned, '\n');
            }
            g_string_append(cleaned, line);
        }
    }

    g_strfreev(lines);
    g_free(file_name);
    if (file_code) g_string_free(file_code, TRUE);

    if (edits_count > 0) {
        if (cleaned->len > 0) {
            g_string_append(cleaned, "\n");
        }
        g_string_append(cleaned, edit_summary->str);
    }

    gchar *result = g_string_free(cleaned, FALSE);
    g_string_free(edit_summary, TRUE);
    return result;
}

static void on_response_received(const gchar *response, gpointer user_data)
{
    (void)user_data;

    if (response) {
        gchar *cleaned_response = ask_mode ? NULL : apply_file_edits(response);

        LumilaMessage msg = {
            .role = g_strdup("assistant"),
            .content = g_strdup(response),
            .timestamp = get_current_timestamp()
        };
        g_array_append_val(messages, msg);
        append_message_to_view("assistant", cleaned_response ? cleaned_response : response);
        g_free(cleaned_response);
    } else {
        append_message_to_view("assistant", "Error: Failed to get response");
    }

    lumila_sidebar_set_status(NULL);
    lumila_sidebar_set_input_sensitive(TRUE);
}

void lumila_chat_cancel_request(void)
{
    if (current_provider) {
        lumila_provider_cancel(current_provider);
    }

    lumila_sidebar_set_status(NULL);
    lumila_sidebar_set_input_sensitive(TRUE);
}

static gchar *build_history_context(void)
{
    if (!messages || messages->len == 0) return g_strdup("");

    GString *history = g_string_new("");

    /* Do not include the very last message (the one being sent now) */
    for (guint i = 0; i < messages->len - 1; i++) {
        LumilaMessage *msg = &g_array_index(messages, LumilaMessage, i);
        if (g_str_equal(msg->role, "user")) {
            g_string_append_printf(history, "User: %s\n", msg->content);
        } else {
            g_string_append_printf(history, "Assistant: %s\n", msg->content);
        }
    }

    return g_string_free(history, FALSE);
}

static gchar *get_current_file_content(void)
{
    GeanyDocument *doc = document_get_current();
    if (!doc || !doc->editor || !doc->editor->sci) return g_strdup("");
    const gchar *content = sci_get_contents(doc->editor->sci, -1);
    gchar *filename = doc->file_name ? g_path_get_basename(doc->file_name) : g_strdup("current");
    gchar *result = g_strdup_printf("[File: %s]\n```\n%s\n```", filename, content ? content : "");
    g_free(filename);
    return result;
}

static gchar *get_git_diff(void)
{
    gchar *output = NULL;
    gint exit_status = 0;
    g_spawn_command_line_sync("git diff --cached", &output, NULL, &exit_status, NULL);
    if (exit_status != 0 || !output || !*output) {
        g_free(output);
        g_spawn_command_line_sync("git diff", &output, NULL, &exit_status, NULL);
    }
    if (!output || !*output) {
        g_free(output);
        return g_strdup("No changes detected.");
    }
    return output;
}

static gchar *extract_references(const gchar *message)
{
    GString *refs = g_string_new("");
    const gchar *p = message;

    while ((p = strchr(p, '@')) != NULL) {
        p++;
        const gchar *start = p;
        while (*p && !g_ascii_isspace(*p) && *p != '\n' && *p != '\r') p++;
        if (p == start) continue;

        gchar *filename = g_strndup(start, p - start);
        gchar *content = NULL;
        gsize len = 0;

        if (!g_file_get_contents(filename, &content, &len, NULL)) {
            GeanyDocument *doc = document_get_current();
            if (doc && doc->file_name) {
                gchar *dir = g_path_get_dirname(doc->file_name);
                gchar *path = g_build_filename(dir, filename, NULL);
                g_file_get_contents(path, &content, &len, NULL);
                g_free(path);
                g_free(dir);
            }
        }

        if (content) {
            g_string_append_printf(refs, "\n\n[Referenced file: %s]\n```\n%s\n```", filename, content);
            g_free(content);
        }
        g_free(filename);
    }

    return g_string_free(refs, FALSE);
}

static gchar *process_slash_command(const gchar *message)
{
    if (!message || message[0] != '/') return g_strdup(message);

    const gchar *cmd = message + 1;
    while (*cmd == ' ') cmd++;

    if (g_str_has_prefix(cmd, "explain")) {
        const gchar *rest = cmd + 7;
        while (*rest == ' ') rest++;
        gchar *file_ctx = get_current_file_content();
        gchar *result = g_strdup_printf("Explain the following code in detail:\n\n%s\n\nSpecific question: %s", file_ctx, rest);
        g_free(file_ctx);
        return result;
    }
    if (g_str_has_prefix(cmd, "refactor")) {
        const gchar *rest = cmd + 8;
        while (*rest == ' ') rest++;
        gchar *file_ctx = get_current_file_content();
        gchar *result = g_strdup_printf("Refactor the following code to improve readability and performance. Keep the same functionality:\n\n%s\n\nSpecific request: %s", file_ctx, rest);
        g_free(file_ctx);
        return result;
    }
    if (g_str_has_prefix(cmd, "test")) {
        const gchar *rest = cmd + 4;
        while (*rest == ' ') rest++;
        gchar *file_ctx = get_current_file_content();
        gchar *result = g_strdup_printf("Write comprehensive unit tests for the following code:\n\n%s\n\nSpecific request: %s", file_ctx, rest);
        g_free(file_ctx);
        return result;
    }
    if (g_str_has_prefix(cmd, "doc")) {
        const gchar *rest = cmd + 3;
        while (*rest == ' ') rest++;
        gchar *file_ctx = get_current_file_content();
        gchar *result = g_strdup_printf("Generate documentation (docstrings/comments) for the following code:\n\n%s\n\nSpecific request: %s", file_ctx, rest);
        g_free(file_ctx);
        return result;
    }
    if (g_str_has_prefix(cmd, "fix")) {
        const gchar *rest = cmd + 3;
        while (*rest == ' ') rest++;
        gchar *file_ctx = get_current_file_content();
        gchar *result = g_strdup_printf("Find and fix any bugs, issues or compilation errors in the following code. Explain what was wrong and provide the corrected version:\n\n%s\n\nSpecific issue: %s", file_ctx, rest);
        g_free(file_ctx);
        return result;
    }
    if (g_str_has_prefix(cmd, "commit")) {
        gchar *diff = get_git_diff();
        gchar *result = g_strdup_printf("Generate a concise, conventional commit message for these changes. Use the format: type(scope): description\n\nChanges:\n```diff\n%s\n```", diff);
        g_free(diff);
        return result;
    }
    if (g_str_has_prefix(cmd, "review")) {
        const gchar *rest = cmd + 6;
        while (*rest == ' ') rest++;
        gchar *file_ctx = get_current_file_content();
        gchar *result = g_strdup_printf("Review the following code for bugs, security issues, performance problems and code quality. Provide specific, actionable suggestions:\n\n%s\n\nFocus areas: %s", file_ctx, rest);
        g_free(file_ctx);
        return result;
    }

    return g_strdup(message);
}

void lumila_chat_set_ask_mode(gboolean enabled)
{
    ask_mode = enabled;
}

static gchar *build_system_prompt(void)
{
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

void lumila_chat_send_message(const gchar *message)
{
    if (!message || !*message) return;

    // Extract @references
    gchar *refs = extract_references(message);

    // Process slash commands
    gchar *processed = process_slash_command(message);

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
            first_line[60] = '\0';
            strcat(first_line, "...");
        }
        g_free(conversation_title);
        conversation_title = first_line;
    }

    // Get context from open files
    gchar *files_context = get_open_files_context();

    // Build conversation history context
    gchar *history = build_history_context();

    // Build complete message with system prompt, history, file context, references and processed message
    gchar *complete_message;
    gchar *sys_prompt = build_system_prompt();

    GString *parts = g_string_new(sys_prompt);
    if (history && *history) {
        g_string_append_printf(parts, "%s\n", history);
    }
    if (files_context && *files_context) {
        g_string_append_printf(parts, "%s\n", files_context);
    }
    if (refs && *refs) {
        g_string_append_printf(parts, "%s\n", refs);
    }
    g_string_append(parts, processed);

    complete_message = g_string_free(parts, FALSE);

    g_free(sys_prompt);
    g_free(processed);
    g_free(refs);

    // Send to provider
    if (current_provider) {
        lumila_sidebar_set_status(_("Thinking..."));
        lumila_sidebar_set_input_sensitive(FALSE);
        if (current_provider->send_message_stream) {
            lumila_chat_ui_stream_start(chat_view);
            lumila_provider_send_message_stream(current_provider, complete_message,
                                                   on_stream_chunk, on_stream_final, NULL);
        } else {
            lumila_provider_send_message(current_provider, complete_message, on_response_received, NULL);
        }
    }

    g_free(complete_message);
    g_free(files_context);
    g_free(history);
}

void lumila_chat_send_selection(void)
{
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

    lumila_chat_send_message(message);

    g_free(message);
    g_free(selection);
    g_free(filename);
}

void lumila_chat_send_current_file(void)
{
    GeanyDocument *doc = document_get_current();
    if (!doc || !doc->editor || !doc->editor->sci) {
        append_message_to_view("system", "Error: No active document");
        return;
    }

    const gchar *content = sci_get_contents(doc->editor->sci, -1);
    if (!content || !*content) {
        append_message_to_view("system", "Error: File is empty");
        return;
    }

    gchar *filename = g_path_get_basename(doc->file_name ? doc->file_name : "untitled");
    gchar *message = g_strdup_printf("[Full content of %s]\n```\n%s\n```\n", filename, content);

    lumila_chat_send_message(message);

    g_free(message);
    g_free(filename);
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

void lumila_chat_set_provider(gint provider_id)
{
    current_provider_id = provider_id;

    if (current_provider) {
        lumila_provider_free(current_provider);
    }

    const LumilaModelEntry *entry = lumila_model_registry_lookup(provider_id);

    current_provider = lumila_provider_create(entry->type);
    current_provider->model_id = entry->model_index;
}

void lumila_chat_save_history(void)
{
    if (!messages || messages->len == 0 || !history_dir || !conversation_id) return;

    // Create JSON structure
    json_t *root = json_object();
    json_object_set_new(root, "id", json_string(conversation_id));

    GDateTime *now = g_date_time_new_now_local();
    gchar *created_at = g_date_time_format(now, "%Y-%m-%d %H:%M:%S");
    json_object_set_new(root, "created_at", json_string(created_at));
    g_free(created_at);
    g_date_time_unref(now);

    json_object_set_new(root, "provider_id", json_integer(current_provider_id));
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
    json_dump_file(root, filepath, JSON_INDENT(2));

    g_free(filename);
    g_free(filepath);
    json_decref(root);
}

void lumila_chat_new_conversation(void)
{
    // Save current conversation
    lumila_chat_save_history();

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
    GDateTime *now = g_date_time_new_now_local();
    conversation_id = g_date_time_format(now, "conversation-%Y%m%d-%H%M%S");
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
    conversation_id = g_strndup(filename, strlen(filename) - 5); // remove .json

    // Restore title from history
    g_free(conversation_title);
    conversation_title = lumila_history_load_title(filename);
}
