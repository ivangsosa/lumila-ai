#include "../config.h"
#include <geanyplugin.h>
#include "plugin.h"
#include "sidebar.h"
#include "chat.h"
#include "config.h"
#include "diff.h"
#include "autocomplete.h"
#include "contextmenu.h"

GeanyPlugin *geany_plugin;
GeanyData *geany_data;

PLUGIN_VERSION_CHECK(247)

PLUGIN_SET_INFO(LUMILA_NAME,
                LUMILA_DESCRIPTION,
                LUMILA_VERSION,
                LUMILA_AUTHOR)

static gboolean on_lumila_key(guint key_id)
{
    switch (key_id) {
        case 0: lumila_chat_send_selection();    break;
        case 1: lumila_chat_send_current_file(); break;
        case 2: lumila_autocomplete_accept();    break;
    }
    return TRUE;
}

static gboolean lumila_init(GeanyPlugin *plugin, gpointer pdata)
{
    (void)pdata;
    geany_plugin = plugin;
    geany_data = plugin->geany_data;

    lumila_config_init();
    lumila_chat_init();
    lumila_sidebar_init();
    lumila_diff_init();
    lumila_autocomplete_init();
    lumila_context_menu_init();

    /* Editor keybindings (assignable in Geany's preferences). */
    GeanyKeyGroup *kg = plugin_set_key_group(plugin, "lumila-ai", 3,
                                             on_lumila_key);
    keybindings_set_item(kg, 0, NULL, 0, 0, "lumila_send_selection",
                         _("Lumila: Send selection to AI"), NULL);
    keybindings_set_item(kg, 1, NULL, 0, 0, "lumila_send_file",
                         _("Lumila: Send current file to AI"), NULL);
    keybindings_set_item(kg, 2, NULL, 0, 0, "lumila_accept_completion",
                         _("Lumila: Accept AI completion"), NULL);

    return TRUE;
}

static void lumila_cleanup(GeanyPlugin *plugin, gpointer pdata)
{
    (void)plugin;
    (void)pdata;

    lumila_context_menu_cleanup();
    lumila_autocomplete_cleanup();
    lumila_diff_cleanup();
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

    /* Async provider callbacks may fire after cleanup (cancelled soup
     * requests complete on the main loop). Keeping the module resident
     * guarantees those callbacks still point to valid code. */
    plugin_module_make_resident(plugin);

    GEANY_PLUGIN_REGISTER(plugin, 247);
}
