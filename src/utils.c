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
        /* Fence markers may arrive indented; detect them on the trimmed
         * line but keep the original when it is file content. */
        const gchar *trim = line;
        while (*trim == ' ' || *trim == '\t') trim++;

        if (!in_file_block && g_str_has_prefix(trim, "```file:")) {
            in_file_block = TRUE;
            fence_depth = 0;
            file_name = g_strdup(trim + 8);
            g_strstrip(file_name);
            file_code = g_string_new("");
            continue;
        }

        if (in_file_block && g_str_has_prefix(trim, "```")) {
            gboolean opens_file = g_str_has_prefix(trim, "```file:");
            /* A bare "```" (optionally followed by whitespace) closes a
             * fence; "```lang" opens a nested one. */
            gboolean bare_close = !opens_file &&
                (trim[3] == '\0' || trim[3] == ' ' || trim[3] == '\t');
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
    /* API-level transient failures (e.g. Anthropic "overloaded_error"
     * mid-stream, rate limits reported in the body). */
    if (g_str_has_prefix(text, "API Error:") &&
        (strstr(text, "overloaded") || strstr(text, "rate limit") ||
         strstr(text, "rate_limit") || strstr(text, "temporarily") ||
         strstr(text, "capacity"))) {
        return TRUE;
    }
    return FALSE;
}

/* --- Line diff --- */

/* Cap for the LCS dynamic table (cells). Above this we fall back to
 * marking the whole differing region as del+ins, which is still a
 * correct (coarser) diff. */
#define DIFF_MAX_CELLS ((guint64)4 * 1024 * 1024)

static void diff_emit(GArray *ops, LumilaDiffKind kind, gint new_line,
                      const gchar *text)
{
    LumilaDiffOp op;
    op.kind = kind;
    op.new_line = new_line;
    op.text = g_strdup(text);
    g_array_append_val(ops, op);
}

/* Split into NUL-terminated lines without a trailing empty element.
 * g_strsplit produces a final "" when the text ends with '\n'; that
 * element is dropped here so line indices match the editor's. */
static gchar **diff_split_lines(const gchar *text)
{
    gchar **lines = g_strsplit(text ? text : "", "\n", -1);
    guint n = g_strv_length(lines);
    if (n > 0 && lines[n - 1][0] == '\0') {
        g_free(lines[n - 1]);
        lines[n - 1] = NULL;
    }
    return lines;
}

GArray *lumila_utils_diff_lines(const gchar *old_text, const gchar *new_text)
{
    GArray *ops = g_array_new(FALSE, FALSE, sizeof(LumilaDiffOp));
    gchar **o = diff_split_lines(old_text);
    gchar **n = diff_split_lines(new_text);
    const gint olen = (gint)g_strv_length(o);
    const gint nlen = (gint)g_strv_length(n);

    /* Common prefix. */
    gint p = 0;
    while (p < olen && p < nlen && g_strcmp0(o[p], n[p]) == 0) p++;
    /* Common suffix (without overlapping the prefix). */
    gint s = 0;
    while (s < (olen - p) && s < (nlen - p) &&
           g_strcmp0(o[olen - 1 - s], n[nlen - 1 - s]) == 0) s++;

    for (gint i = 0; i < p; i++)
        diff_emit(ops, LUMILA_DIFF_KEEP, i, o[i]);

    const gint omid = olen - p - s;   /* differing lines in old */
    const gint nmid = nlen - p - s;   /* differing lines in new */

    if ((guint64)omid * (guint64)nmid > DIFF_MAX_CELLS) {
        /* Too large for the DP table: coarse fallback. */
        for (gint i = 0; i < omid; i++)
            diff_emit(ops, LUMILA_DIFF_DEL, -1, o[p + i]);
        for (gint i = 0; i < nmid; i++)
            diff_emit(ops, LUMILA_DIFF_INS, p + i, n[p + i]);
    } else {
        /* LCS over the differing region. dp[i][j] = LCS length of
         * o[p+i..] vs n[p+j..]; built bottom-up, table width nmid+1. */
        guint32 *dp = g_new0(guint32, (gsize)(omid + 1) * (gsize)(nmid + 1));
        const gint w = nmid + 1;
        for (gint i = omid - 1; i >= 0; i--) {
            for (gint j = nmid - 1; j >= 0; j--) {
                if (g_strcmp0(o[p + i], n[p + j]) == 0)
                    dp[i * w + j] = dp[(i + 1) * w + (j + 1)] + 1;
                else
                    dp[i * w + j] = MAX(dp[(i + 1) * w + j],
                                        dp[i * w + (j + 1)]);
            }
        }
        /* Walk the table emitting ops: prefer emitting DEL before INS
         * so a modified line renders as "old removed + new added". */
        gint i = 0, j = 0;
        while (i < omid && j < nmid) {
            if (g_strcmp0(o[p + i], n[p + j]) == 0) {
                diff_emit(ops, LUMILA_DIFF_KEEP, p + j, o[p + i]);
                i++; j++;
            } else if (dp[(i + 1) * w + j] >= dp[i * w + (j + 1)]) {
                diff_emit(ops, LUMILA_DIFF_DEL, -1, o[p + i]);
                i++;
            } else {
                diff_emit(ops, LUMILA_DIFF_INS, p + j, n[p + j]);
                j++;
            }
        }
        while (i < omid) {
            diff_emit(ops, LUMILA_DIFF_DEL, -1, o[p + i]);
            i++;
        }
        while (j < nmid) {
            diff_emit(ops, LUMILA_DIFF_INS, p + j, n[p + j]);
            j++;
        }
        g_free(dp);
    }

    for (gint i = 0; i < s; i++)
        diff_emit(ops, LUMILA_DIFF_KEEP, nlen - s + i, o[olen - s + i]);

    g_strfreev(o);
    g_strfreev(n);
    return ops;
}

void lumila_utils_diff_ops_free(GArray *ops)
{
    if (!ops) return;
    for (guint i = 0; i < ops->len; i++) {
        LumilaDiffOp *op = &g_array_index(ops, LumilaDiffOp, i);
        g_free(op->text);
    }
    g_array_free(ops, TRUE);
}
