#ifndef LUMILA_MESSAGE_H
#define LUMILA_MESSAGE_H

#include <glib.h>

typedef struct {
    gchar *role;
    gchar *content;
    gchar *timestamp;
} LumilaMessage;

#endif
