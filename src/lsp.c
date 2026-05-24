#include "tx.h"
#include "lsp.h"

#include <sys/types.h>
#include <sys/wait.h>

/* Fork the language server as a child process connected to us via two pipes.
 * The child's stdin/stdout become those pipes; we get file descriptors for
 * writing to it (in_fd) and reading from it (out_fd).
 * out_fd is set non-blocking so lspRead never stalls the editor. */
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
        /* Child: rewire stdio then exec the server.
         * Stderr is silenced so server log output doesn't corrupt the display. */
        dup2(to_server[0], STDIN_FILENO);
        dup2(from_server[1], STDOUT_FILENO);

        int devnull = open("/dev/null", O_WRONLY);
        if (devnull != -1) {
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }

        close(to_server[0]);   close(to_server[1]);
        close(from_server[0]); close(from_server[1]);

        execvp(server_cmd[0], (char *const *)server_cmd);
        _exit(1);
    }

    /* Parent: keep only the write end of to_server and the read end of from_server. */
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
    c->rx_buf  = NULL;
    c->rx_len  = 0;
    c->rx_cap  = 0;

    int fl = fcntl(c->out_fd, F_GETFL, 0);
    if (fl != -1)
        fcntl(c->out_fd, F_SETFL, fl | O_NONBLOCK);

    return c;
}

void lspStop(LspClient *c) {
    if (!c) return;
    if (c->active) {
        kill(c->pid, SIGTERM);
        close(c->in_fd);
        close(c->out_fd);
    }
    free(c->rx_buf);
    free(c);
}

/* Write a JSON-RPC message to the server.
 * LSP framing is a Content-Length header followed by a blank line, then the body. */
void lspSend(LspClient *c, const char *message) {
    if (!c || !c->active || !message) return;

    size_t body_len = strlen(message);
    char   header[64];
    int    hlen = snprintf(header, sizeof(header),
                           "Content-Length: %zu\r\n\r\n", body_len);
    if (hlen < 0 || (size_t)hlen >= sizeof(header)) return;

    const char *parts[2] = { header, message };
    size_t      lens[2]  = { (size_t)hlen, body_len };

    for (int i = 0; i < 2; i++) {
        const char *p = parts[i];
        size_t remaining = lens[i];
        while (remaining > 0) {
            ssize_t n = write(c->in_fd, p, remaining);
            if (n < 0) {
                if (errno == EINTR) continue;
                return;
            }
            p += n;
            remaining -= (size_t)n;
        }
    }
}

/* Grow the receive buffer to have at least `headroom` bytes free. */
static int rx_reserve(LspClient *c, size_t headroom) {
    if (c->rx_cap - c->rx_len >= headroom) return 0;
    size_t new_cap = c->rx_cap ? c->rx_cap : 8192;
    while (new_cap - c->rx_len < headroom) new_cap *= 2;
    char *nb = realloc(c->rx_buf, new_cap);
    if (!nb) return -1;
    c->rx_buf = nb;
    c->rx_cap = new_cap;
    return 0;
}

/* Read and return the next complete JSON-RPC message body, or NULL if no
 * complete message is buffered yet. The caller owns the returned string.
 * Partial data stays in rx_buf for the next call. Because out_fd is
 * non-blocking, this returns quickly when the server has nothing new. */
char *lspRead(LspClient *c) {
    if (!c || !c->active) return NULL;

    /* Pull everything currently readable into the buffer. */
    for (;;) {
        if (rx_reserve(c, 4096) != 0) break;
        ssize_t n = read(c->out_fd, c->rx_buf + c->rx_len,
                         c->rx_cap - c->rx_len);
        if (n > 0) {
            c->rx_len += (size_t)n;
            continue;
        }
        if (n == 0) { c->active = 0; break; }
        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
        if (errno == EINTR) continue;
        c->active = 0;
        break;
    }

    if (c->rx_len == 0) return NULL;

    /* Look for the blank line that terminates the headers. */
    char *sep = memmem(c->rx_buf, c->rx_len, "\r\n\r\n", 4);
    if (!sep) return NULL;

    size_t header_len = (size_t)(sep - c->rx_buf);
    size_t body_start = header_len + 4;

    char *cl = memmem(c->rx_buf, header_len, "Content-Length:", 15);
    if (!cl) return NULL;
    cl += 15;

    char *endptr = NULL;
    long  clen   = strtol(cl, &endptr, 10);
    if (endptr == cl || clen <= 0) return NULL;

    /* Wait until the full body has arrived. */
    if (c->rx_len - body_start < (size_t)clen) return NULL;

    char *body = malloc((size_t)clen + 1);
    if (!body) return NULL;
    memcpy(body, c->rx_buf + body_start, (size_t)clen);
    body[clen] = '\0';

    /* Slide any remaining data to the front of the buffer. */
    size_t consumed = body_start + (size_t)clen;
    size_t leftover = c->rx_len - consumed;
    if (leftover > 0)
        memmove(c->rx_buf, c->rx_buf + consumed, leftover);
    c->rx_len = leftover;

    return body;
}
