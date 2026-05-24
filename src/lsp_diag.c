#include "tx.h"
#include "editor.h"
#include "lsp.h"
#include "lsp_diag.h"
#include "lsp_json.h"
#include "lsp_msg.h"

#include <limits.h>

/* Server lifecycle: UNINIT -> INIT_SENT (after we write "initialize") -> READY
 * (after the server responds). Only READY state sends further notifications. */
enum { LSP_UNINIT = 0, LSP_INIT_SENT, LSP_READY };
static int lsp_state   = LSP_UNINIT;
static int lsp_init_id = -1;  /* id we used in the initialize request */

int lspIsReady(void) {
    return E.lsp && E.lsp->active && lsp_state == LSP_READY;
}

/* Map a filename extension to the LSP languageId string. */
static const char *language_id(const char *filename) {
    if (!filename) return "text";
    const char *dot = strrchr(filename, '.');
    if (!dot) return "text";
    if (!strcmp(dot, ".c")   || !strcmp(dot, ".h"))   return "c";
    if (!strcmp(dot, ".cpp") || !strcmp(dot, ".cc") ||
        !strcmp(dot, ".cxx") || !strcmp(dot, ".hpp")) return "cpp";
    if (!strcmp(dot, ".py"))  return "python";
    if (!strcmp(dot, ".js"))  return "javascript";
    if (!strcmp(dot, ".ts"))  return "typescript";
    if (!strcmp(dot, ".rs"))  return "rust";
    if (!strcmp(dot, ".go"))  return "go";
    return "text";
}

/* Flatten a buffer's rows into a single heap string for didOpen/didChange. */
static char *buffer_to_string(Buffer *b) {
    int total = 0;
    for (int i = 0; i < b->numrows; i++)
        total += b->row[i].size + 1;
    char *s = malloc(total + 1);
    if (!s) return NULL;
    char *p = s;
    for (int i = 0; i < b->numrows; i++) {
        memcpy(p, b->row[i].chars, b->row[i].size);
        p += b->row[i].size;
        *p++ = '\n';
    }
    *p = '\0';
    return s;
}

static void send_did_open(int idx) {
    Buffer *b = &E.buffers[idx];
    if (!b->filename) return;

    char *uri = lspBuildFileUri(b->filename);
    if (!uri) return;

    char *text = buffer_to_string(b);
    if (!text) { free(uri); return; }

    b->lsp_version = 1;
    char *msg = lspBuildDidOpen(uri, language_id(b->filename),
                                b->lsp_version, text);
    free(text);
    free(uri);
    if (msg) { lspSend(E.lsp, msg); free(msg); }
}

/* Called once per diagnostic element in the JSON array.
 * Stores the worst (lowest) severity seen for each line. */
static int diag_cb(const char *e_start, const char *e_end, void *ud) {
    Buffer *b = (Buffer *)ud;

    const char *range_v = jsonFindKey(e_start, e_end, "range");
    if (!range_v) return 0;
    const char *start_v = jsonFindKey(range_v, e_end, "start");
    if (!start_v) return 0;
    const char *line_v  = jsonFindKey(start_v, e_end, "line");
    if (!line_v) return 0;

    long line;
    if (jsonParseInt(line_v, e_end, &line, NULL) != 0) return 0;

    /* severity: 1=error 2=warning 3=info 4=hint. Defaults to error if absent. */
    long severity = 1;
    const char *sev_v = jsonFindKey(e_start, e_end, "severity");
    if (sev_v) jsonParseInt(sev_v, e_end, &severity, NULL);

    if (line >= 0 && line < b->diags_count) {
        if (b->diags[line] == 0 || severity < b->diags[line])
            b->diags[line] = (int)severity;
    }
    return 0;
}

/* Handle a textDocument/publishDiagnostics notification.
 * Matches the URI to an open buffer, resets that buffer's diags array,
 * then walks the diagnostics list and records severity per line. */
static void handle_publish_diagnostics(const char *msg, size_t len) {
    const char *end = msg + len;

    const char *params_v = jsonFindKey(msg, end, "params");
    if (!params_v) return;

    const char *uri_v = jsonFindKey(params_v, end, "uri");
    if (!uri_v) return;
    char *uri = jsonParseString(uri_v, end, NULL);
    if (!uri) return;

    int target = -1;
    for (int i = 0; i < E.buf_count; i++) {
        if (!E.buffers[i].filename) continue;
        char *buf_uri = lspBuildFileUri(E.buffers[i].filename);
        if (!buf_uri) continue;
        int match = (strcmp(buf_uri, uri) == 0);
        free(buf_uri);
        if (match) { target = i; break; }
    }
    free(uri);
    if (target < 0) return;

    Buffer *b = &E.buffers[target];

    /* Replace the whole diags array. diags_count tracks its length so the
     * gutter renderer stays safe if rows are added or removed before the
     * next publishDiagnostics arrives. */
    free(b->diags);
    int nrows = b->numrows > 0 ? b->numrows : 1;
    b->diags = calloc(nrows, sizeof(int));
    if (!b->diags) { b->diags_count = 0; return; }
    b->diags_count = b->numrows;

    const char *diags_v = jsonFindKey(params_v, end, "diagnostics");
    if (diags_v)
        jsonForEachArrayElem(diags_v, end, diag_cb, b);
}

