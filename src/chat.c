#include "chat.h"
#include "chat_ui.h"
#include "providers/provider.h"
#include "config.h"
#include "plugin.h"
#include <geanyplugin.h>
#include <string.h>
#include <time.h>
#include <jansson.h>

typedef struct {
    gchar *role;
    gchar *content;
    gchar *timestamp;
} ChatMessage;

static GtkTextView *chat_view = NULL;
static GArray *messages = NULL;
static LumilaProvider *current_provider = NULL;
static gint current_provider_id = 0;
static gchar *conversation_id = NULL;
static gchar *history_dir = NULL;

void lumila_chat_init(void)
{
    messages = g_array_new(FALSE, FALSE, sizeof(ChatMessage));

    // Get default provider from config
    current_provider_id = lumila_config_get_default_provider();

    // Map provider_id to type and model_id
    LumilaProviderType type;
    gint model_id = 0;

    switch (current_provider_id) {
        case 0: type = LUMILA_PROVIDER_ANTHROPIC; model_id = 0; break;
        case 1: type = LUMILA_PROVIDER_ANTHROPIC; model_id = 1; break;
        case 2: type = LUMILA_PROVIDER_ANTHROPIC; model_id = 2; break;
        case 3: type = LUMILA_PROVIDER_ANTHROPIC; model_id = 3; break;
        case 4: type = LUMILA_PROVIDER_OPENAI; model_id = 0; break;
        case 5: type = LUMILA_PROVIDER_OPENAI; model_id = 1; break;
        case 6: type = LUMILA_PROVIDER_GOOGLE; model_id = 0; break;
        case 7: type = LUMILA_PROVIDER_GOOGLE; model_id = 1; break;
        case 8: type = LUMILA_PROVIDER_GOOGLE; model_id = 2; break;
        case 9: type = LUMILA_PROVIDER_OLLAMA; model_id = 0; break;
        case 10: type = LUMILA_PROVIDER_OLLAMA; model_id = 1; break;
        case 11: type = LUMILA_PROVIDER_OPENROUTER; model_id = 0; break;
        default: type = LUMILA_PROVIDER_OPENROUTER; model_id = 0; break;
    }

    current_provider = lumila_provider_create(type);
    current_provider->model_id = model_id;

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
            ChatMessage *msg = &g_array_index(messages, ChatMessage, i);
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

    g_free(conversation_id);
    g_free(history_dir);
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
    GeanyDocument *doc = document_get_current();

    if (doc && doc->file_name) {
        // Get filename
        const gchar *filename = g_path_get_basename(doc->file_name);

        // Get file content
        const gchar *content = sci_get_contents(doc->editor->sci, -1);

        if (content && *content) {
            g_string_append_printf(context, "\n\n[Archivo abierto: %s]\n```\n%s\n```\n", 
                                   filename, content);
        }
    }

    gchar *result = g_string_free(context, FALSE);
    return result;
}

static void on_response_received(const gchar *response, gpointer user_data)
{
    (void)user_data;

    if (response) {
        ChatMessage msg = {
            .role = g_strdup("assistant"),
            .content = g_strdup(response),
            .timestamp = get_current_timestamp()
        };
        g_array_append_val(messages, msg);
        append_message_to_view("assistant", response);
    } else {
        append_message_to_view("assistant", "Error: Failed to get response");
    }
}

void lumila_chat_send_message(const gchar *message)
{
    if (!message || !*message) return;

    // Add user message
    ChatMessage msg = {
        .role = g_strdup("user"),
        .content = g_strdup(message),
        .timestamp = get_current_timestamp()
    };
    g_array_append_val(messages, msg);
    append_message_to_view("user", message);

    // Get context from open files
    gchar *files_context = get_open_files_context();

    // Build complete message with context
    gchar *complete_message;

    if (files_context && *files_context) {
        complete_message = g_strdup_printf("%s%s", message, files_context);
    } else {
        complete_message = g_strdup(message);
    }

    // Send to provider
    if (current_provider) {
        lumila_provider_send_message(current_provider, complete_message, on_response_received, NULL);
    }

    g_free(complete_message);
    g_free(files_context);
}

void lumila_chat_set_provider(gint provider_id)
{
    current_provider_id = provider_id;

    if (current_provider) {
        lumila_provider_free(current_provider);
    }

    LumilaProviderType type;
    gint model_id = 0;

    switch (provider_id) {
        case 0: type = LUMILA_PROVIDER_ANTHROPIC; model_id = 0; break;    // Claude Sonnet 4.5
        case 1: type = LUMILA_PROVIDER_ANTHROPIC; model_id = 1; break;    // Claude Sonnet 4.6
        case 2: type = LUMILA_PROVIDER_ANTHROPIC; model_id = 2; break;    // Claude Opus 4.5
        case 3: type = LUMILA_PROVIDER_ANTHROPIC; model_id = 3; break;    // Claude Opus 4.6
        case 4: type = LUMILA_PROVIDER_OPENAI; model_id = 0; break;       // GPT-5.2-Codex
        case 5: type = LUMILA_PROVIDER_OPENAI; model_id = 1; break;       // GPT-5.3-Codex
        case 6: type = LUMILA_PROVIDER_GOOGLE; model_id = 0; break;       // Gemini 3 Flash
        case 7: type = LUMILA_PROVIDER_GOOGLE; model_id = 1; break;       // Gemini 3 Pro
        case 8: type = LUMILA_PROVIDER_GOOGLE; model_id = 2; break;       // Gemini 3.1 Pro
        case 9: type = LUMILA_PROVIDER_OLLAMA; model_id = 0; break;       // Ollama Llama 3.2
        case 10: type = LUMILA_PROVIDER_OLLAMA; model_id = 1; break;      // Ollama Qwen 2.5
        case 11: type = LUMILA_PROVIDER_OPENROUTER; model_id = 0; break;  // OpenRouter
        default: type = LUMILA_PROVIDER_ANTHROPIC; model_id = 0; break;
    }

    current_provider = lumila_provider_create(type);
    current_provider->model_id = model_id;
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

    // Add messages array
    json_t *msgs_array = json_array();
    for (guint i = 0; i < messages->len; i++) {
        ChatMessage *msg = &g_array_index(messages, ChatMessage, i);
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
            ChatMessage *msg = &g_array_index(messages, ChatMessage, i);
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
    GDateTime *now = g_date_time_new_now_local();
    conversation_id = g_date_time_format(now, "conversation-%Y%m%d-%H%M%S");
    g_date_time_unref(now);
}
