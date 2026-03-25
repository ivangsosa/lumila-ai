#ifndef LUMILA_UTILS_H
#define LUMILA_UTILS_H

#include <glib.h>

gchar *lumila_utils_escape_markup(const gchar *text);
gchar *lumila_utils_format_code_block(const gchar *language, const gchar *code);

#endif