/* Route an incoming JSON-RPC message. Notifications have a "method" key;
 * responses do not. We only care about publishDiagnostics and the
 * initialize response. */
static void handle_message(const char *msg) {
    size_t      len = strlen(msg);
    const char *end = msg + len;

    const char *method_v = jsonFindKey(msg, end, "method");
    if (method_v) {
        char *method = jsonParseString(method_v, end, NULL);
        if (method) {
            if (strcmp(method, "textDocument/publishDiagnostics") == 0)
                handle_publish_diagnostics(msg, len);
            free(method);
        }
        return;
    }

    /* Must be a response. If we're still waiting on initialize, check the id. */
    if (lsp_state == LSP_INIT_SENT) {
        const char *id_v = jsonFindKey(msg, end, "id");
        if (id_v) {
            long id;
            if (jsonParseInt(id_v, end, &id, NULL) == 0 && id == lsp_init_id) {
                char *notif = lspBuildInitialized();
                if (notif) { lspSend(E.lsp, notif); free(notif); }

                /* Now that the handshake is done, open all buffers. */
                for (int i = 0; i < E.buf_count; i++)
                    send_did_open(i);

                lsp_state = LSP_READY;
            }
        }
    }
}

/* Split CONFIG_LSP_CMD on whitespace, launch the server process, and
 * send the initialize request. The rest of the handshake happens in
 * lspPoll when the response arrives. */
void lspInit(void) {
    if (!E.settings.CONFIG_LSP_ENABLE || !E.settings.CONFIG_LSP_CMD[0]) return;

    char cmd_buf[256];
    strncpy(cmd_buf, E.settings.CONFIG_LSP_CMD, sizeof(cmd_buf) - 1);
    cmd_buf[sizeof(cmd_buf) - 1] = '\0';

    const char *argv[16];
    int argc_lsp = 0;
    char *tok = strtok(cmd_buf, " \t");
    while (tok && argc_lsp < 15) {
        argv[argc_lsp++] = tok;
        tok = strtok(NULL, " \t");
    }
    argv[argc_lsp] = NULL;
    if (argc_lsp == 0) return;

    E.lsp = lspStart(argv);
    if (!E.lsp) return;

    lsp_init_id = E.lsp->next_id++;
    char *init_msg = lspBuildInitialize(lsp_init_id, NULL);
    if (init_msg) {
        lspSend(E.lsp, init_msg);
        free(init_msg);
        lsp_state = LSP_INIT_SENT;
    }
}

/* Drain all pending messages from the server. Called once per frame so the
 * editor stays responsive without blocking on the LSP pipe. */
void lspPoll(void) {
    if (!E.lsp || !E.lsp->active) return;
    char *msg;
    while ((msg = lspRead(E.lsp)) != NULL) {
        handle_message(msg);
        free(msg);
    }
}

void lspNotifyOpen(int idx) {
    if (!lspIsReady()) return;
    send_did_open(idx);
}

/* Send didChange with the full buffer text followed by didSave.
 * The didChange ensures the server has the latest content even if
 * the user edits without explicit sync between saves. */
void lspNotifySave(int idx) {
    if (!lspIsReady()) return;
    Buffer *b = &E.buffers[idx];
    if (!b->filename) return;

    char *uri = lspBuildFileUri(b->filename);
    if (!uri) return;

    char *text = buffer_to_string(b);
    if (text) {
        b->lsp_version++;
        char *change_msg = lspBuildDidChange(uri, b->lsp_version, text);
        free(text);
        if (change_msg) { lspSend(E.lsp, change_msg); free(change_msg); }
    }

    char *save_msg = lspBuildDidSave(uri);
    free(uri);
    if (save_msg) { lspSend(E.lsp, save_msg); free(save_msg); }
}

void lspNotifyClose(int idx) {
    if (!lspIsReady()) return;
    Buffer *b = &E.buffers[idx];
    if (!b->filename) return;

    char *uri = lspBuildFileUri(b->filename);
    if (!uri) return;
    char *msg = lspBuildDidClose(uri);
    free(uri);
    if (msg) { lspSend(E.lsp, msg); free(msg); }
}
