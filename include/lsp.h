#ifndef LSP_H
#define LSP_H

#include <stddef.h>

typedef struct {
    int    pid;
    int    in_fd;
    int    out_fd;
    int    active;
    int    next_id;
    char  *rx_buf;
    size_t rx_len;
    size_t rx_cap;
} LspClient;

LspClient *lspStart(const char *server_cmd[]);
void       lspStop(LspClient *c);
char      *lspRead(LspClient *c);
void       lspSend(LspClient *c, const char *message);

#endif
