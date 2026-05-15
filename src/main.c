#include "../config.h"
#include <geanyplugin.h>
#include "plugin.h"
#include "sidebar.h"
#include "chat.h"
#include "config.h"

GeanyPlugin *geany_plugin;
GeanyData *geany_data;

PLUGIN_VERSION_CHECK(247)

PLUGIN_SET_INFO(LUMILA_NAME,
                LUMILA_DESCRIPTION,
                LUMILA_VERSION,
                LUMILA_AUTHOR)

static gboolean lumila_init(GeanyPlugin *plugin, gpointer pdata)
{
    (void)pdata;
    geany_plugin = plugin;
    geany_data = plugin->geany_data;

    lumila_config_init();
    lumila_chat_init();
    lumila_sidebar_init();

    return TRUE;
}

static void lumila_cleanup(GeanyPlugin *plugin, gpointer pdata)
{
    (void)plugin;
    (void)pdata;

    lumila_sidebar_cleanup();
    lumila_chat_cleanup();
    lumila_config_cleanup();
}

void geany_load_module(GeanyPlugin *plugin)
{
    geany_plugin = plugin;
    plugin->info->name = LUMILA_NAME;
    plugin->info->description = LUMILA_DESCRIPTION;
    plugin->info->version = LUMILA_VERSION;
    plugin->info->author = LUMILA_AUTHOR;

    plugin->funcs->init = lumila_init;
    plugin->funcs->cleanup = lumila_cleanup;

    GEANY_PLUGIN_REGISTER(plugin, 247);
}
