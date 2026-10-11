#include "utils.h"
#include <string.h>

gchar *lumila_utils_escape_markup(const gchar *text)
{
    if (!text) return NULL;
    return g_markup_escape_text(text, -1);
}

gchar *lumila_utils_format_code_block(const gchar *language, const gchar *code)
{
    if (!code) return NULL;

    gchar *escaped = lumila_utils_escape_markup(code);
    gchar *result = g_strdup_printf("<code language=\"%s\">%s</code>",
                                     language ? language : "",
                                     escaped);
    g_free(escaped);
    return result;
}

GList *lumila_utils_parse_file_edits(const gchar *response, gchar **cleaned_out)
{
    if (cleaned_out) {
        *cleaned_out = NULL;
    }
    if (!response) {
        return NULL;
    }

    GString *cleaned = cleaned_out ? g_string_new("") : NULL;
    GList *edits = NULL;
    gchar **lines = g_strsplit(response, "\n", -1);
    gboolean in_file_block = FALSE;
    gint fence_depth = 0;
    GString *file_code = NULL;
    gchar *file_name = NULL;

    for (gint i = 0; lines[i] != NULL; i++) {
        const gchar *line = lines[i];

        if (!in_file_block && g_str_has_prefix(line, "```file:")) {
            in_file_block = TRUE;
            fence_depth = 0;
            file_name = g_strdup(line + 8);
            g_strstrip(file_name);
            file_code = g_string_new("");
            continue;
        }

        if (in_file_block && g_str_has_prefix(line, "```")) {
            gboolean opens_file = g_str_has_prefix(line, "```file:");
            /* A bare "```" (optionally followed by whitespace) closes a
             * fence; "```lang" opens a nested one. */
            gboolean bare_close = !opens_file &&
                (line[3] == '\0' || line[3] == ' ' || line[3] == '\t');
            if (bare_close && fence_depth == 0) {
                in_file_block = FALSE;
                if (file_name && *file_name) {
                    LumilaFileEdit *edit = g_new0(LumilaFileEdit, 1);
                    edit->file_name = g_strdup(file_name);
                    edit->content = g_strdup(file_code->str);
                    edits = g_list_append(edits, edit);
                }
                g_free(file_name);
                file_name = NULL;
                g_string_free(file_code, TRUE);
                file_code = NULL;
                continue;
            }
            /* Fence marker inside the file block: it is content too. */
            fence_depth += bare_close ? -1 : +1;
        }

        if (in_file_block) {
            if (file_code->len > 0) {
                g_string_append_c(file_code, '\n');
            }
            g_string_append(file_code, line);
        } else if (cleaned) {
            if (cleaned->len > 0) {
                g_string_append_c(cleaned, '\n');
            }
            g_string_append(cleaned, line);
        }
    }

    g_strfreev(lines);
    g_free(file_name);
    if (file_code) {
        g_string_free(file_code, TRUE);
    }

    if (cleaned) {
        *cleaned_out = g_string_free(cleaned, FALSE);
    }
    return edits;
}

void lumila_utils_file_edits_free(GList *edits)
{
    for (GList *l = edits; l; l = l->next) {
        LumilaFileEdit *edit = l->data;
        g_free(edit->file_name);
        g_free(edit->content);
        g_free(edit);
    }
    g_list_free(edits);
}

gboolean lumila_utils_is_error_response(const gchar *text)
{
    if (!text) return TRUE;
    return g_str_has_prefix(text, "Error:") ||
           g_str_has_prefix(text, "API Error:") ||
           g_str_has_prefix(text, "HTTP Error") ||
           g_str_has_prefix(text, "Transient HTTP Error") ||
           g_str_has_prefix(text, "JSON Parse Error");
}

gboolean lumila_utils_is_transient_response(const gchar *text)
{
    if (!text) return FALSE;
    if (g_str_has_prefix(text, "Transient HTTP Error")) return TRUE;
    /* Transport-level transient failures surface as plain "Error: ..." */
    if (g_str_has_prefix(text, "Error:") &&
        (strstr(text, "timed out") || strstr(text, "Timeout") ||
         strstr(text, "temporarily") || strstr(text, "Connection"))) {
        return TRUE;
    }
    return FALSE;
}
