#ifndef LUMILA_UTILS_H
#define LUMILA_UTILS_H

#include <glib.h>

gchar *lumila_utils_escape_markup(const gchar *text);
gchar *lumila_utils_format_code_block(const gchar *language, const gchar *code);

typedef struct {
    gchar *file_name;
    gchar *content;
} LumilaFileEdit;

/* Parse ```file:name blocks out of a model response.
 * Nested fenced code blocks inside the file content (e.g. when editing a
 * markdown file) are handled by tracking fence depth: a "```lang" line
 * inside the block opens a nested fence, a bare "```" line closes it.
 * Returns a GList of LumilaFileEdit* (free with
 * lumila_utils_file_edits_free). If cleaned_out is non-NULL it receives
 * the response text with all file blocks removed. */
GList *lumila_utils_parse_file_edits(const gchar *response, gchar **cleaned_out);
void lumila_utils_file_edits_free(GList *edits);

/* TRUE when the text is an error string produced by the providers
 * (should not be persisted in the conversation). */
gboolean lumila_utils_is_error_response(const gchar *text);

/* TRUE for transient errors that are worth retrying. */
gboolean lumila_utils_is_transient_response(const gchar *text);

/* --- Line diff for the inline diff preview --- */

typedef enum {
    LUMILA_DIFF_KEEP,
    LUMILA_DIFF_DEL,
    LUMILA_DIFF_INS
} LumilaDiffKind;

typedef struct {
    LumilaDiffKind kind;
    gint new_line;   /* index into the NEW content lines (KEEP/INS), -1 for DEL */
    gchar *text;     /* line content (owned by the GArray) */
} LumilaDiffOp;

/* Line-level diff between two texts. Returns a GArray of LumilaDiffOp in
 * output order (KEEP/DEL for old lines, INS for new lines). Free with
 * lumila_utils_diff_ops_free. */
GArray *lumila_utils_diff_lines(const gchar *old_text, const gchar *new_text);
void lumila_utils_diff_ops_free(GArray *ops);

#endif
