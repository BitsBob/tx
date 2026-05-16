#include "tx.h"
#include "lsp.h"

#include <sys/types.h>
#include <sys/wait.h>

LspClient *lspStart(const char *server_cmd[]) {
    int to_server[2];
    int from_server[2];

    if (pipe(to_server) == -1 || pipe(from_server) == -1) {
        perror("lsp: pipe");
        return NULL;
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("lsp: fork");
        close(to_server[0]); close(to_server[1]);
        close(from_server[0]); close(from_server[1]);
        return NULL;
    } 
    
    if (pid == 0) {
        dup2(to_server[0], STDIN_FILENO);
        dup2(from_server[1], STDOUT_FILENO);

        close(to_server[0]);   close(to_server[1]);
        close(from_server[0]); close(from_server[1]);

        execvp(server_cmd[0], (char *const *)server_cmd);
        perror("lsp: execvp");
        _exit(1);
    }

    close(to_server[0]);
    close(from_server[1]);

    LspClient *c = malloc(sizeof(LspClient));
    if (!c) {
        close(to_server[1]);
        close(from_server[0]);
        kill(pid, SIGTERM);
        return NULL;
    }

    c->pid     = pid;
    c->in_fd   = to_server[1];
    c->out_fd  = from_server[0];
    c->active  = 1;
    c->next_id = 1;

    return c;
}

void lspStop(LspClient *c) {
    if (!c) return;
    if (c->active) {
        kill(c->pid, SIGTERM);
        close(c->in_fd);
        close(c->out_fd);
        free(c);
    }
    free(c);
}

void lspSend(LspClient *c, const char *json) {
    
}

char *lspRead(LspClient *c) {

}
