#include <geanyplugin.h>
#include "plugin.h"
#include "sidebar.h"
#include "chat.h"
#include "config.h"

GeanyPlugin *geany_plugin;
GeanyData *geany_data;

PLUGIN_VERSION_CHECK(247)

PLUGIN_SET_INFO("Lumila AI", 
                "AI Assistant for Geany - Multi-provider chat",
                "0.1.0",
                "Your Name")

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
    plugin->info->name = "Lumila AI";
    plugin->info->description = "AI Assistant for Geany - Multi-provider chat";
    plugin->info->version = "0.1.0";
    plugin->info->author = "Iván Gabriel Sosa";

    plugin->funcs->init = lumila_init;
    plugin->funcs->cleanup = lumila_cleanup;

    GEANY_PLUGIN_REGISTER(plugin, 247);
}
