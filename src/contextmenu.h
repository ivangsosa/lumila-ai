#ifndef LUMILA_CONTEXTMENU_H
#define LUMILA_CONTEXTMENU_H

/* "Lumila AI" submenu in the editor right-click popup
 * (geany->main_widgets->editor_menu). Item sensitivity is updated on
 * every "update-editor-menu" signal. */
void lumila_context_menu_init(void);
void lumila_context_menu_cleanup(void);

#endif
