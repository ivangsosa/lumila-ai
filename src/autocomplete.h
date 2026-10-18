#ifndef LUMILA_AUTOCOMPLETE_H
#define LUMILA_AUTOCOMPLETE_H

#include <glib.h>

/* Ghost-text AI completion: after a typing debounce the module asks a
 * (configurable) model for a short continuation and shows it as a gray
 * end-of-line annotation. Accepted via the "Lumila: Accept AI
 * completion" keybinding or the editor context menu; dismissed
 * automatically on further typing or caret movement.
 *
 * lumila_autocomplete_suppress wraps programmatic buffer edits (file
 * edit apply, the accept insert itself) so they do not schedule
 * completion requests. */
void lumila_autocomplete_init(void);
void lumila_autocomplete_cleanup(void);
void lumila_autocomplete_accept(void);
void lumila_autocomplete_suppress(gboolean on);
gboolean lumila_autocomplete_has_pending(void);

#endif
