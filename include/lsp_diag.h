#ifndef LSP_DIAG_H
#define LSP_DIAG_H

/* Start LSP server and send initialize. No-op if CONFIG_LSP_ENABLE is false. */
void lspInit(void);

/* Drain pending LSP messages and dispatch them. Call once per frame. */
void lspPoll(void);

/* Notify LSP of a buffer being opened/saved/closed.
 * All are no-ops until the server completes the initialize handshake. */
void lspNotifyOpen(int buf_idx);
void lspNotifySave(int buf_idx);
void lspNotifyClose(int buf_idx);

/* Returns 1 if the LSP server is running and fully initialized. */
int lspIsReady(void);

#endif
