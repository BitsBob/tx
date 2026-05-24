#ifndef LSP_H
#define LSP_H

#include <stddef.h>

typedef struct {
    int    pid;      /* server process id */
    int    in_fd;    /* we write to the server here */
    int    out_fd;   /* we read from the server here (non-blocking) */
    int    active;   /* 0 once the server exits or an I/O error occurs */
    int    next_id;  /* monotonically increasing request id counter */
    char  *rx_buf;   /* receive buffer for partial messages */
    size_t rx_len;   /* bytes currently in rx_buf */
    size_t rx_cap;   /* allocated size of rx_buf */
} LspClient;

LspClient *lspStart(const char *server_cmd[]);
void       lspStop(LspClient *c);
char      *lspRead(LspClient *c);
void       lspSend(LspClient *c, const char *message);

#endif
