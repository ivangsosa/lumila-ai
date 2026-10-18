#ifndef LUMILA_DIFF_H
#define LUMILA_DIFF_H

#include <glib.h>

/* Inline diff preview for ```file: edits.
 *
 * lumila_diff_init wires the editor-notify guard that drops a decorated
 * document out of the preview when the user edits it mid-decision. */
void lumila_diff_init(void);
/*
 * lumila_diff_preview_begin decorates every previewable file (added
 * lines: green margin/background marker; deleted lines: red annotation
 * under the previous surviving line) and shows a NON-modal dialog with
 * Apply/Discard so the user can scroll and switch tabs while deciding.
 * Ownership of the edits list (GList of LumilaFileEdit) is transferred.
 *
 * lumila_diff_clear_all drops decorations and discards pending edits;
 * safe to call when nothing is pending. */
void lumila_diff_preview_begin(GList *edits);
void lumila_diff_clear_all(void);
gboolean lumila_diff_has_preview(void);
void lumila_diff_cleanup(void);

#endif
