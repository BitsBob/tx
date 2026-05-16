#ifndef LSP_H
#define LSP_H

typedef struct {
    int pid;
    int in_fd;
    int out_fd;
    int active;
    int next_id;
} LspClient;

LspClient *lspStart(const char *server_cmd[]);
void       lspStop(LspClient *c);
char      *lspRead(LspClient *c);
void       lspSend(LspClient *c, const char *message);

#endif
