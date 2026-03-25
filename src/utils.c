#include "utils.h"

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
